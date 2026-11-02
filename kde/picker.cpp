#include "frameitem.h"
#include "library.h"
#include "shellconfig.h"

#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QEventLoop>

#ifdef BENDWALL_HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

namespace {

struct Options {
    bool check = false;
    QString workshop;
    QString vdf;
    QString mock;
    QString shot;
};

Options parseArgs(int argc, char** argv, QString* error) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        auto need = [&](const char* name) -> QString {
            if (i + 1 >= argc) {
                if (error)
                    *error = QStringLiteral("%1 needs a path").arg(QString::fromLatin1(name));
                return QString();
            }
            return QString::fromLocal8Bit(argv[++i]);
        };
        if (arg == QLatin1String("--check"))
            opt.check = true;
        else if (arg == QLatin1String("--workshop"))
            opt.workshop = need("--workshop");
        else if (arg == QLatin1String("--libraryfolders"))
            opt.vdf = need("--libraryfolders");
        else if (arg == QLatin1String("--mock"))
            opt.mock = need("--mock");
        else if (arg == QLatin1String("--screenshot"))
            opt.shot = need("--screenshot");
        else if (error)
            *error = QStringLiteral("Unknown argument %1").arg(arg);
        if (error && !error->isEmpty())
            return opt;
    }
    if (opt.check && (opt.workshop.isEmpty() || opt.mock.isEmpty() || opt.shot.isEmpty()) && error)
        *error = QStringLiteral("--check needs --workshop, --mock, and --screenshot");
    return opt;
}

QObject* childNamed(QObject* root, const char* name) {
    if (!root)
        return nullptr;
    return root->findChild<QObject*>(QString::fromLatin1(name), Qt::FindChildrenRecursively);
}

int fail(const QString& text) {
    fprintf(stderr, "picker check: %s\n", text.toUtf8().constData());
    return 1;
}

