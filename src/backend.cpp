#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

#include "filepicker.h"
#include "platepainter.h"

namespace {
// Enough to walk back through a long session without holding every edit forever.
constexpr int HistoryLimit = 500;

Direction toDirection(int direction) {
    return direction == 1 ? Direction::Left : direction == 2 ? Direction::Down : direction == 3 ? Direction::Up
                                                                                                 : Direction::Right;
}
} // namespace

Backend::Backend(FilePicker *picker, QObject *parent) : QObject(parent), m_picker(picker), m_history{Plate()} {
    if (!m_picker)
        return;
    connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) { openPath(url.toLocalFile()); });
    connect(m_picker, &FilePicker::saveSelected, this, [this](const QUrl &url) {
        QString path = url.toLocalFile();
        if (QFileInfo(path).suffix().isEmpty())
            path += ".plate";
        savePath(path);
    });
    connect(m_picker, &FilePicker::exportSelected, this, [this](const QUrl &url, const QString &format) {
        QString path = url.toLocalFile();
        if (QFileInfo(path).suffix().isEmpty())
            path += "." + format;
        exportTo(path, format);
    });
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) { say(message, true); });
}

QString Backend::fileName() const {
    return m_filePath.isEmpty() ? tr("Untitled plate") : QFileInfo(m_filePath).fileName();
}

QVariantList Backend::typeCounts() const {
    QVariantList counts;
    for (int i = 0; i < WellTypeCount; ++i)
        counts.append(plate().count(WellType(i)));
    return counts;
}

void Backend::setCurrentType(int type) {
    if (type < 0 || type >= WellTypeCount || type == int(m_currentType))
        return;
    m_currentType = WellType(type);
    emit currentTypeChanged();
}

QString Backend::typeName(int type) const {
    if (type < 0 || type >= WellTypeCount)
        return {};
    return QCoreApplication::translate("Plate", Plate::typeDisplayName(WellType(type)));
}

QVariantMap Backend::wellInfo(int row, int column) const {
    if (!plate().contains(row, column))
        return {};
    const Well &well = plate().well(row, column);
    QVariantMap info{{"name", Plate::wellName(row, column)},
                     {"type", int(well.type)},
                     {"typeName", typeName(int(well.type))},
                     {"label", well.label}};
    if (well.concentration)
        info.insert("concentration", *well.concentration);
    return info;
}

QVariantMap Backend::selectionInfo() const {
    const QRect area = plate().clip(m_selection);
    QVariantMap info{{"name", Plate::rangeName(area)}, {"count", area.width() * area.height()}};
    if (area.isEmpty())
        return info;
    const Well &first = plate().well(area.top(), area.left());
    bool labelMixed = false, concentrationMixed = false;
    for (int row = area.top(); row <= area.bottom(); ++row) {
        for (int column = area.left(); column <= area.right(); ++column) {
            const Well &well = plate().well(row, column);
            labelMixed = labelMixed || well.label != first.label;
            concentrationMixed = concentrationMixed || well.concentration != first.concentration;
        }
    }
    info.insert("label", labelMixed ? QString() : first.label);
    info.insert("labelMixed", labelMixed);
    info.insert("concentrationMixed", concentrationMixed);
    if (!concentrationMixed && first.concentration)
        info.insert("concentration", *first.concentration);
    return info;
}

void Backend::newPlate(int wells) {
    resetHistory(wells == 384 ? Plate(16, 24) : Plate(8, 12));
    if (!m_filePath.isEmpty()) {
        m_filePath.clear();
        emit fileChanged();
    }
    setCursor(0, 0);
    say(tr("New %1-well plate").arg(plate().wellCount()));
}

void Backend::setCursor(int row, int column, bool extend) {
    row = std::clamp(row, 0, rows() - 1);
    column = std::clamp(column, 0, columns() - 1);
    m_cursor = QPoint(column, row);
    if (!extend)
        m_anchor = m_cursor;
    m_selection = QRect(m_anchor, m_cursor).normalized();
    emit selectionChanged();
}

void Backend::moveCursor(int rowStep, int columnStep, bool extend) {
    setCursor(m_cursor.y() + rowStep, m_cursor.x() + columnStep, extend);
}

void Backend::select(int row1, int column1, int row2, int column2) {
    setCursor(row1, column1);
    setCursor(row2, column2, true);
}

void Backend::assign(int row1, int column1, int row2, int column2) {
    select(row1, column1, row2, column2);
    assignSelection();
}

void Backend::assignSelection() {
    Plate next = plate();
    const QRect area = next.clip(m_selection);
    for (int row = area.top(); row <= area.bottom(); ++row)
        for (int column = area.left(); column <= area.right(); ++column)
            next.well(row, column).type = m_currentType;
    commit(next);
}

void Backend::setSelectionLabel(const QString &label) {
    Plate next = plate();
    const QRect area = next.clip(m_selection);
    for (int row = area.top(); row <= area.bottom(); ++row)
        for (int column = area.left(); column <= area.right(); ++column)
            next.well(row, column).label = label.trimmed();
    commit(next);
}

void Backend::setSelectionConcentration(const QVariant &value) {
    bool ok = false;
    const double number = value.toDouble(&ok);
    std::optional<double> concentration;
    if (ok && value.isValid() && !(value.typeId() == QMetaType::QString && value.toString().trimmed().isEmpty())) {
        if (!std::isfinite(number) || number < 0) {
            say(tr("A concentration is a number, zero or more."), true);
            return;
        }
        concentration = number;
    }
    Plate next = plate();
    const QRect area = next.clip(m_selection);
    for (int row = area.top(); row <= area.bottom(); ++row)
        for (int column = area.left(); column <= area.right(); ++column)
            next.well(row, column).concentration = concentration;
    commit(next);
}

