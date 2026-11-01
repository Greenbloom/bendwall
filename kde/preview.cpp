#include "engine.h"
#include "props.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPaintEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QRect>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVector>

#include <cstring>

#ifdef BENDWALL_HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

#include <cstdlib>
#include <cstdio>
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

static QVector<float> constantAudio() {
    QVector<float> s(256);
    for (int i = 0; i < 128; i++) {
        s[i * 2] = 1.f;
        s[i * 2 + 1] = 0.f;
    }
    return s;
}

static int frameMs(const QString& line) {
    const int at = line.indexOf(QStringLiteral("step="));
    if (at < 0) return 1000;
    bool ok = false;
    const int ms = line.mid(at + 5).section(' ', 0, 0).toInt(&ok);
    return ok && ms > 0 ? ms : 1000;
}

/* One frame of a 24fps film is often the same pixel at the center.
   A second of playback (or one whole frame, when the frame is longer)
   is enough for the synthetic 1fps clip and for a real shot. */
static int spacedMs(int frameMs) {
    return frameMs > 1000 ? frameMs : 1000;
}

static int pictureDiff(const QImage& a, const QImage& b) {
    if (a.isNull() || b.isNull()) return 1;
    if (a.width() != b.width() || a.height() != b.height()) return 1;
    for (int y = 0; y < a.height(); ++y) {
        if (std::memcmp(a.constScanLine(y), b.constScanLine(y), static_cast<size_t>(a.width()) * 4) != 0)
            return 1;
    }
    return 0;
}

static int playFrames(const QString& dir, int bench) {
    Engine eng;
    eng.setAudio(constantAudio());
    if (!eng.open(dir, 160, 90)) {
        std::cerr << eng.error().toStdString() << "\n";
        return 1;
    }
    eng.step(0);
    const QImage a = eng.image();
    if (a.isNull()) return 1;
    const int span = spacedMs(frameMs(eng.line()));
    if (!eng.step(span)) return 1;
    const QImage b = eng.image();
    const QRgb p0 = a.pixel(a.width() / 2, a.height() / 2);
    const QRgb p1 = b.pixel(b.width() / 2, b.height() / 2);
    const int diff = pictureDiff(a, b);
    std::printf("FRAME 0 %d %d %d %d\n", qRed(p0), qGreen(p0), qBlue(p0), qAlpha(p0));
    std::printf("FRAME 1 %d %d %d %d\n", qRed(p1), qGreen(p1), qBlue(p1), qAlpha(p1));
    std::printf("SPAN %d\n", span);
    std::printf("DIFF %d\n", diff);
    eng.close();
    /* Throughput at a desktop-sized target. The color check above stays small. */
    const int bw = 640, bh = 360;
    const int n = bench > 0 ? bench : 12;
    eng.setAudio(constantAudio());
    if (!eng.open(dir, bw, bh)) return diff ? 0 : 1;
    const long t0 = bendwallProcTicks();
    QElapsedTimer clock;
    clock.start();
    for (int i = 0; i < n; i++) eng.step(0);
    long elapsed = clock.elapsed();
    if (elapsed < 1) elapsed = 1;
    const double fps = n * 1000.0 / (double)elapsed;
    const double cpu = bendwallCpuPercent(t0, bendwallProcTicks(), elapsed);
    const QByteArray fpsText = bendwallFixed1(fps).toLatin1();
    const QByteArray cpuText = bendwallFixed1(cpu).toLatin1();
    const QByteArray line = eng.line().toUtf8();
    std::printf("PLAY fps=%s cpu=%s size=%dx%d %s\n", fpsText.constData(), cpuText.constData(),
                bw, bh, line.constData());
    eng.close();
    return diff ? 0 : 1;
}

static QString topType(const QString& dir) {
    QFile file(QDir(dir).filePath(QStringLiteral("project.json")));
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("scene");
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    const QString type = obj.value(QStringLiteral("type")).toString().trimmed().toLower();
    return type.isEmpty() ? QStringLiteral("scene") : type;
}

