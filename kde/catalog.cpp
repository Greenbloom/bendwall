#include "catalog.h"

#include "props.h"

Catalog::Catalog(QObject* parent) : QObject(parent) {}

QStringList Catalog::ids() const {
    return workshopIds(workshopRoot());
}
