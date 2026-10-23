#pragma once

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QRectF>

#include "plate.h"

class QPainter;

struct PlatePalette {
    QColor plate, outline, grid, text, mutedText, accent;
    QColor types[WellTypeCount];
    bool print = false;
};

// Each type's color comes from the accent where it can: Sample is the accent
// and Standard its opposite, while the controls keep green and red. Every type
// also has its own pattern, so the map reads in grayscale and without color.
PlatePalette screenPalette(const QColor &accent);
PlatePalette printPalette(const QColor &accent);

// Where everything goes for a plate drawn into bounds. The window, its hit
// testing, and the exports all lay the plate out with this one function.
struct PlateLayout {
    QRectF body;   // The plate itself, headers included.
    QRectF grid;   // The wells.
    double pitch = 0;
    bool labels = false; // Room under each well for its label and concentration.
    int rows = 0, columns = 0;

    QRectF cell(int row, int column) const;
    QRectF wellRect(int row, int column) const;
    // The well under point as (column, row), or (-1, -1).
    QPoint wellAt(const QPointF &point) const;
};

PlateLayout plateLayout(const QRectF &bounds, const Plate &plate);

// What the window adds over the plate; columns in x and rows in y.
struct PlateMarks {
    QRect selection;
    QPoint cursor{-1, -1};
    QPoint hover{-1, -1};
};

void paintWell(QPainter *painter, const QRectF &rect, WellType type, const PlatePalette &palette);
void paintPlate(QPainter *painter, const PlateLayout &layout, const Plate &plate, const PlatePalette &palette,
                const PlateMarks &marks = {}, double radius = 0);

// The printable map: title, plate, and legend on a landscape A4 page.
QSizeF printPageSize(); // In 96 dpi pixels.
void paintPrintMap(QPainter *painter, const QRectF &page, const Plate &plate, const QString &title,
                   const QColor &accent);

// Both write beside path and rename over it, so a failed export leaves no half file.
bool exportSvg(const Plate &plate, const QString &path, const QString &title, const QColor &accent, QString *error);
bool exportPdf(const Plate &plate, const QString &path, const QString &title, const QColor &accent, QString *error);
bool exportCsv(const Plate &plate, const QString &path, QString *error);