int runCheck(QGuiApplication* app, QQmlApplicationEngine* engine, Library* library, ShellConfig* shell,
             const QString& shot) {
    QQuickWindow* window = nullptr;
    for (QObject* obj : engine->rootObjects()) {
        window = qobject_cast<QQuickWindow*>(obj);
        if (window)
            break;
    }
    if (!window)
        return fail(QStringLiteral("window did not load"));
    window->show();
    app->processEvents();
    const char* names[] = {"searchField", "wallpaperGrid", "applyButton", "livePane", "screenBox",
                           "muteBox", "volumeSlider", "autoMuteBox"};
    for (const char* name : names) {
        if (!childNamed(window, name))
            return fail(QStringLiteral("missing %1").arg(QString::fromLatin1(name)));
    }
    if (library->scanning() || library->totalCount() == 0) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(library, &Library::scanFinished, &loop, &QEventLoop::quit);
        timer.start(15000);
        loop.exec();
    }
    if (library->scanning())
        return fail(QStringLiteral("scan did not finish"));
    if (library->totalCount() != 5)
        return fail(QStringLiteral("expected 5 wallpapers, got %1").arg(library->totalCount()));

    QStringList ids;
    library->setQuery(QString());
    library->setTypeFilter(QString());
    library->setTagFilter(QString());
    for (int i = 0; i < library->shownCount(); ++i)
        ids << library->idAt(i);
    const QStringList want = {"1001", "1002", "1003", "1004", "1005"};
    for (const QString& id : want) {
        if (!ids.contains(id))
            return fail(QStringLiteral("missing item %1").arg(id));
    }

    library->setQuery(QStringLiteral("Night"));
    if (library->shownCount() != 1 || library->idAt(0) != QLatin1String("1002"))
        return fail(QStringLiteral("search Night"));
    library->setQuery(QString());
    library->setTypeFilter(QStringLiteral("web"));
    if (library->shownCount() != 1 || library->idAt(0) != QLatin1String("1003"))
        return fail(QStringLiteral("type web"));
    library->setTypeFilter(QStringLiteral("other"));
    if (library->shownCount() != 1 || library->idAt(0) != QLatin1String("1004"))
        return fail(QStringLiteral("type other"));
    if (library->liveOkAt(0) || !library->noteAt(0).contains(QStringLiteral("Unsupported")))
        return fail(QStringLiteral("application item should stay unsupported"));
    library->setTypeFilter(QString());
    library->setTagFilter(QStringLiteral("Abstract"));
    if (library->shownCount() != 1 || library->idAt(0) != QLatin1String("1001"))
        return fail(QStringLiteral("tag Abstract"));
    library->setTagFilter(QString());

    if (shell->activeText(QStringLiteral("1001")) != QLatin1String("Screen 0"))
        return fail(QStringLiteral("1001 should be the wallpaper on screen 0"));

    shell->setMute(false);
    shell->setVolume(40);
    shell->setAutoMute(true);
    if (!shell->apply(QStringLiteral("1002"), 0))
        return fail(shell->error().isEmpty() ? QStringLiteral("apply failed") : shell->error());
    library->setActiveText(shell->activeMap());
    if (shell->activeText(QStringLiteral("1002")) != QLatin1String("Screen 0"))
        return fail(QStringLiteral("1002 was not applied to screen 0"));
    if (!shell->activeText(QStringLiteral("1001")).isEmpty())
        return fail(QStringLiteral("1001 is still active"));

    const QVariantMap chosen = library->paper(QStringLiteral("1001"));
    if (chosen.isEmpty())
        return fail(QStringLiteral("1001 was not in the grid"));
    if (!chosen.value(QStringLiteral("liveOk")).toBool())
        return fail(QStringLiteral("1001 should be eligible for a live preview"));
    auto showStill = [&]() {
        window->setProperty("currentId", chosen.value(QStringLiteral("id")));
        window->setProperty("currentTitle", chosen.value(QStringLiteral("title")));
        window->setProperty("currentKind", chosen.value(QStringLiteral("type")));
        window->setProperty("currentDir", chosen.value(QStringLiteral("dir")));
        window->setProperty("currentPreview", chosen.value(QStringLiteral("preview")));
        window->setProperty("currentThumb", chosen.value(QStringLiteral("thumb")));
        window->setProperty("currentPage", chosen.value(QStringLiteral("page")));
        window->setProperty("currentNote", chosen.value(QStringLiteral("note")));
        window->setProperty("currentGif", chosen.value(QStringLiteral("gif")));
        window->setProperty("currentLive", false);
    };
    // The grid's first paint queues selectRow, which turns the live pane on.
    // Drain that, then pin the still so the screenshot is the preview image.
    app->processEvents();
    showStill();
    app->processEvents();
    showStill();
    QTimer flush;
    QEventLoop paint;
    flush.setSingleShot(true);
    QObject::connect(&flush, &QTimer::timeout, &paint, &QEventLoop::quit);
    flush.start(400);
    paint.exec();
    const QImage image = window->grabWindow();
    if (image.isNull() || image.width() < 900 || image.height() < 500)
        return fail(QStringLiteral("screenshot is empty"));
    if (!image.save(shot))
        return fail(QStringLiteral("could not write %1").arg(shot));

    printf("CHECK items=5 shown_night=1 shown_web=1 shown_other=1 shown_tag=1 applied=1002 screen=0 mute=0 volume=40 auto=1\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    QString error;
    const Options opt = parseArgs(argc, argv, &error);
    if (!error.isEmpty()) {
        fprintf(stderr, "%s\n", error.toUtf8().constData());
        return 2;
    }
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#ifdef BENDWALL_HAS_WEBENGINE
    QtWebEngineQuick::initialize();
#endif
    /* QQuickFramebufferObject paints only on the OpenGL scene graph. Plasma's
       default on some NVIDIA setups is Vulkan, which leaves this pane black. */
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("bendwall-picker"));
    QGuiApplication::setOrganizationName(QStringLiteral("bendwall"));
    qmlRegisterType<FrameItem>("BendwallPicker", 1, 0, "Frame");

    Library library;
    ShellConfig shell;
    if (opt.check || !opt.workshop.isEmpty() || !opt.vdf.isEmpty()) {
        QStringList roots;
        if (!opt.workshop.isEmpty())
            roots << opt.workshop;
        library.setRoots(roots, opt.vdf);
    }
    if (!opt.mock.isEmpty())
        shell.setMock(opt.mock);
    shell.refresh();
    library.setActiveText(shell.activeMap());
    QObject::connect(&shell, &ShellConfig::screensChanged, &library, [&]() {
        library.setActiveText(shell.activeMap());
    });

    QQmlApplicationEngine engine;
    QList<QQmlError> errors;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                     [&](const QList<QQmlError>& list) { errors.append(list); });
    engine.rootContext()->setContextProperty(QStringLiteral("library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("shell"), &shell);
#ifdef BENDWALL_HAS_WEBENGINE
    engine.rootContext()->setContextProperty(QStringLiteral("webEngineOk"), true);
#else
    engine.rootContext()->setContextProperty(QStringLiteral("webEngineOk"), false);
#endif
    engine.load(QUrl(QStringLiteral("qrc:/Picker.qml")));
    if (engine.rootObjects().isEmpty()) {
        for (const QQmlError& err : errors)
            fprintf(stderr, "%s\n", err.toString().toUtf8().constData());
        fprintf(stderr, "bendwall-picker: could not load the window\n");
        return 1;
    }
    library.start();
    if (opt.check)
        return runCheck(&app, &engine, &library, &shell, opt.shot);
    return app.exec();
}
