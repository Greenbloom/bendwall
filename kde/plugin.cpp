#include "catalog.h"
#include "frameitem.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class BendwallPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char* uri) override {
        qmlRegisterType<FrameItem>(uri, 1, 0, "Frame");
        qmlRegisterSingletonType<Catalog>(uri, 1, 0, "Catalog",
            [](QQmlEngine*, QJSEngine*) -> QObject* { return new Catalog(); });
    }
};

#include "plugin.moc"
