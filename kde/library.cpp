#include "library.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QVariant>

namespace {

void skipWs(const QString& s, int* i) {
    while (*i < s.size()) {
        const QChar c = s[*i];
        if (c == QLatin1Char('/') && *i + 1 < s.size() && s[*i + 1] == QLatin1Char('/')) {
            while (*i < s.size() && s[*i] != QLatin1Char('\n'))
                ++*i;
            continue;
        }
        if (c.isSpace()) {
            ++*i;
            continue;
        }
        break;
    }
}

QString parseString(const QString& s, int* i) {
    skipWs(s, i);
    if (*i >= s.size() || s[*i] != QLatin1Char('"'))
        return QString();
    ++*i;
    QString out;
    while (*i < s.size()) {
        const QChar c = s[*i];
        ++*i;
        if (c == QLatin1Char('\\') && *i < s.size()) {
            out += s[*i];
            ++*i;
            continue;
        }
        if (c == QLatin1Char('"'))
            return out;
        out += c;
    }
    return QString();
}

bool parseMap(const QString& s, int* i, QStringList* paths);

bool parseValue(const QString& s, int* i, const QString& key, QStringList* paths) {
    skipWs(s, i);
    if (*i >= s.size())
        return false;
    if (s[*i] == QLatin1Char('{')) {
        ++*i;
        return parseMap(s, i, paths);
    }
    const QString val = parseString(s, i);
    if (val.isNull())
        return false;
    if (key == QLatin1String("path") || key == QLatin1String("Path"))
        paths->append(val);
    bool ok = false;
    key.toInt(&ok);
    if (ok && val.startsWith(QLatin1Char('/')))
        paths->append(val);
    return true;
}

bool parseMap(const QString& s, int* i, QStringList* paths) {
    while (true) {
        skipWs(s, i);
        if (*i >= s.size())
            return false;
        if (s[*i] == QLatin1Char('}')) {
            ++*i;
            return true;
        }
        const QString key = parseString(s, i);
        if (key.isNull() && ( *i >= s.size() || s[*i] != QLatin1Char('}')))
            return false;
        if (key.isNull())
            continue;
        if (!parseValue(s, i, key, paths))
            return false;
    }
}

QStringList pathsInVdf(const QString& file) {
    QStringList paths;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return paths;
    const QString text = QString::fromUtf8(f.readAll());
    int i = 0;
    skipWs(text, &i);
    if (i < text.size() && text[i] == QLatin1Char('"')) {
        parseString(text, &i);
        skipWs(text, &i);
    }
    if (i < text.size() && text[i] == QLatin1Char('{')) {
        ++i;
        parseMap(text, &i, &paths);
    }
    return paths;
}

QString workshopFromLibrary(const QString& path) {
    const QString clean = QDir::cleanPath(path);
    if (clean.endsWith(QLatin1String("/workshop/content/431960")))
        return clean;
    return QDir(clean).filePath(QStringLiteral("steamapps/workshop/content/431960"));
}

QStringList defaultVdfFiles() {
    const QString home = QDir::homePath();
    return {
        home + QStringLiteral("/.steam/steam/steamapps/libraryfolders.vdf"),
        home + QStringLiteral("/.steam/root/steamapps/libraryfolders.vdf"),
        home + QStringLiteral("/.local/share/Steam/steamapps/libraryfolders.vdf"),
        home + QStringLiteral("/.local/share/Steam/config/libraryfolders.vdf")
    };
}

QStringList workshopRoots(const QStringList& only, const QString& vdfPath) {
    QStringList roots;
    if (!only.isEmpty() || !vdfPath.isEmpty()) {
        roots = only;
        if (!vdfPath.isEmpty()) {
            for (const QString& path : pathsInVdf(vdfPath))
                roots << workshopFromLibrary(path);
        }
    } else {
        roots << QDir::homePath() + QStringLiteral("/.local/share/Steam/steamapps/workshop/content/431960");
        const QByteArray env = qgetenv("BENDWALL_WORKSHOP");
        if (!env.isEmpty())
            roots << QString::fromLocal8Bit(env);
        for (const QString& vdf : defaultVdfFiles()) {
            if (!QFile::exists(vdf))
                continue;
            for (const QString& path : pathsInVdf(vdf))
                roots << workshopFromLibrary(path);
        }
    }
    QStringList out;
    QSet<QString> seen;
    for (QString root : roots) {
        root = QDir::cleanPath(root);
        if (root.isEmpty() || seen.contains(root))
            continue;
        seen.insert(root);
        if (QDir(root).exists())
            out << root;
    }
    return out;
}

QString cacheDir() {
    const QByteArray env = qgetenv("BENDWALL_PICKER_CACHE");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!base.isEmpty())
        return base + QStringLiteral("/thumbs");
    return QDir::homePath() + QStringLiteral("/.cache/bendwall-picker/thumbs");
}

