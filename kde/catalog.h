#pragma once

#include <QObject>
#include <QStringList>

/* Workshop ids for the wallpaper config page. The list is the directory
   names under BENDWALL_WORKSHOP, or the default Steam workshop path. */
class Catalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList ids READ ids CONSTANT)

public:
    explicit Catalog(QObject* parent = nullptr);
    QStringList ids() const;
};
