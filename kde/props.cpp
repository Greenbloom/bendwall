#include "props.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

static QString showVal(const QJsonValue& v) {
    if (v.isBool()) return v.toBool() ? "true" : "false";
    if (v.isDouble()) return QString::number(v.toDouble(), 'g', 8);
    if (v.isString()) return v.toString();
    return QString();
}

QStringList propertyLines(const QString& path) {
    QStringList out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray props = root.value("general").toObject().value("properties").toArray();
    for (const QJsonValue& item : props) {
        const QJsonObject o = item.toObject();
        const QString name = o.value("name").toString();
        const QString type = o.value("type").toString();
        QString extra;
        if (type == "slider") {
            if (o.contains("min")) extra += "min=" + showVal(o.value("min"));
            if (o.contains("max")) {
                if (!extra.isEmpty()) extra += " ";
                extra += "max=" + showVal(o.value("max"));
            }
        } else if (type == "combo") {
            QStringList opts;
            for (const QJsonValue& op : o.value("options").toArray())
                opts << op.toObject().value("value").toString();
            extra = "options=" + opts.join("|");
        } else if (o.contains("text")) {
            extra = "text=" + o.value("text").toString();
        }
        out << (name + "\t" + type + "\t" + showVal(o.value("value")) + "\t" + extra);
    }
    return out;
}

QString workshopRoot() {
    const QByteArray env = qgetenv("BENDWALL_WORKSHOP");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
    return QDir::homePath() + "/.local/share/Steam/steamapps/workshop/content/431960";
}

QString itemDirectory(const QString& id) {
    if (id.isEmpty()) return QString();
    if (id.startsWith(QLatin1Char('/'))) return id;
    return workshopRoot() + QLatin1Char('/') + id;
}

QStringList workshopIds(const QString& root) {
    QStringList ids;
    const QDir dir(root);
    const QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : names) {
        if (QFile::exists(dir.filePath(name + "/project.json"))) ids << name;
    }
    return ids;
}