static QString watchQmlPath() {
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        app + QStringLiteral("/../../kde/watch.qml"),
        app + QStringLiteral("/watch.qml"),
        QStringLiteral("kde/watch.qml"),
    };
    for (const QString& cand : cands) {
        if (QFile::exists(cand)) return QFileInfo(cand).absoluteFilePath();
    }
    return {};
}

static void addImports(QQmlApplicationEngine* engine) {
    engine->addImportPath(QCoreApplication::applicationDirPath());
    engine->addImportPath(QDir::homePath() + QStringLiteral("/.local/lib/qt6/qml"));
    const QByteArray extra = qgetenv("QML_IMPORT_PATH");
    const QList<QByteArray> parts = extra.split(':');
    for (const QByteArray& part : parts) {
        if (!part.isEmpty()) engine->addImportPath(QString::fromLocal8Bit(part));
    }
}

static bool videoAudioRequested(int argc, char** argv) {
    const QByteArray env = qgetenv("BENDWALL_VIDEO_AUDIO");
    if (env == "1") return true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--audio") == 0) return true;
    }
    return false;
}

static QString watchDir(int argc, char** argv) {
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--audio") == 0) continue;
        return QString::fromLocal8Bit(argv[i]);
    }
    return {};
}

/* The window is the Plasma page: main.qml, then WebLayer when the item is web.
   wallpaperAudio is the --watch opt-in. Plasma leaves it unset, so a missing
   configuration stays muted. */
