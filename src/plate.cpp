#include "plate.h"

#include <QJsonArray>
#include <algorithm>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>
#include <cmath>

Plate::Plate(int rows, int columns)
    : m_rows(rows), m_columns(columns), m_wells(rows * columns) {}

bool Plate::isSupportedSize(int rows, int columns) {
    return (rows == 8 && columns == 12) || (rows == 16 && columns == 24);
}

bool Plate::contains(int row, int column) const {
    return row >= 0 && row < m_rows && column >= 0 && column < m_columns;
}

int Plate::count(WellType type) const {
    return std::count_if(m_wells.begin(), m_wells.end(), [type](const Well &w) { return w.type == type; });
}

QRect Plate::clip(const QRect &rect) const {
    return rect.normalized().intersected(QRect(0, 0, m_columns, m_rows));
}

void Plate::dilute(const QRect &rect, double start, double factor, Direction direction, WellType fillType) {
    const QRect area = clip(rect);
    for (int row = area.top(); row <= area.bottom(); ++row) {
        for (int column = area.left(); column <= area.right(); ++column) {
            const int step = direction == Direction::Right ? column - area.left()
                           : direction == Direction::Left  ? area.right() - column
                           : direction == Direction::Down  ? row - area.top()
                                                           : area.bottom() - row;
            Well &w = well(row, column);
            w.concentration = start / std::pow(factor, step);
            if (w.type == WellType::Empty)
                w.type = fillType;
        }
    }
}

QRect Plate::replicaRect(const QRect &rect, Direction direction) const {
    const QRect area = clip(rect);
    if (area.isEmpty())
        return {};
    const QRect target = direction == Direction::Right ? area.translated(area.width(), 0)
                       : direction == Direction::Left  ? area.translated(-area.width(), 0)
                       : direction == Direction::Down  ? area.translated(0, area.height())
                                                       : area.translated(0, -area.height());
    return clip(target) == target ? target : QRect();
}

QRect Plate::replicate(const QRect &rect, Direction direction) {
    const QRect source = clip(rect);
    const QRect target = replicaRect(source, direction);
    if (target.isEmpty())
        return {};
    const int dx = target.left() - source.left(), dy = target.top() - source.top();
    // The blocks never overlap, so copying in place is safe.
    for (int row = source.top(); row <= source.bottom(); ++row)
        for (int column = source.left(); column <= source.right(); ++column)
            well(row + dy, column + dx) = well(row, column);
    return target;
}

void Plate::clear(const QRect &rect) {
    const QRect area = clip(rect);
    for (int row = area.top(); row <= area.bottom(); ++row)
        for (int column = area.left(); column <= area.right(); ++column)
            well(row, column) = Well();
}

QRect Plate::pasteLabels(const QPoint &topLeft, const QVector<QStringList> &table, WellType fillType, int *dropped) {
    int width = 0, lost = 0;
    for (const QStringList &line : table)
        width = std::max(width, int(line.size()));
    for (int r = 0; r < table.size(); ++r) {
        for (int c = 0; c < table[r].size(); ++c) {
            const QString name = table[r][c].trimmed();
            if (name.isEmpty())
                continue;
            const int row = topLeft.y() + r, column = topLeft.x() + c;
            if (!contains(row, column)) {
                ++lost;
                continue;
            }
            Well &w = well(row, column);
            w.label = name;
            if (w.type == WellType::Empty)
                w.type = fillType;
        }
    }
    if (dropped)
        *dropped = lost;
    return clip(QRect(topLeft.x(), topLeft.y(), width, int(table.size())));
}

QString Plate::labelsAsTable(const QRect &rect) const {
    const QRect area = clip(rect);
    QStringList lines;
    for (int row = area.top(); row <= area.bottom(); ++row) {
        QStringList cells;
        for (int column = area.left(); column <= area.right(); ++column)
            cells.append(well(row, column).label);
        lines.append(cells.join('\t'));
    }
    return lines.join('\n');
}

