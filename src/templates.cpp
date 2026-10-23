#include "templates.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {
const QString BuiltInDirectory = ":/templates";

std::optional<Plate> read(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return std::nullopt;
    }
    QVector<PlateProblem> problems;
    auto plate = Plate::fromJson(file.readAll(), &problems);
    if (!plate || !problems.isEmpty()) {
        if (error)
            *error = problems.value(0).message;
        return std::nullopt;
    }
    return plate;
}

QString pathFor(const QString &directory, const QString &name) {
    return QDir(directory).filePath(name + ".plate");
}

QVector<Templates::Info> scan(const QString &directory, bool builtIn) {
    QVector<Templates::Info> found;
    for (const QFileInfo &file : QDir(directory).entryInfoList({"*.plate"}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        // A file that isn't a plate is left out rather than offered and refused.
        if (const auto plate = read(file.filePath(), nullptr))
            found.append({file.completeBaseName(), builtIn, plate->wellCount()});
    }
    return found;
}
} // namespace

namespace Templates {

QString defaultDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/plate/templates";
}

QVector<Info> list(const QString &directory) {
    const QVector<Info> saved = scan(directory, false);
    QVector<Info> all;
    for (const Info &info : scan(BuiltInDirectory, true))
        if (std::none_of(saved.begin(), saved.end(), [&](const Info &s) { return s.name == info.name; }))
            all.append(info);
    all.append(saved);
    return all;
}

std::optional<Plate> load(const QString &directory, const QString &name, QString *error) {
    if (!isValidName(name)) {
        if (error)
            *error = QCoreApplication::translate("Templates", "There is no template named %1.").arg(name);
        return std::nullopt;
    }
    for (const QString &path : {pathFor(directory, name), pathFor(BuiltInDirectory, name)})
        if (QFileInfo::exists(path))
            return read(path, error);
    if (error)
        *error = QCoreApplication::translate("Templates", "There is no template named %1.").arg(name);
    return std::nullopt;
}

bool isValidName(const QString &name) {
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty() && trimmed == name && !name.contains('/') && !name.contains('\\')
        && !name.startsWith('.') && name.size() <= 100;
}

bool save(const QString &directory, const QString &name, const Plate &plate, QString *error) {
    if (!isValidName(name)) {
        if (error)
            *error = QCoreApplication::translate("Templates", "A template name can't be empty, start with a dot, or hold a slash.");
        return false;
    }
    QSaveFile file(pathFor(directory, name));
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(plate.toJsonBytes()) < 0 || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool remove(const QString &directory, const QString &name, QString *error) {
    QFile file(pathFor(directory, name));
    if (!isValidName(name) || !file.exists()) {
        if (error)
            *error = QCoreApplication::translate("Templates", "Only saved templates can be deleted.");
        return false;
    }
    if (!file.remove()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace Templates