static int loadWatch(QQmlApplicationEngine* engine, QList<QQmlError>* errs, const QString& dir, bool audio) {
    const QString qml = watchQmlPath();
    if (qml.isEmpty()) {
        std::cerr << "watch.qml not found\n";
        return 1;
    }
    QObject::connect(engine, &QQmlApplicationEngine::warnings,
                     [errs](const QList<QQmlError>& w) { if (errs) *errs += w; });
    addImports(engine);
    const QString abs = QFileInfo(dir).absoluteFilePath();
    engine->rootContext()->setContextProperty(QStringLiteral("wallpaperDir"), abs);
    engine->rootContext()->setContextProperty(QStringLiteral("wallpaperKind"), topType(abs));
    engine->rootContext()->setContextProperty(QStringLiteral("wallpaperAudio"), audio);
    engine->load(QUrl::fromLocalFile(qml));
    if (engine->rootObjects().isEmpty()) {
        if (errs) {
            for (const QQmlError& err : *errs)
                std::cerr << err.toString().toStdString() << "\n";
        }
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
    if (!window) return 1;
    window->resize(960, 540);
    window->show();
    return 0;
}

static int watchCheck(const QString& dir, bool audio) {
    QQmlApplicationEngine engine;
    QList<QQmlError> errs;
    if (loadWatch(&engine, &errs, dir, audio)) return 1;
    QEventLoop loop;
    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
    loop.exec();
    QObject* root = engine.rootObjects().first();
    QObject* frame = root->findChild<QObject*>(QStringLiteral("bendwallFrame"));
    QObject* loader = root->findChild<QObject*>(QStringLiteral("bendwallWeb"));
    QString kind = frame ? frame->property("kind").toString() : QString();
    if (kind.isEmpty()) {
        QTimer::singleShot(800, &loop, &QEventLoop::quit);
        loop.exec();
        kind = frame ? frame->property("kind").toString() : QString();
    }
    const QString page = frame ? frame->property("pageUrl").toString() : QString();
    const int status = loader ? loader->property("status").toInt() : -1;
    const QByteArray kindB = kind.toUtf8();
    const QByteArray pageB = page.toUtf8();
    const bool muted = frame ? frame->property("videoMuted").toBool() : true;
    std::printf("WATCH kind=%s loader=%d page=%s\n", kindB.constData(), status, pageB.constData());
    std::printf("WATCHMUTE %d\n", muted ? 1 : 0);
    for (const QQmlError& err : errs) {
        const QByteArray line = err.toString().toUtf8();
        std::printf("WATCHWARN %s\n", line.constData());
    }
    return kind.isEmpty() ? 1 : 0;
}

/* The Plasma page's own timer, not a direct Engine::step. A static scene
   stays identical; a particle system has to move, and the step counter has
   to keep climbing, or the clock never reached the player. */
static int watchDiff(const QString& dir, bool audio) {
    QQmlApplicationEngine engine;
    QList<QQmlError> errs;
    if (loadWatch(&engine, &errs, dir, audio)) return 1;
    QObject* root = engine.rootObjects().first();
    QObject* frame = root->findChild<QObject*>(QStringLiteral("bendwallFrame"));
    if (!frame) {
        std::cerr << "bendwallFrame missing\n";
        return 1;
    }
    if (frame->property("width").toReal() < 8 || frame->property("height").toReal() < 8) {
        frame->setProperty("width", 320);
        frame->setProperty("height", 180);
    }
    QElapsedTimer clock;
    clock.start();
    while (frame->property("steps").toInt() < 1 && clock.elapsed() < 2500)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
    const int s0 = frame->property("steps").toInt();
    const QImage a = frame->property("frameImage").value<QImage>();
    QEventLoop loop;
    QTimer::singleShot(1100, &loop, &QEventLoop::quit);
    loop.exec();
    const int s1 = frame->property("steps").toInt();
    const QImage b = frame->property("frameImage").value<QImage>();
    const int diff = (!a.isNull() && !b.isNull()) ? pictureDiff(a, b) : 0;
    std::printf("WATCHDIFF steps=%d %d diff=%d size=%dx%d\n", s0, s1, diff, b.width(), b.height());
    for (const QQmlError& err : errs) {
        const QByteArray line = err.toString().toUtf8();
        std::printf("WATCHWARN %s\n", line.constData());
    }
    if (s0 < 1 || s1 < s0 + 8) return 1;
    if (!diff || b.width() < 8 || b.height() < 8) return 1;
    return 0;
}

int main(int argc, char** argv) {
    const std::string cmd = argc > 1 ? argv[1] : "";
    /* WebEngineView stays uncreatable until this runs, and it has to run
       before the application object. Plasmashell does its own init; the
       preview process has to do it itself. */
#ifdef BENDWALL_HAS_WEBENGINE
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();
#endif
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
    if (cmd == "--play-frames" && argc >= 3) {
        QCoreApplication app(argc, argv);
        const int n = argc >= 4 ? std::atoi(argv[3]) : 30;
        return playFrames(QString::fromLocal8Bit(argv[2]), n);
    }
    if (cmd == "--watch" && argc >= 3) {
        QGuiApplication app(argc, argv);
        QQmlApplicationEngine engine;
        QList<QQmlError> errs;
        const QString dir = watchDir(argc, argv);
        if (dir.isEmpty() || loadWatch(&engine, &errs, dir, videoAudioRequested(argc, argv))) return 1;
        return app.exec();
    }
    if (cmd == "--watch-check" && argc >= 3) {
        QGuiApplication app(argc, argv);
        const QString dir = watchDir(argc, argv);
        if (dir.isEmpty()) return 1;
        return watchCheck(dir, videoAudioRequested(argc, argv));
    }
    if (cmd == "--watch-diff" && argc >= 3) {
        QGuiApplication app(argc, argv);
        const QString dir = watchDir(argc, argv);
        if (dir.isEmpty()) return 1;
        return watchDiff(dir, videoAudioRequested(argc, argv));
    }
    std::cerr << "usage:\n"
              << "  bendwall-preview --dump-props <project.json>\n"
              << "  bendwall-preview --dump-workshop <workshop-root>\n"
              << "  bendwall-preview --grab <qml> <frame.png> <project.json> <workshop> <out.png> <qml-import-path>\n"
              << "  bendwall-preview --play-frames <item-dir> [frames]\n"
              << "  bendwall-preview --watch <item-dir> [--audio]\n"
              << "  bendwall-preview --watch-check <item-dir> [--audio]\n"
              << "  bendwall-preview --watch-diff <item-dir> [--audio]\n";
    return 2;
}
