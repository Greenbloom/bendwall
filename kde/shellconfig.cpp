#include "shellconfig.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QHash>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class ScriptDesk : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screen READ screen CONSTANT)
    Q_PROPERTY(QString wallpaperPlugin READ wallpaperPlugin WRITE setWallpaperPlugin)
    Q_PROPERTY(QVariantList currentConfigGroup READ currentConfigGroup WRITE setCurrentConfigGroup)

public:
    int m_screen = 0;
    QString plugin;
    QHash<QString, QString> cfg;
    QVariantList group;

    int screen() const { return m_screen; }
    QString wallpaperPlugin() const { return plugin; }
    void setWallpaperPlugin(const QString& value) { plugin = value; }
    QVariantList currentConfigGroup() const { return group; }
    void setCurrentConfigGroup(const QVariantList& value) { group = value; }

    Q_INVOKABLE QString readConfig(const QString& key) const { return cfg.value(key); }

    Q_INVOKABLE void writeConfig(const QString& key, const QJSValue& value) {
        if (value.isBool())
            cfg.insert(key, value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        else if (value.isNumber())
            cfg.insert(key, QString::number(value.toInt()));
        else
            cfg.insert(key, value.toString());
    }
};

class ScriptHost : public QObject {
    Q_OBJECT

public:
    QJSEngine* engine = nullptr;
    QList<ScriptDesk*>* desks = nullptr;
    QString printed;

    Q_INVOKABLE void print(const QJSValue& value) {
        if (!printed.isEmpty())
            printed += QLatin1Char('\n');
        printed += value.toString();
    }

    Q_INVOKABLE QJSValue desktops() {
        if (!engine || !desks)
            return QJSValue(QJSValue::NullValue);
        QJSValue array = engine->newArray(desks->size());
        for (int i = 0; i < desks->size(); ++i) {
            ScriptDesk* desk = desks->at(i);
            QJSEngine::setObjectOwnership(desk, QJSEngine::CppOwnership);
            array.setProperty(i, engine->newQObject(desk));
        }
        return array;
    }
};