QString thumbFor(const QString& preview, const QString& cache) {
    if (preview.isEmpty() || !QFile::exists(preview))
        return QString();
    const QFileInfo info(preview);
    const QString key = QString::number(qHash(preview), 16) + QLatin1Char('_')
        + QString::number(info.size()) + QLatin1Char('_')
        + QString::number(info.lastModified().toSecsSinceEpoch());
    const QString out = cache + QLatin1Char('/') + key + QStringLiteral(".png");
    if (QFile::exists(out))
        return out;
    QImageReader reader(preview);
    reader.setAutoTransform(true);
    if (reader.canRead())
        reader.jumpToImage(0);
    const QSize size = reader.size();
    if (size.width() > 480 && size.height() > 0)
        reader.setScaledSize(QSize(480, qMax(1, 480 * size.height() / size.width())));
    const QImage img = reader.read();
    if (img.isNull())
        return preview;
    QDir().mkpath(cache);
    if (!img.save(out, "PNG"))
        return preview;
    return out;
}

QString existingFile(const QString& dir, const QStringList& names) {
    for (const QString& name : names) {
        const QString path = QDir(dir).filePath(name);
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

Paper readPaper(const QString& dir, const QString& cache) {
    Paper paper;
    paper.dir = QDir(dir).absolutePath();
    paper.id = QFileInfo(paper.dir).fileName();
    paper.title = paper.id;
    paper.type = QStringLiteral("scene");
    QFile file(paper.dir + QStringLiteral("/project.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        paper.liveOk = false;
        paper.note = QStringLiteral("Could not read project.json.");
        return paper;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty() && file.size() > 2) {
        paper.liveOk = false;
        paper.note = QStringLiteral("project.json is not valid JSON.");
        return paper;
    }
    const QString type = root.value(QStringLiteral("type")).toString().trimmed().toLower();
    if (!type.isEmpty())
        paper.type = type;
    const QString title = root.value(QStringLiteral("title")).toString().trimmed();
    if (!title.isEmpty())
        paper.title = title;
    const QJsonValue tags = root.value(QStringLiteral("tags"));
    if (tags.isArray()) {
        for (const QJsonValue& tag : tags.toArray()) {
            const QString text = tag.toString().trimmed();
            if (!text.isEmpty())
                paper.tags << text;
        }
    } else if (tags.isString()) {
        for (const QString& tag : tags.toString().split(QLatin1Char(','))) {
            const QString text = tag.trimmed();
            if (!text.isEmpty())
                paper.tags << text;
        }
    }
    const QString named = root.value(QStringLiteral("preview")).toString().trimmed();
    if (!named.isEmpty()) {
        const QString path = QDir::isAbsolutePath(named) ? named : QDir(paper.dir).filePath(named);
        if (QFile::exists(path))
            paper.preview = path;
    }
    if (paper.preview.isEmpty()) {
        paper.preview = existingFile(paper.dir, {
            QStringLiteral("preview.gif"),
            QStringLiteral("preview.jpg"),
            QStringLiteral("preview.jpeg"),
            QStringLiteral("preview.png")
        });
    }
    paper.gif = paper.preview.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive);
    paper.thumb = thumbFor(paper.preview, cache);

    const QString rel = root.value(QStringLiteral("file")).toString().trimmed();
    const QString payload = rel.isEmpty()
        ? QString()
        : (QDir::isAbsolutePath(rel) ? rel : QDir(paper.dir).filePath(rel));
    auto hasPayload = [&]() { return !payload.isEmpty() && QFile::exists(payload); };

    if (paper.type == QLatin1String("application")) {
        paper.liveOk = false;
        paper.note = QStringLiteral("Unsupported. Bendwall does not run an application wallpaper.");
    } else if (paper.type == QLatin1String("video")) {
        if (!hasPayload()) {
            paper.liveOk = false;
            paper.note = QStringLiteral("Unsupported. The video file is missing.");
        } else {
            paper.liveOk = true;
        }
    } else if (paper.type == QLatin1String("web")) {
        if (!hasPayload()) {
            paper.liveOk = false;
            paper.note = QStringLiteral("Unsupported. The web page is missing.");
        } else {
            paper.liveOk = true;
        }
    } else if (paper.type == QLatin1String("preset")) {
        paper.liveOk = false;
        paper.note = QStringLiteral("Preset. This follows another item. The card does not invent that item's picture.");
    } else if (paper.type == QLatin1String("scene")) {
        const bool scene = hasPayload()
            || QFile::exists(paper.dir + QStringLiteral("/scene.json"))
            || QFile::exists(paper.dir + QStringLiteral("/scene.pkg"))
            || QFile::exists(paper.dir + QStringLiteral("/gifscene.pkg"));
        if (!scene) {
            paper.liveOk = false;
            paper.note = QStringLiteral("Unsupported. The scene file is missing.");
        } else {
            paper.liveOk = true;
        }
    } else {
        paper.liveOk = false;
        paper.note = QStringLiteral("Unsupported type %1.").arg(paper.type);
    }
    return paper;
}

class Scanner : public QObject {
    Q_OBJECT

public:
    Scanner(QStringList roots, QString cache) : m_roots(std::move(roots)), m_cache(std::move(cache)) {}

public slots:
    void run() {
        QStringList missing;
        int n = 0;
        for (const QString& root : m_roots) {
            QDir dir(root);
            if (!dir.exists()) {
                missing << root;
                continue;
            }
            const QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString& name : names) {
                const QString child = dir.filePath(name);
                if (!QFile::exists(child + QStringLiteral("/project.json")))
                    continue;
                const Paper paper = readPaper(child, m_cache);
                QVariantMap map;
                map.insert(QStringLiteral("id"), paper.id);
                map.insert(QStringLiteral("title"), paper.title);
                map.insert(QStringLiteral("type"), paper.type);
                map.insert(QStringLiteral("dir"), paper.dir);
                map.insert(QStringLiteral("preview"), paper.preview);
                map.insert(QStringLiteral("thumb"), paper.thumb);
                map.insert(QStringLiteral("note"), paper.note);
                map.insert(QStringLiteral("tags"), paper.tags);
                map.insert(QStringLiteral("gif"), paper.gif);
                map.insert(QStringLiteral("liveOk"), paper.liveOk);
                emit itemReady(map);
                ++n;
            }
        }
        QString status = QStringLiteral("%1 wallpapers").arg(n);
        if (!missing.isEmpty())
            status += QStringLiteral(". Could not read ") + missing.join(QStringLiteral(", "));
        emit done(status);
    }

signals:
    void itemReady(const QVariantMap& item);
    void done(const QString& status);

private:
    QStringList m_roots;
    QString m_cache;
};

}  // namespace