QVector<QStringList> Plate::parseTable(const QString &text) {
    // Spreadsheets copy with tabs; CSV files use semicolons where the comma is
    // the decimal separator, and commas elsewhere. The first line decides.
    const QString first = text.section('\n', 0, 0);
    const QChar separator = first.contains('\t') ? '\t' : first.contains(';') ? ';' : ',';

    QVector<QStringList> table;
    QStringList line;
    QString cell;
    bool quoted = false, any = false;
    auto endLine = [&] {
        line.append(cell);
        cell.clear();
        if (any)
            table.append(line);
        line.clear();
        any = false;
    };
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (quoted) {
            if (ch == '"' && i + 1 < text.size() && text[i + 1] == '"') {
                cell += '"';
                ++i;
            } else if (ch == '"') {
                quoted = false;
            } else {
                cell += ch;
            }
        } else if (ch == '"' && cell.trimmed().isEmpty()) {
            quoted = true;
            cell.clear();
            any = true;
        } else if (ch == separator) {
            line.append(cell);
            cell.clear();
        } else if (ch == '\n') {
            endLine();
        } else if (ch != '\r') {
            cell += ch;
            any = any || !ch.isSpace();
        }
    }
    if (!cell.isEmpty() || !line.isEmpty())
        endLine();
    static const QStringList headers{"name", "names", "sample", "samples", "sample name", "sample names",
                                     "sample id", "id", "label", "labels"};
    if (!table.isEmpty() && headers.contains(table.first().value(0).trimmed().toLower()))
        table.removeFirst();
    return table;
}

QString Plate::rowName(int row) {
    return QString(QChar('A' + row));
}

QString Plate::wellName(int row, int column) {
    return rowName(row) + QString::number(column + 1);
}

bool Plate::parseWellName(const QString &name, int *row, int *column) const {
    static const QRegularExpression pattern("^([A-Za-z])0?(\\d{1,2})$");
    const auto match = pattern.match(name.trimmed());
    if (!match.hasMatch())
        return false;
    const int r = match.captured(1).toUpper().at(0).unicode() - 'A';
    const int c = match.captured(2).toInt() - 1;
    if (!contains(r, c))
        return false;
    *row = r;
    *column = c;
    return true;
}

QString Plate::rangeName(const QRect &rect) {
    const QRect r = rect.normalized();
    const QString first = wellName(r.top(), r.left());
    return r.width() == 1 && r.height() == 1 ? first : first + ":" + wellName(r.bottom(), r.right());
}

QString Plate::typeId(WellType type) {
    switch (type) {
    case WellType::Sample: return "sample";
    case WellType::Standard: return "standard";
    case WellType::ControlPositive: return "control+";
    case WellType::ControlNegative: return "control-";
    case WellType::Blank: return "blank";
    case WellType::Empty: return "empty";
    }
    return "empty";
}

QString Plate::typeCsvName(WellType type) {
    switch (type) {
    case WellType::Sample: return "Sample";
    case WellType::Standard: return "Standard";
    case WellType::ControlPositive: return "Control+";
    case WellType::ControlNegative: return "Control-";
    case WellType::Blank: return "Blank";
    case WellType::Empty: return "Empty";
    }
    return "Empty";
}

const char *Plate::typeDisplayName(WellType type) {
    switch (type) {
    case WellType::Sample: return QT_TRANSLATE_NOOP("Plate", "Sample");
    case WellType::Standard: return QT_TRANSLATE_NOOP("Plate", "Standard");
    case WellType::ControlPositive: return QT_TRANSLATE_NOOP("Plate", "Control+");
    case WellType::ControlNegative: return QT_TRANSLATE_NOOP("Plate", "Control−");
    case WellType::Blank: return QT_TRANSLATE_NOOP("Plate", "Blank");
    case WellType::Empty: return QT_TRANSLATE_NOOP("Plate", "Empty");
    }
    return "Empty";
}

std::optional<WellType> Plate::typeFromId(const QString &id) {
    for (int i = 0; i < WellTypeCount; ++i)
        if (typeId(WellType(i)) == id)
            return WellType(i);
    return std::nullopt;
}

// Only wells that hold something are written, in plate order, so a file
// reads top to bottom like the plate and diffs stay small.
QJsonObject Plate::toJson() const {
    QJsonArray wells;
    for (int row = 0; row < m_rows; ++row) {
        for (int column = 0; column < m_columns; ++column) {
            const Well &w = well(row, column);
            if (w == Well())
                continue;
            QJsonObject object{{"well", wellName(row, column)}, {"type", typeId(w.type)}};
            if (!w.label.isEmpty())
                object.insert("label", w.label);
            if (w.concentration)
                object.insert("concentration", *w.concentration);
            wells.append(object);
        }
    }
    QJsonObject root{{"format", "plate"}, {"version", 1}, {"rows", m_rows}, {"columns", m_columns}, {"wells", wells}};
    if (!m_unit.isEmpty())
        root.insert("unit", m_unit);
    return root;
}

