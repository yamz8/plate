#pragma once

#include <QString>
#include <QVector>
#include <optional>

#include "plate.h"

// Layouts to start a new plate from. A few ship inside the binary; the ones
// people save live as ordinary .plate files in ~/.local/share/plate/templates,
// so they can be copied, shared, or edited by hand. Shared by the window and
// the command line.
namespace Templates {

struct Info {
    QString name;
    bool builtIn = false;
    int wells = 96;
};

QString defaultDirectory();
// Built-in templates first, then saved ones, each sorted by name. A saved
// template with a built-in's name hides the built-in.
QVector<Info> list(const QString &directory);
std::optional<Plate> load(const QString &directory, const QString &name, QString *error);
// Names become file names, so they can't hold a slash or start with a dot.
bool isValidName(const QString &name);
bool save(const QString &directory, const QString &name, const Plate &plate, QString *error);
bool remove(const QString &directory, const QString &name, QString *error);

} // namespace Templates