Library::Library(QObject* parent) : QAbstractListModel(parent) {}

int Library::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return m_shown.size();
}

QVariant Library::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shown.size())
        return QVariant();
    const Paper& paper = m_all.at(m_shown.at(index.row()));
    switch (role) {
    case IdRole: return paper.id;
    case TitleRole: return paper.title;
    case TypeRole: return paper.type;
    case DirRole: return paper.dir;
    case PreviewRole: return paper.preview;
    case ThumbRole: return paper.thumb.isEmpty() ? paper.preview : paper.thumb;
    case NoteRole: return paper.note;
    case TagsRole: return paper.tags.join(QStringLiteral(", "));
    case GifRole: return paper.gif;
    case LiveRole: return paper.liveOk;
    case ActiveRole: return m_active.value(paper.id);
    default: return QVariant();
    }
}

QHash<int, QByteArray> Library::roleNames() const {
    return {
        {IdRole, "itemId"},
        {TitleRole, "title"},
        {TypeRole, "kind"},
        {DirRole, "dir"},
        {PreviewRole, "preview"},
        {ThumbRole, "thumb"},
        {NoteRole, "note"},
        {TagsRole, "tags"},
        {GifRole, "gif"},
        {LiveRole, "liveOk"},
        {ActiveRole, "activeText"}
    };
}

void Library::setQuery(const QString& query) {
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
    rebuildShown();
}

void Library::setTypeFilter(const QString& type) {
    if (m_type == type)
        return;
    m_type = type;
    emit typeFilterChanged();
    rebuildShown();
}

void Library::setTagFilter(const QString& tag) {
    if (m_tag == tag)
        return;
    m_tag = tag;
    emit tagFilterChanged();
    rebuildShown();
}

void Library::setActiveText(const QHash<QString, QString>& byId) {
    if (m_active == byId)
        return;
    m_active = byId;
    if (!m_shown.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_shown.size() - 1, 0), {ActiveRole});
    }
}

void Library::setRoots(const QStringList& onlyWorkshop, const QString& vdfPath) {
    m_only = onlyWorkshop;
    m_vdf = vdfPath;
}