QByteArray Plate::toJsonBytes() const {
    return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
}

namespace {
QString csvField(const QString &value) {
    if (!value.contains(',') && !value.contains('"') && !value.contains('\n') && !value.contains('\r'))
        return value;
    return '"' + QString(value).replace('"', "\"\"") + '"';
}
} // namespace

// Every well, Empty included, so instruments that expect a full plate get one.
// Numbers are written without locale formatting, the way instruments read them.
QByteArray Plate::toCsv() const {
    QString csv = "Well,Type,Label,Concentration\r\n";
    for (int row = 0; row < m_rows; ++row) {
        for (int column = 0; column < m_columns; ++column) {
            const Well &w = well(row, column);
            csv += wellName(row, column) + ',' + typeCsvName(w.type) + ',' + csvField(w.label) + ','
                 + (w.concentration ? QString::number(*w.concentration, 'g', 12) : QString()) + "\r\n";
        }
    }
    return csv.toUtf8();
}

std::optional<Plate> Plate::fromJson(const QByteArray &json, QVector<PlateProblem> *problems) {
    auto problem = [problems](const QString &well, const QString &message) {
        if (problems)
            problems->append({well, message});
    };
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        problem({}, QString("Not valid JSON at offset %1: %2").arg(error.offset).arg(error.errorString()));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (!document.isObject() || root.value("format").toString() != "plate") {
        problem({}, "Not a plate file: \"format\" must be \"plate\".");
        return std::nullopt;
    }
    if (root.value("version").toInt() != 1)
        problem({}, "Unsupported version; this plate reads version 1.");
    const int rows = root.value("rows").toInt(), columns = root.value("columns").toInt();
    if (!isSupportedSize(rows, columns)) {
        problem({}, QString("A plate is 8 rows × 12 columns or 16 × 24, not %1 × %2.").arg(rows).arg(columns));
        return std::nullopt;
    }

    Plate plate(rows, columns);
    if (root.contains("unit") && !root.value("unit").isString())
        problem({}, "\"unit\" must be a string.");
    plate.setUnit(root.value("unit").toString());
    if (!root.value("wells").isArray()) {
        problem({}, "\"wells\" must be a list.");
        return plate;
    }

    QSet<QString> seen;
    const QJsonArray wells = root.value("wells").toArray();
    for (qsizetype i = 0; i < wells.size(); ++i) {
        const QJsonObject object = wells.at(i).toObject();
        const QString name = object.value("well").toString();
        int row = 0, column = 0;
        if (!wells.at(i).isObject() || !plate.parseWellName(name, &row, &column)) {
            problem(name, QString("Entry %1: \"%2\" is not a well on a %3-well plate.")
                              .arg(i + 1).arg(name).arg(rows * columns));
            continue;
        }
        const QString canonical = wellName(row, column);
        if (seen.contains(canonical)) {
            problem(canonical, canonical + " is listed more than once.");
            continue;
        }
        seen.insert(canonical);

        Well &w = plate.well(row, column);
        const QJsonValue typeValue = object.value("type");
        const auto type = typeFromId(typeValue.isUndefined() ? QString("empty") : typeValue.toString());
        if (!typeValue.isUndefined() && !typeValue.isString())
            problem(canonical, canonical + ": \"type\" must be a string.");
        else if (!type)
            problem(canonical, QString("%1: unknown type \"%2\"; use sample, standard, control+, control-, blank, or empty.")
                                   .arg(canonical, object.value("type").toString()));
        w.type = type.value_or(WellType::Empty);
        if (object.contains("label") && !object.value("label").isString())
            problem(canonical, canonical + ": \"label\" must be a string.");
        w.label = object.value("label").toString();
        if (object.contains("concentration") && !object.value("concentration").isNull()) {
            const QJsonValue value = object.value("concentration");
            if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < 0)
                problem(canonical, canonical + ": \"concentration\" must be a number, zero or more.");
            else
                w.concentration = value.toDouble();
        }
    }
    return plate;
}
