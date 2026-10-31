#include "frameitem.h"

#include <QQmlExtensionPlugin>

class BendwallPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char* uri) override {
        qmlRegisterType<FrameItem>(uri, 1, 0, "Frame");
    }
};

#include "plugin.moc"
