#include "engine.h"
#include "props.h"

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QRasterWindow>
#include <QRect>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVector>

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
    const int ms = frameMs(eng.line());
    if (!eng.step(ms)) return 1;
    const QImage b = eng.image();
    const QRgb p0 = a.pixel(a.width() / 2, a.height() / 2);
    const QRgb p1 = b.pixel(b.width() / 2, b.height() / 2);
    const int diff = qRed(p0) != qRed(p1) || qGreen(p0) != qGreen(p1) || qBlue(p0) != qBlue(p1);
    std::printf("FRAME 0 %d %d %d %d\n", qRed(p0), qGreen(p0), qBlue(p0), qAlpha(p0));
    std::printf("FRAME 1 %d %d %d %d\n", qRed(p1), qGreen(p1), qBlue(p1), qAlpha(p1));
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
    std::printf("PLAY fps=%.1f cpu=%.1f size=%dx%d %s\n", fps, cpu, bw, bh,
                eng.line().toUtf8().constData());
    eng.close();
    return diff ? 0 : 1;
}

class Watch final : public QRasterWindow {
public:
    explicit Watch(const QString& dir) {
        resize(960, 540);
        setTitle(QStringLiteral("Bendwall"));
        m_eng.setAudio(constantAudio());
        if (!m_eng.open(dir, 960, 540)) m_hud = m_eng.error();
        else m_img = m_eng.image();
        connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
        m_timer.start(33);
        m_clock.start();
        m_ticks = bendwallProcTicks();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(0, 0, width(), height(), QColor(18, 20, 24));
        if (!m_img.isNull()) painter.drawImage(QRect(0, 0, width(), height()), m_img);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(QRectF(12, 12, 320, 36), 6, 6);
        painter.setPen(QColor(232, 230, 227));
        QFont font = painter.font();
        font.setPixelSize(16);
        painter.setFont(font);
        painter.drawText(QRectF(24, 12, 296, 36), Qt::AlignVCenter, m_hud);
    }

private:
    void tick() {
        if (m_eng.ready()) {
            m_eng.step(33);
            m_img = m_eng.image();
        }
        m_frames++;
        if (m_clock.elapsed() >= 500) {
            const long ms = m_clock.elapsed();
            const long now = bendwallProcTicks();
            const double fps = m_frames * 1000.0 / (double)ms;
            const double cpu = bendwallCpuPercent(m_ticks, now, ms);
            m_hud = QString("%1 fps   %2% cpu   %3")
                        .arg(fps, 0, 'f', 1)
                        .arg(cpu, 0, 'f', 1)
                        .arg(m_eng.kind());
            std::printf("PLAY fps=%.1f cpu=%.1f %s\n", fps, cpu, m_eng.line().toUtf8().constData());
            m_frames = 0;
            m_ticks = now;
            m_clock.restart();
        }
        if (m_hud.isEmpty()) m_hud = QStringLiteral("measuring");
        update();
    }

    Engine m_eng;
    QImage m_img;
    QString m_hud = QStringLiteral("measuring");
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_frames = 0;
    long m_ticks = 0;
};

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
    if (cmd == "--play-frames" && argc >= 3) {
        QCoreApplication app(argc, argv);
        const int n = argc >= 4 ? std::atoi(argv[3]) : 30;
        return playFrames(QString::fromLocal8Bit(argv[2]), n);
    }
    if (cmd == "--watch" && argc >= 3) {
        QGuiApplication app(argc, argv);
        Watch window(QString::fromLocal8Bit(argv[2]));
        window.show();
        return app.exec();
    }
    std::cerr << "usage:\n"
              << "  bendwall-preview --dump-props <project.json>\n"
              << "  bendwall-preview --dump-workshop <workshop-root>\n"
              << "  bendwall-preview --grab <qml> <frame.png> <project.json> <workshop> <out.png> <qml-import-path>\n"
              << "  bendwall-preview --play-frames <item-dir> [frames]\n"
              << "  bendwall-preview --watch <item-dir>\n";
    return 2;
}
