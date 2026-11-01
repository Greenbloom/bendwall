#include "catalog.h"
#include "frameitem.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtGlobal>

class BendwallPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char* uri) override {
        qmlRegisterType<FrameItem>(uri, 1, 0, "Frame");
        qmlRegisterSingletonType<Catalog>(uri, 1, 0, "Catalog",
            [](QQmlEngine*, QJSEngine*) -> QObject* { return new Catalog(); });
    }

    void initializeEngine(QQmlEngine* engine, const char* uri) override {
        Q_UNUSED(uri);
        if (!engine) return;
        engine->rootContext()->setContextProperty(
            QStringLiteral("bendwallQtVersion"), QString::fromLatin1(qVersion()));
    }
};

#include "plugin.moc"
