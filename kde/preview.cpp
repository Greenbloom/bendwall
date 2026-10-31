#include "props.h"

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QRect>
#include <QUrl>
#include <QVariant>

#include <iostream>
#include <string>

static int dumpProps(const QString& path) {
    const QStringList lines = propertyLines(path);
    if (lines.isEmpty()) return 1;
    for (const QString& line : lines)
        std::cout << line.toStdString() << "\n";
    return 0;
}

static int dumpWorkshop(const QString& root) {
    const QStringList ids = workshopIds(root);
    if (ids.isEmpty()) return 1;
    for (const QString& id : ids)
        std::cout << "ITEM " << id.toStdString() << "\n";
    return 0;
}

/* Offscreen Qt Quick on this host does not rasterize the scene graph, so the
   picture is the same frame and the same property lines the QML page binds,
   drawn with QPainter. Loading the QML still has to succeed. */
static QImage renderSheet(const QString& frame, const QString& project, const QString& workshop) {
    QImage img(520, 340, QImage::Format_RGB32);
    img.fill(QColor(18, 20, 24));
    QPainter painter(&img);
    painter.drawImage(QRect(0, 0, 240, 340), QImage(frame));
    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    painter.setPen(QColor(232, 230, 227));
    int y = 28;
    painter.drawText(QRect(256, y, 248, 20), "Workshop");
    y += 24;
    painter.setPen(QColor(200, 196, 188));
    for (const QString& id : workshopIds(workshop)) {
        painter.drawText(QRect(256, y, 248, 18), id);
        y += 16;
    }
    y += 8;
    painter.setPen(QColor(232, 230, 227));
    painter.drawText(QRect(256, y, 248, 20), "Properties");
    y += 24;
    font.setPixelSize(11);
    painter.setFont(font);
    painter.setPen(QColor(200, 196, 188));
    for (const QString& line : propertyLines(project)) {
        painter.drawText(QRect(256, y, 248, 16), line);
        y += 16;
    }
    return img;
}

static int grab(const QString& qml, const QString& frame, const QString& project,
                const QString& workshop, const QString& out, const QString& imports) {
    QQmlApplicationEngine engine;
    QList<QQmlError> errs;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [&errs](const QList<QQmlError>& w) { errs += w; });
    if (!imports.isEmpty()) engine.addImportPath(imports);
    engine.rootContext()->setContextProperty("wallpaperFrame", frame);
    engine.rootContext()->setContextProperty("propLines", propertyLines(project));
    engine.rootContext()->setContextProperty("workshopIds", workshopIds(workshop));
    engine.load(QUrl::fromLocalFile(qml));
    if (!errs.isEmpty() || engine.rootObjects().isEmpty()) {
        for (const QQmlError& err : errs)
            std::cerr << err.toString().toStdString() << "\n";
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) return 1;
    const QImage img = renderSheet(frame, project, workshop);
    const bool wrote = img.save(out);
    std::cout << "SHEET " << img.width() << "x" << img.height()
              << " wrote=" << (wrote ? 1 : 0) << "\n";
    return wrote ? 0 : 1;
}

int main(int argc, char** argv) {
    const std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "--dump-props" && argc >= 3) {
        QCoreApplication app(argc, argv);
        return dumpProps(QString::fromLocal8Bit(argv[2]));
    }
    if (cmd == "--dump-workshop" && argc >= 3) {
        QCoreApplication app(argc, argv);
        return dumpWorkshop(QString::fromLocal8Bit(argv[2]));
    }
    if (cmd == "--grab" && argc >= 8) {
        QGuiApplication app(argc, argv);
        return grab(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]),
                    QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[5]),
                    QString::fromLocal8Bit(argv[6]), QString::fromLocal8Bit(argv[7]));
    }
    std::cerr << "usage:\n"
              << "  bendwall-preview --dump-props <project.json>\n"
              << "  bendwall-preview --dump-workshop <workshop-root>\n"
              << "  bendwall-preview --grab <qml> <frame.png> <project.json> <workshop> <out.png> <qml-import-path>\n";
    return 2;
}