void Backend::setUnit(const QString &unit) {
    Plate next = plate();
    next.setUnit(unit.trimmed());
    commit(next);
}

bool Backend::dilute(double start, double factor, int direction) {
    if (!std::isfinite(start) || start <= 0 || !std::isfinite(factor) || factor <= 0) {
        say(tr("A dilution needs a start concentration and a factor above zero."), true);
        return false;
    }
    Plate next = plate();
    next.dilute(m_selection, start, factor, toDirection(direction), m_currentType);
    commit(next);
    say(tr("Diluted %1 by %2").arg(selectionName(), QLocale().toString(factor, 'g', 6)));
    return true;
}

bool Backend::replicate(int direction) {
    Plate next = plate();
    const QRect target = next.replicate(m_selection, toDirection(direction));
    if (target.isEmpty()) {
        say(tr("No room beside %1 for a replicate.").arg(selectionName()), true);
        return false;
    }
    commit(next);
    const QString source = selectionName();
    // The copy becomes the selection, so pressing R again makes a triplicate.
    select(target.top(), target.left(), target.bottom(), target.right());
    say(tr("Replicated %1 to %2").arg(source, selectionName()));
    return true;
}

void Backend::undo() {
    if (!canUndo())
        return;
    --m_index;
    ++m_revision;
    emit plateChanged();
}

void Backend::redo() {
    if (!canRedo())
        return;
    ++m_index;
    ++m_revision;
    emit plateChanged();
}

void Backend::commit(const Plate &next) {
    if (next == plate())
        return;
    // The saved state is on the redo branch about to be dropped; nothing will match it again.
    if (m_cleanIndex > m_index)
        m_cleanIndex = -1;
    m_history.resize(m_index + 1);
    m_history.append(next);
    ++m_index;
    if (m_history.size() > HistoryLimit) {
        m_history.removeFirst();
        --m_index;
        // The saved state may have just fallen off the end; then nothing matches it.
        m_cleanIndex = m_cleanIndex > 0 ? m_cleanIndex - 1 : -1;
    }
    ++m_revision;
    emit plateChanged();
}

void Backend::resetHistory(const Plate &next) {
    m_history = {next};
    m_index = 0;
    m_cleanIndex = 0;
    ++m_revision;
    emit plateChanged();
}

void Backend::say(const QString &message, bool error) {
    m_message = message;
    m_messageIsError = error;
    emit messageChanged();
}

void Backend::clearMessage() {
    if (m_message.isEmpty())
        return;
    m_message.clear();
    m_messageIsError = false;
    emit messageChanged();
}

bool Backend::openPath(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        say(tr("Could not open %1: %2").arg(path, file.errorString()), true);
        return false;
    }
    QVector<PlateProblem> problems;
    const auto opened = Plate::fromJson(file.readAll(), &problems);
    if (!opened) {
        say(tr("Could not open %1: %2").arg(QFileInfo(path).fileName(), problems.value(0).message), true);
        return false;
    }
    resetHistory(*opened);
    m_filePath = QFileInfo(path).absoluteFilePath();
    emit fileChanged();
    setCursor(0, 0);
    if (problems.isEmpty())
        say(tr("Opened %1").arg(fileName()));
    else
        say(tr("Opened %1 with %n problem(s); the first: %2", "", problems.size())
                .arg(fileName(), problems.first().message), true);
    return true;
}

bool Backend::savePath(const QString &path) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(plate().toJsonBytes()) < 0 || !file.commit()) {
        say(tr("Could not save %1: %2").arg(path, file.errorString()), true);
        return false;
    }
    m_filePath = QFileInfo(path).absoluteFilePath();
    m_cleanIndex = m_index;
    emit fileChanged();
    emit plateChanged();
    say(tr("Saved %1").arg(fileName()));
    emit saved();
    return true;
}

bool Backend::exportTo(const QString &path, const QString &format) {
    const QString title = m_filePath.isEmpty() ? tr("Untitled plate") : QFileInfo(m_filePath).completeBaseName();
    QString error;
    const bool ok = format == "csv" ? exportCsv(plate(), path, &error)
                  : format == "svg" ? exportSvg(plate(), path, title, m_accent, &error)
                  : format == "pdf" ? exportPdf(plate(), path, title, m_accent, &error)
                                    : (error = tr("Unknown export format %1").arg(format), false);
    if (!ok) {
        say(tr("Could not export %1: %2").arg(path, error), true);
        return false;
    }
    say(tr("Exported %1").arg(QFileInfo(path).fileName()));
    return true;
}

QString Backend::suggestedPath(const QString &extension) const {
    if (!m_filePath.isEmpty()) {
        const QFileInfo info(m_filePath);
        return info.absoluteDir().filePath(info.completeBaseName() + "." + extension);
    }
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QDir(folder).exists())
        folder = QDir::homePath();
    return QDir(folder).filePath("plate." + extension);
}

void Backend::open() {
    if (m_picker)
        m_picker->openPlate();
}

void Backend::save() {
    if (!m_filePath.isEmpty())
        savePath(m_filePath);
    else
        saveAs();
}

void Backend::saveAs() {
    if (m_picker)
        m_picker->savePlate(QUrl::fromLocalFile(suggestedPath("plate")));
}

void Backend::exportPlate() {
    if (m_picker)
        m_picker->exportPlate(QUrl::fromLocalFile(suggestedPath("csv")));
}