void Library::start() {
    if (m_started)
        return;
    m_started = true;
    m_scanning = true;
    m_status = QStringLiteral("Reading workshop folders…");
    emit scanningChanged();
    emit statusLineChanged();
    const QString cache = cacheDir();
    QDir().mkpath(cache);
    const QStringList roots = workshopRoots(m_only, m_vdf);
    auto* thread = new QThread(this);
    auto* scanner = new Scanner(roots, cache);
    scanner->moveToThread(thread);
    connect(thread, &QThread::started, scanner, &Scanner::run);
    connect(scanner, &Scanner::itemReady, this, &Library::addItem);
    connect(scanner, &Scanner::done, this, &Library::finishScan);
    connect(scanner, &Scanner::done, thread, &QThread::quit);
    connect(thread, &QThread::finished, scanner, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString Library::idAt(int row) const {
    if (row < 0 || row >= m_shown.size())
        return QString();
    return m_all.at(m_shown.at(row)).id;
}

QVariantMap Library::paper(const QString& id) const {
    QVariantMap map;
    for (int row = 0; row < m_shown.size(); ++row) {
        const Paper& paper = m_all.at(m_shown.at(row));
        if (paper.id != id)
            continue;
        map.insert(QStringLiteral("id"), paper.id);
        map.insert(QStringLiteral("title"), paper.title);
        map.insert(QStringLiteral("type"), paper.type);
        map.insert(QStringLiteral("dir"), paper.dir);
        map.insert(QStringLiteral("preview"), paper.preview);
        map.insert(QStringLiteral("thumb"), paper.thumb.isEmpty() ? paper.preview : paper.thumb);
        map.insert(QStringLiteral("note"), paper.note);
        map.insert(QStringLiteral("gif"), paper.gif);
        map.insert(QStringLiteral("liveOk"), paper.liveOk);
        return map;
    }
    return map;
}

bool Library::liveOkAt(int row) const {
    if (row < 0 || row >= m_shown.size())
        return false;
    return m_all.at(m_shown.at(row)).liveOk;
}

QString Library::noteAt(int row) const {
    if (row < 0 || row >= m_shown.size())
        return QString();
    return m_all.at(m_shown.at(row)).note;
}

void Library::addItem(const QVariantMap& item) {
    Paper paper;
    paper.id = item.value(QStringLiteral("id")).toString();
    paper.title = item.value(QStringLiteral("title")).toString();
    paper.type = item.value(QStringLiteral("type")).toString();
    paper.dir = item.value(QStringLiteral("dir")).toString();
    paper.preview = item.value(QStringLiteral("preview")).toString();
    paper.thumb = item.value(QStringLiteral("thumb")).toString();
    paper.note = item.value(QStringLiteral("note")).toString();
    paper.tags = item.value(QStringLiteral("tags")).toStringList();
    paper.gif = item.value(QStringLiteral("gif")).toBool();
    paper.liveOk = item.value(QStringLiteral("liveOk")).toBool();
    for (const Paper& have : m_all) {
        if (have.dir == paper.dir)
            return;
    }
    m_all.append(paper);
    emit totalChanged();
    recomputeTags();
    if (!passes(paper))
        return;
    const int row = m_shown.size();
    beginInsertRows(QModelIndex(), row, row);
    m_shown.append(m_all.size() - 1);
    endInsertRows();
    emit shownChanged();
}

void Library::finishScan(const QString& status) {
    m_scanning = false;
    m_status = status;
    emit scanningChanged();
    emit statusLineChanged();
    emit scanFinished();
}

bool Library::passes(const Paper& paper) const {
    if (!m_type.isEmpty()) {
        const bool main = paper.type == QLatin1String("scene")
            || paper.type == QLatin1String("video")
            || paper.type == QLatin1String("web");
        if (m_type == QLatin1String("other")) {
            if (main)
                return false;
        } else if (paper.type != m_type) {
            return false;
        }
    }
    if (!m_tag.isEmpty() && !paper.tags.contains(m_tag))
        return false;
    const QString q = m_query.trimmed();
    if (q.isEmpty())
        return true;
    if (paper.title.contains(q, Qt::CaseInsensitive))
        return true;
    if (paper.id.contains(q, Qt::CaseInsensitive))
        return true;
    if (paper.type.contains(q, Qt::CaseInsensitive))
        return true;
    for (const QString& tag : paper.tags) {
        if (tag.contains(q, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void Library::rebuildShown() {
    beginResetModel();
    m_shown.clear();
    for (int i = 0; i < m_all.size(); ++i) {
        if (passes(m_all.at(i)))
            m_shown.append(i);
    }
    endResetModel();
    emit shownChanged();
}

void Library::recomputeTags() {
    QStringList tags;
    QSet<QString> seen;
    for (const Paper& paper : m_all) {
        for (const QString& tag : paper.tags) {
            if (seen.contains(tag))
                continue;
            seen.insert(tag);
            tags << tag;
        }
    }
    tags.sort(Qt::CaseInsensitive);
    if (tags == m_tags)
        return;
    m_tags = tags;
    emit tagsChanged();
}

#include "library.moc"
