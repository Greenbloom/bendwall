#pragma once

#include <QString>
#include <QStringList>

/* Presentation of project.json. Bend still resolves presets and user values. */
QStringList propertyLines(const QString& projectPath);
QStringList workshopIds(const QString& workshopRoot);