namespace {

const char* kPlugin = "com.philip.bendwall";

QString jsQuote(const QString& text) {
    QString out;
    out.reserve(text.size() + 2);
    out += QLatin1Char('"');
    for (const QChar c : text) {
        if (c == QLatin1Char('\\') || c == QLatin1Char('"')) {
            out += QLatin1Char('\\');
            out += c;
        } else if (c == QLatin1Char('\n')) {
            out += QLatin1String("\\n");
        } else {
            out += c;
        }
    }
    out += QLatin1Char('"');
    return out;
}

bool parseBool(const QString& text, bool fallback) {
    const QString v = text.trimmed().toLower();
    if (v == QLatin1String("true") || v == QLatin1String("1"))
        return true;
    if (v == QLatin1String("false") || v == QLatin1String("0"))
        return false;
    return fallback;
}

int parseVolume(const QString& text) {
    bool ok = false;
    int n = text.trimmed().toInt(&ok);
    if (!ok)
        n = 100;
    if (n < 0)
        n = 0;
    if (n > 100)
        n = 100;
    return n;
}

QString readScript() {
    return QStringLiteral(
        "var all = desktops();\n"
        "var parts = [];\n"
        "for (var i = 0; i < all.length; i++) {\n"
        "  var d = all[i];\n"
        "  var plugin = d.wallpaperPlugin;\n"
        "  d.currentConfigGroup = ['Wallpaper', 'com.philip.bendwall', 'General'];\n"
        "  var item = '';\n"
        "  var mute = 'true';\n"
        "  var vol = '100';\n"
        "  var autoMute = 'false';\n"
        "  try {\n"
        "    var got = d.readConfig('Item');\n"
        "    if (got !== undefined && got !== null) item = String(got);\n"
        "    got = d.readConfig('VideoMute');\n"
        "    if (got !== undefined && got !== null && String(got) !== '') mute = String(got);\n"
        "    got = d.readConfig('VideoVolume');\n"
        "    if (got !== undefined && got !== null && String(got) !== '') vol = String(got);\n"
        "    got = d.readConfig('AutoMuteOthers');\n"
        "    if (got !== undefined && got !== null && String(got) !== '') autoMute = String(got);\n"
        "  } catch (e) {}\n"
        "  parts.push('{\"screen\":' + d.screen\n"
        "    + ',\"plugin\":' + JSON.stringify(String(plugin))\n"
        "    + ',\"item\":' + JSON.stringify(item)\n"
        "    + ',\"mute\":' + JSON.stringify(mute)\n"
        "    + ',\"volume\":' + JSON.stringify(vol)\n"
        "    + ',\"autoMute\":' + JSON.stringify(autoMute) + '}');\n"
        "}\n"
        "print('[' + parts.join(',') + ']');\n");
}

QString writeScript(const QString& id, int screen, bool mute, int volume, bool autoMute) {
    return QStringLiteral(
               "var all = desktops();\n"
               "var want = %1;\n"
               "var n = 0;\n"
               "for (var i = 0; i < all.length; i++) {\n"
               "  var d = all[i];\n"
               "  if (want >= 0 && d.screen !== want) continue;\n"
               "  d.wallpaperPlugin = 'com.philip.bendwall';\n"
               "  d.currentConfigGroup = ['Wallpaper', 'com.philip.bendwall', 'General'];\n"
               "  d.writeConfig('Item', %2);\n"
               "  d.writeConfig('VideoMute', %3);\n"
               "  d.writeConfig('VideoVolume', '%4');\n"
               "  d.writeConfig('AutoMuteOthers', %5);\n"
               "  n++;\n"
               "}\n"
               "print(String(n));\n")
        .arg(screen)
        .arg(jsQuote(id))
        .arg(mute ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(volume)
        .arg(autoMute ? QStringLiteral("true") : QStringLiteral("false"));
}

QList<ScreenAssign> fromJson(const QByteArray& bytes, QString* error) {
    QList<ScreenAssign> out;
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("Screen list is not valid JSON.");
        return out;
    }
    const QJsonArray screens = doc.object().value(QStringLiteral("screens")).toArray();
    for (const QJsonValue& value : screens) {
        const QJsonObject obj = value.toObject();
        ScreenAssign screen;
        screen.screen = obj.value(QStringLiteral("screen")).toInt();
        screen.plugin = obj.value(QStringLiteral("plugin")).toString();
        screen.item = obj.value(QStringLiteral("item")).toString();
        screen.mute = parseBool(obj.value(QStringLiteral("mute")).toVariant().toString(), true);
        if (obj.value(QStringLiteral("mute")).isBool())
            screen.mute = obj.value(QStringLiteral("mute")).toBool();
        screen.volume = parseVolume(obj.value(QStringLiteral("volume")).toVariant().toString());
        screen.autoMute = parseBool(obj.value(QStringLiteral("autoMute")).toVariant().toString(), false);
        if (obj.value(QStringLiteral("autoMute")).isBool())
            screen.autoMute = obj.value(QStringLiteral("autoMute")).toBool();
        out.append(screen);
    }
    return out;
}

struct DeskState {
    int screen = 0;
    QString plugin;
    QHash<QString, QString> cfg;
};

QList<DeskState> desksFromAssign(const QList<ScreenAssign>& screens) {
    QList<DeskState> desks;
    for (const ScreenAssign& screen : screens) {
        DeskState desk;
        desk.screen = screen.screen;
        desk.plugin = screen.plugin;
        desk.cfg.insert(QStringLiteral("Item"), screen.item);
        desk.cfg.insert(QStringLiteral("VideoMute"),
                        screen.mute ? QStringLiteral("true") : QStringLiteral("false"));
        desk.cfg.insert(QStringLiteral("VideoVolume"), QString::number(screen.volume));
        desk.cfg.insert(QStringLiteral("AutoMuteOthers"),
                        screen.autoMute ? QStringLiteral("true") : QStringLiteral("false"));
        desks.append(desk);
    }
    return desks;
}

QList<ScreenAssign> assignFromDesks(const QList<DeskState>& desks) {
    QList<ScreenAssign> screens;
    for (const DeskState& desk : desks) {
        ScreenAssign screen;
        screen.screen = desk.screen;
        screen.plugin = desk.plugin;
        screen.item = desk.cfg.value(QStringLiteral("Item"));
        screen.mute = parseBool(desk.cfg.value(QStringLiteral("VideoMute")), true);
        screen.volume = parseVolume(desk.cfg.value(QStringLiteral("VideoVolume")));
        screen.autoMute = parseBool(desk.cfg.value(QStringLiteral("AutoMuteOthers")), false);
        screens.append(screen);
    }
    return screens;
}

/* Plasma's evaluateScript returns only what the script printed. The
   completion value is discarded, including by this mock. */
bool evalPrinted(const QString& script, QList<DeskState>* desks, QString* printed) {
    QList<ScriptDesk*> live;
    ScriptHost host;
    host.desks = &live;
    QString text;
    bool ok = false;
    for (const DeskState& state : *desks) {
        auto* desk = new ScriptDesk();
        desk->m_screen = state.screen;
        desk->plugin = state.plugin;
        desk->cfg = state.cfg;
        QJSEngine::setObjectOwnership(desk, QJSEngine::CppOwnership);
        live.append(desk);
    }
    {
        QJSEngine js;
        host.engine = &js;
        QJSEngine::setObjectOwnership(&host, QJSEngine::CppOwnership);
        js.globalObject().setProperty(QStringLiteral("__host"), js.newQObject(&host));
        const QJSValue setup = js.evaluate(QStringLiteral(
            "function print(v){ __host.print(v); }\n"
            "function desktops(){ return __host.desktops(); }\n"));
        if (!setup.isError()) {
            const QJSValue result = js.evaluate(script);
            ok = !result.isError();
            text = host.printed;
        }
        host.engine = nullptr;
    }
    desks->clear();
    for (ScriptDesk* desk : live) {
        DeskState state;
        state.screen = desk->m_screen;
        state.plugin = desk->plugin;
        state.cfg = desk->cfg;
        desks->append(state);
        delete desk;
    }
    live.clear();
    host.desks = nullptr;
    if (printed)
        *printed = text.trimmed();
    return ok;
}

QString unquotePrinted(QString body) {
    body = body.trimmed();
    if (body.startsWith(QLatin1Char('"')) && body.endsWith(QLatin1Char('"')) && body.size() >= 2)
        body = body.mid(1, body.size() - 2);
    return body.trimmed();
}

QByteArray toJson(const QList<ScreenAssign>& screens) {
    QJsonArray array;
    for (const ScreenAssign& screen : screens) {
        QJsonObject obj;
        obj.insert(QStringLiteral("screen"), screen.screen);
        obj.insert(QStringLiteral("plugin"), screen.plugin);
        obj.insert(QStringLiteral("item"), screen.item);
        obj.insert(QStringLiteral("mute"), screen.mute ? QStringLiteral("true") : QStringLiteral("false"));
        obj.insert(QStringLiteral("volume"), QString::number(screen.volume));
        obj.insert(QStringLiteral("autoMute"), screen.autoMute ? QStringLiteral("true") : QStringLiteral("false"));
        array.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("screens"), array);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

}  // namespace

ShellConfig::ShellConfig(QObject* parent) : QObject(parent) {}

QStringList ShellConfig::screenChoices() const {
    QStringList choices;
    choices << QStringLiteral("All screens");
    for (const ScreenAssign& screen : m_screens)
        choices << QStringLiteral("Screen %1").arg(screen.screen);
    return choices;
}

void ShellConfig::setMute(bool on) {
    if (m_mute == on)
        return;
    m_mute = on;
    emit muteChanged();
}

void ShellConfig::setVolume(int volume) {
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;
    if (m_volume == volume)
        return;
    m_volume = volume;
    emit volumeChanged();
}

void ShellConfig::setAutoMute(bool on) {
    if (m_auto == on)
        return;
    m_auto = on;
    emit autoMuteChanged();
}

void ShellConfig::setMock(const QString& path) { m_mock = path; }

void ShellConfig::setError(const QString& text) {
    if (m_error == text)
        return;
    m_error = text;
    emit errorChanged();
}

void ShellConfig::setStatus(const QString& text) {
    if (m_status == text)
        return;
    m_status = text;
    emit statusChanged();
}

void ShellConfig::replaceScreens(const QList<ScreenAssign>& screens) {
    m_screens = screens;
    emit screensChanged();
}

void ShellConfig::refresh() {
    setError(QString());
    if (!m_mock.isEmpty()) {
        if (!readMock())
            return;
        setStatus(QStringLiteral("%1 screens").arg(m_screens.size()));
        return;
    }
    if (!readDBus())
        return;
    setStatus(QStringLiteral("%1 screens").arg(m_screens.size()));
}

bool loadDesks(const QString& path, QList<DeskState>* desks) {
    desks->clear();
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QString error;
    const QList<ScreenAssign> screens = fromJson(file.readAll(), &error);
    if (!error.isEmpty())
        return false;
    *desks = desksFromAssign(screens);
    return true;
}

bool saveDesks(const QString& path, const QList<DeskState>& desks) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(toJson(assignFromDesks(desks)));
    return true;
}

