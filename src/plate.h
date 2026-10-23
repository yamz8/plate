#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// What a well holds. The order is the order of the 1–6 keys and the legend.
enum class WellType { Sample, Standard, ControlPositive, ControlNegative, Blank, Empty };
constexpr int WellTypeCount = 6;

struct Well {
    WellType type = WellType::Empty;
    QString label;
    std::optional<double> concentration;

    bool operator==(const Well &other) const = default;
};

enum class Direction { Right, Left, Down, Up };

struct PlateProblem {
    QString well; // Empty for problems with the file as a whole.
    QString message;
};

// A microplate layout: 96 wells (8 × 12) or 384 wells (16 × 24), rows lettered
// and columns numbered as on the plastic. Plain data with no Qt object model,
// so the window, the command line, and the tests all share it.
class Plate {
public:
    explicit Plate(int rows = 8, int columns = 12);

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int wellCount() const { return m_rows * m_columns; }
    QString unit() const { return m_unit; }
    void setUnit(const QString &unit) { m_unit = unit; }

    bool contains(int row, int column) const;
    const Well &well(int row, int column) const { return m_wells[row * m_columns + column]; }
    Well &well(int row, int column) { return m_wells[row * m_columns + column]; }
    int count(WellType type) const;

    // Every well inside rect, clipped to the plate; x is the column and y the row.
    QRect clip(const QRect &rect) const;

    // Concentrations across rect: the first well in the direction of travel
    // gets start, and each step after it start / factor^step. Empty wells
    // take fillType so the dilution shows on the map.
    void dilute(const QRect &rect, double start, double factor, Direction direction, WellType fillType);

    // The block beside rect in the given direction, the same size, or an
    // empty rect when the plate has no room for it.
    QRect replicaRect(const QRect &rect, Direction direction) const;
    // Copies rect into replicaRect(rect, direction) and returns where it went.
    QRect replicate(const QRect &rect, Direction direction);

    // Every well in rect back to Empty, with no label or concentration.
    void clear(const QRect &rect);

    // Writes a table of names into the labels of the wells from topLeft (x is
    // the column) as a spreadsheet pastes: one cell to a well, across and down.
    // Blank cells leave their well alone, empty wells that get a name take
    // fillType, and cells past the plate's edge are dropped. Returns the wells
    // the table covers on the plate and, in dropped, the names that didn't fit.
    QRect pasteLabels(const QPoint &topLeft, const QVector<QStringList> &table, WellType fillType, int *dropped = nullptr);
    // The labels in rect as tab-separated lines, as spreadsheets copy them.
    QString labelsAsTable(const QRect &rect) const;
    // A table from pasted or imported text: tabs, semicolons, or commas
    // between cells (in that order of preference, from the first line), CSV quoting, blank lines dropped.
    // A header row ("Sample", "Name", "ID", …) names the column, so it's dropped too.
    static QVector<QStringList> parseTable(const QString &text);

    QJsonObject toJson() const;
    QByteArray toJsonBytes() const;
    QByteArray toCsv() const;
    // Fills problems and returns an empty optional when the JSON is not a plate.
    static std::optional<Plate> fromJson(const QByteArray &json, QVector<PlateProblem> *problems);

    static bool isSupportedSize(int rows, int columns);
    static QString rowName(int row);
    static QString wellName(int row, int column);
    // "B12" to row 1, column 11; false when it doesn't name a well on this plate.
    bool parseWellName(const QString &name, int *row, int *column) const;
    static QString rangeName(const QRect &rect);

    // The id in .plate files (control+, control-) and the CSV name (Control+, Control-).
    static QString typeId(WellType type);
    static QString typeCsvName(WellType type);
    // The English display name, marked for translation (Control−, with a real minus).
    static const char *typeDisplayName(WellType type);
    static std::optional<WellType> typeFromId(const QString &id);

    bool operator==(const Plate &other) const = default;

private:
    int m_rows;
    int m_columns;
    QString m_unit;
    QVector<Well> m_wells;
};