bool ShellConfig::readMock() {
    QList<DeskState> desks;
    if (!loadDesks(m_mock, &desks)) {
        setError(QStringLiteral("Could not read %1.").arg(m_mock));
        return false;
    }
    QString printed;
    if (!evalPrinted(readScript(), &desks, &printed)) {
        setError(QStringLiteral("Screen list is not valid JSON."));
        return false;
    }
    QString error;
    const QByteArray body = unquotePrinted(printed).toUtf8();
    const QList<ScreenAssign> screens = fromJson(QByteArray("{\"screens\":") + body + "}", &error);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    replaceScreens(screens);
    return true;
}

bool ShellConfig::readDBus() {
    QDBusInterface bus(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                       QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    if (!bus.isValid()) {
        setError(QStringLiteral("Plasmashell is not running, so the current wallpaper per screen is unknown."));
        replaceScreens({});
        return false;
    }
    const QDBusReply<QString> reply = bus.call(QStringLiteral("evaluateScript"), readScript());
    if (!reply.isValid()) {
        setError(reply.error().message().isEmpty()
                     ? QStringLiteral("Plasmashell did not return the screen list.")
                     : reply.error().message());
        return false;
    }
    QString error;
    QByteArray body = reply.value().trimmed().toUtf8();
    if (body.startsWith('"') && body.endsWith('"') && body.size() >= 2)
        body = body.mid(1, body.size() - 2);
    const QList<ScreenAssign> screens = fromJson(QByteArray("{\"screens\":") + body + "}", &error);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    replaceScreens(screens);
    return true;
}

bool ShellConfig::writeDBus(const QString& id, int screen) {
    QDBusInterface bus(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                       QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    if (!bus.isValid()) {
        setError(QStringLiteral("Plasmashell is not running. Apply needs a Plasma session."));
        return false;
    }
    const QDBusReply<QString> reply = bus.call(QStringLiteral("evaluateScript"),
                                               writeScript(id, screen, m_mute, m_volume, m_auto));
    if (!reply.isValid()) {
        setError(reply.error().message().isEmpty()
                     ? QStringLiteral("Plasmashell did not apply the wallpaper.")
                     : reply.error().message());
        return false;
    }
    bool ok = false;
    const int n = unquotePrinted(reply.value()).toInt(&ok);
    if (!ok || n < 1) {
        setError(QStringLiteral("No screen matched that choice."));
        return false;
    }
    return readDBus();
}

bool ShellConfig::apply(const QString& id, int screen) {
    setError(QString());
    if (id.isEmpty()) {
        setError(QStringLiteral("Choose a wallpaper first."));
        return false;
    }
    if (!m_mock.isEmpty()) {
        QList<DeskState> desks;
        if (!loadDesks(m_mock, &desks)) {
            setError(QStringLiteral("Could not read %1.").arg(m_mock));
            return false;
        }
        QString printed;
        if (!evalPrinted(writeScript(id, screen, m_mute, m_volume, m_auto), &desks, &printed)) {
            setError(QStringLiteral("No screen matched that choice."));
            return false;
        }
        bool ok = false;
        const int n = unquotePrinted(printed).toInt(&ok);
        if (!ok || n < 1) {
            setError(QStringLiteral("No screen matched that choice."));
            return false;
        }
        if (!saveDesks(m_mock, desks)) {
            setError(QStringLiteral("Could not write %1.").arg(m_mock));
            return false;
        }
        if (!readMock())
            return false;
        setStatus(QStringLiteral("Applied %1.").arg(id));
        return true;
    }
    if (!writeDBus(id, screen))
        return false;
    setStatus(QStringLiteral("Applied %1.").arg(id));
    return true;
}

int ShellConfig::screenChoiceValue(int index) const {
    if (index <= 0)
        return -1;
    const int row = index - 1;
    if (row < 0 || row >= m_screens.size())
        return -1;
    return m_screens.at(row).screen;
}

void ShellConfig::loadEditor(int screen) {
    const ScreenAssign* found = nullptr;
    for (const ScreenAssign& row : m_screens) {
        if (screen >= 0 && row.screen != screen)
            continue;
        if (row.plugin == QLatin1String(kPlugin) || !found)
            found = &row;
        if (screen >= 0)
            break;
    }
    if (!found)
        return;
    setMute(found->mute);
    setVolume(found->volume);
    setAutoMute(found->autoMute);
}

QString ShellConfig::activeText(const QString& id) const {
    QList<int> hits;
    for (const ScreenAssign& row : m_screens) {
        if (row.plugin == QLatin1String(kPlugin) && row.item == id)
            hits.append(row.screen);
    }
    if (hits.isEmpty())
        return QString();
    if (hits.size() == 1)
        return QStringLiteral("Screen %1").arg(hits.at(0));
    QStringList parts;
    for (int n : hits)
        parts << QString::number(n);
    return QStringLiteral("Screens %1").arg(parts.join(QStringLiteral(", ")));
}

QHash<QString, QString> ShellConfig::activeMap() const {
    QHash<QString, QString> map;
    QHash<QString, QList<int>> hits;
    for (const ScreenAssign& row : m_screens) {
        if (row.plugin == QLatin1String(kPlugin) && !row.item.isEmpty())
            hits[row.item].append(row.screen);
    }
    for (auto it = hits.cbegin(); it != hits.cend(); ++it) {
        if (it.value().size() == 1)
            map.insert(it.key(), QStringLiteral("Screen %1").arg(it.value().at(0)));
        else {
            QStringList parts;
            for (int n : it.value())
                parts << QString::number(n);
            map.insert(it.key(), QStringLiteral("Screens %1").arg(parts.join(QStringLiteral(", "))));
        }
    }
    return map;
}

#include "shellconfig.moc"
