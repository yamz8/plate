#include "platepainter.h"
#include "theme.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSvgGenerator>
#include <algorithm>
#include <cmath>

namespace {
QString tr(const char *text) { return QCoreApplication::translate("Plate", text); }

double hueDistance(double a, double b) {
    const double d = std::fabs(a - b);
    return std::min(d, 360 - d);
}

QColor typeColor(int hue, double saturation, double value) {
    return QColor::fromHsvF(std::fmod(hue + 360.0, 360.0) / 360.0, saturation, value);
}

void fillTypes(PlatePalette &palette, const QColor &accent) {
    const QColor hsv = accent.toHsv();
    // A gray accent has no hue; start the wheel at blue rather than red, which
    // belongs to the negative control.
    const double hue = hsv.hsvHueF() < 0 ? 215 : hsv.hsvHueF() * 360;
    const double saturation = std::clamp<double>(hsv.hsvSaturationF(), 0.45, 0.85);
    const double value = std::clamp<double>(hsv.valueF(), 0.70, palette.print ? 0.85 : 0.95);
    const double positive = 140, negative = 355;
    double standard = std::fmod(hue + 180, 360);
    if (hueDistance(standard, positive) < 40 || hueDistance(standard, negative) < 40)
        standard = std::fmod(hue + 90, 360);

    palette.types[int(WellType::Sample)] = accent;
    palette.types[int(WellType::Standard)] = typeColor(standard, saturation, value);
    palette.types[int(WellType::ControlPositive)] = typeColor(positive, saturation, value);
    palette.types[int(WellType::ControlNegative)] = typeColor(negative, saturation, value);
    palette.types[int(WellType::Blank)] = typeColor(hue, 0.12, palette.print ? 0.55 : 0.80);
    palette.types[int(WellType::Empty)] = palette.grid;
}

// Black or white, whichever reads on color.
QColor ink(const QColor &color) {
    return Theme::isLight(color) ? QColor("#111114") : QColor("#ffffff");
}

QString formatConcentration(double value, const QString &unit) {
    const QString number = QLocale().toString(value, 'g', 4);
    return unit.isEmpty() ? number : number + " " + unit;
}

bool hasText(const Plate &plate) {
    for (int row = 0; row < plate.rows(); ++row)
        for (int column = 0; column < plate.columns(); ++column)
            if (!plate.well(row, column).label.isEmpty() || plate.well(row, column).concentration)
                return true;
    return false;
}
} // namespace

PlatePalette screenPalette(const QColor &accent) {
    PlatePalette palette;
    palette.plate = QColor("#17171b");
    palette.outline = QColor("#2a2a31");
    palette.grid = QColor("#464652");
    palette.text = QColor("#e6e6ea");
    palette.mutedText = QColor("#8b8b96");
    palette.accent = accent;
    fillTypes(palette, accent);
    return palette;
}

PlatePalette printPalette(const QColor &accent) {
    PlatePalette palette;
    palette.print = true;
    palette.plate = QColor("#ffffff");
    palette.outline = QColor("#9a9aa2");
    palette.grid = QColor("#b4b4bc");
    palette.text = QColor("#111114");
    palette.mutedText = QColor("#5c5c66");
    palette.accent = accent;
    fillTypes(palette, accent);
    return palette;
}

QRectF PlateLayout::cell(int row, int column) const {
    return QRectF(grid.left() + column * pitch, grid.top() + row * pitch, pitch, pitch);
}

QRectF PlateLayout::wellRect(int row, int column) const {
    const QRectF c = cell(row, column);
    if (!labels) {
        const double d = pitch * 0.80;
        return QRectF(c.center().x() - d / 2, c.center().y() - d / 2, d, d);
    }
    const double d = pitch * 0.52;
    return QRectF(c.center().x() - d / 2, c.top() + pitch * 0.07, d, d);
}

QPoint PlateLayout::wellAt(const QPointF &point) const {
    if (pitch <= 0 || !grid.contains(point))
        return {-1, -1};
    const int column = std::clamp(int((point.x() - grid.left()) / pitch), 0, columns - 1);
    const int row = std::clamp(int((point.y() - grid.top()) / pitch), 0, rows - 1);
    return {column, row};
}

// The headers take 0.6 of a pitch and the padding 0.4 on each side, so the
// whole plate is (wells + 1.4) pitches across and down.
PlateLayout plateLayout(const QRectF &bounds, const Plate &plate) {
    PlateLayout layout;
    layout.rows = plate.rows();
    layout.columns = plate.columns();
    layout.pitch = std::max(0.0, std::min(bounds.width() / (plate.columns() + 1.4), bounds.height() / (plate.rows() + 1.4)));
    const QSizeF size(layout.pitch * (plate.columns() + 1.4), layout.pitch * (plate.rows() + 1.4));
    layout.body = QRectF(bounds.center().x() - size.width() / 2, bounds.center().y() - size.height() / 2,
                         size.width(), size.height());
    layout.grid = QRectF(layout.body.left() + layout.pitch, layout.body.top() + layout.pitch,
                         layout.pitch * plate.columns(), layout.pitch * plate.rows());
    layout.labels = layout.pitch >= 52 && hasText(plate);
    return layout;
}

void paintWell(QPainter *painter, const QRectF &rect, WellType type, const PlatePalette &palette) {
    const QColor color = palette.types[int(type)];
    const double d = rect.width();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(rect);

    switch (type) {
    case WellType::Sample:
        painter->fillPath(circle, color);
        break;
    case WellType::Standard: {
        QColor wash = color;
        wash.setAlphaF(palette.print ? 0.22 : 0.28);
        painter->fillPath(circle, wash);
        // Each stripe is the chord of the circle along a diagonal, worked out
        // rather than clipped, because SVG output ignores clip paths.
        const double width = std::max(1.0, d * 0.11), radius = d / 2;
        painter->setPen(QPen(color, width, Qt::SolidLine, Qt::FlatCap));
        const QPointF center = rect.center();
        const double s = std::sqrt(0.5);
        for (double offset = -radius * 0.9; offset < radius * 0.95; offset += d * 0.21) {
            const double half = std::sqrt(std::max(0.0, radius * radius - offset * offset));
            const QPointF middle = center + QPointF(offset * s, offset * s);
            painter->drawLine(middle + QPointF(-half * s, half * s), middle + QPointF(half * s, -half * s));
        }
        painter->setPen(QPen(color, std::max(1.0, d * 0.06)));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect.adjusted(d * 0.03, d * 0.03, -d * 0.03, -d * 0.03));
        break;
    }
    case WellType::ControlPositive:
    case WellType::ControlNegative: {
        painter->fillPath(circle, color);
        const double arm = d * 0.27, bar = std::max(1.0, d * 0.13);
        const QPointF c = rect.center();
        painter->setPen(Qt::NoPen);
        painter->setBrush(ink(color));
        painter->drawRect(QRectF(c.x() - arm, c.y() - bar / 2, arm * 2, bar));
        if (type == WellType::ControlPositive)
            painter->drawRect(QRectF(c.x() - bar / 2, c.y() - arm, bar, arm * 2));
        break;
    }
    case WellType::Blank: {
        const double ring = std::max(1.0, d * 0.11);
        painter->setPen(QPen(color, ring));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect.adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2));
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(rect.center(), d * 0.12, d * 0.12);
        break;
    }
    case WellType::Empty:
        painter->setPen(QPen(palette.grid, std::max(1.0, d * 0.035)));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect.adjusted(0.5, 0.5, -0.5, -0.5));
        break;
    }

    // On paper a light color needs an edge to hold its shape.
    if (palette.print && type != WellType::Empty && type != WellType::Blank) {
        painter->setPen(QPen(palette.outline, std::max(0.5, d * 0.025)));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect);
    }
    painter->restore();
}

void paintPlate(QPainter *painter, const PlateLayout &layout, const Plate &plate, const PlatePalette &palette,
                const PlateMarks &marks, double radius) {
    if (layout.pitch <= 0)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);

    painter->setPen(QPen(palette.outline, 1));
    painter->setBrush(palette.plate);
    painter->drawRoundedRect(layout.body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    QFont header = painter->font();
    header.setPixelSize(std::max(7, int(layout.pitch * 0.30)));
    header.setBold(true);
    painter->setFont(header);
    for (int column = 0; column < plate.columns(); ++column) {
        const bool active = marks.selection.isValid() && column >= marks.selection.left() && column <= marks.selection.right();
        painter->setPen(active ? palette.accent : palette.mutedText);
        const QRectF c = layout.cell(0, column);
        painter->drawText(QRectF(c.left(), layout.body.top(), layout.pitch, layout.pitch * 1.05), Qt::AlignCenter,
                          QString::number(column + 1));
    }
    for (int row = 0; row < plate.rows(); ++row) {
        const bool active = marks.selection.isValid() && row >= marks.selection.top() && row <= marks.selection.bottom();
        painter->setPen(active ? palette.accent : palette.mutedText);
        const QRectF c = layout.cell(row, 0);
        painter->drawText(QRectF(layout.body.left(), c.top(), layout.pitch * 1.05, layout.pitch), Qt::AlignCenter,
                          Plate::rowName(row));
    }

    if (marks.selection.isValid()) {
        const QRect s = marks.selection.normalized();
        const QRectF area = layout.cell(s.top(), s.left()).united(layout.cell(s.bottom(), s.right()));
        QColor wash = palette.accent;
        wash.setAlphaF(0.16);
        painter->setPen(QPen(palette.accent, 1.5));
        painter->setBrush(wash);
        const double r = std::min(4.0, radius);
        painter->drawRoundedRect(area.adjusted(1, 1, -1, -1), r, r);
    }

    QFont small = painter->font();
    small.setBold(false);
    small.setPixelSize(std::max(6, int(layout.pitch * 0.15)));
    for (int row = 0; row < plate.rows(); ++row) {
        for (int column = 0; column < plate.columns(); ++column) {
            const Well &well = plate.well(row, column);
            const QRectF rect = layout.wellRect(row, column);
            paintWell(painter, rect, well.type, palette);
            if (!layout.labels)
                continue;
            // The label and concentration go under the well, elided to the cell.
            painter->setFont(small);
            const QFontMetricsF metrics(small);
            const QRectF c = layout.cell(row, column);
            const double width = c.width() - 4, line = metrics.height();
            double y = rect.bottom() + layout.pitch * 0.03;
            if (!well.label.isEmpty()) {
                painter->setPen(palette.text);
                painter->drawText(QRectF(c.left() + 2, y, width, line), Qt::AlignHCenter | Qt::AlignTop,
                                  metrics.elidedText(well.label, Qt::ElideRight, width));
                y += line;
            }
            if (well.concentration) {
                painter->setPen(palette.mutedText);
                painter->drawText(QRectF(c.left() + 2, y, width, line), Qt::AlignHCenter | Qt::AlignTop,
                                  metrics.elidedText(formatConcentration(*well.concentration, plate.unit()),
                                                     Qt::ElideRight, width));
            }
        }
    }

    if (plate.contains(marks.hover.y(), marks.hover.x()) && marks.hover != marks.cursor) {
        const QRectF rect = layout.wellRect(marks.hover.y(), marks.hover.x());
        painter->setPen(QPen(palette.mutedText, 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect.adjusted(-3, -3, 3, 3));
    }
    if (plate.contains(marks.cursor.y(), marks.cursor.x())) {
        const QRectF rect = layout.wellRect(marks.cursor.y(), marks.cursor.x());
        painter->setPen(QPen(palette.accent, std::max(2.0, layout.pitch * 0.05)));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(rect.adjusted(-4, -4, 4, 4));
    }
    painter->restore();
}

QSizeF printPageSize() {
    return QPageSize(QPageSize::A4).rect(QPageSize::Inch).transposed().size() * 96;
}

void paintPrintMap(QPainter *painter, const QRectF &page, const Plate &plate, const QString &title,
                   const QColor &accent) {
    const PlatePalette palette = printPalette(accent);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->fillRect(page, Qt::white);
    const QRectF area = page.adjusted(48, 40, -48, -40);

    QFont heading = painter->font();
    heading.setPixelSize(22);
    heading.setBold(true);
    painter->setFont(heading);
    painter->setPen(palette.text);
    const double headingHeight = QFontMetricsF(heading).height();
    painter->drawText(QRectF(area.left(), area.top(), area.width(), headingHeight), Qt::AlignLeft | Qt::AlignVCenter,
                      title);

    QFont body = painter->font();
    body.setPixelSize(13);
    body.setBold(false);
    painter->setFont(body);
    painter->setPen(palette.mutedText);
    const QFontMetricsF metrics(body);
    QString subtitle = tr("%1-well plate").arg(plate.wellCount());
    if (!plate.unit().isEmpty())
        subtitle += " · " + tr("Concentrations in %1").arg(plate.unit());
    const double subtitleTop = area.top() + headingHeight + 2;
    painter->drawText(QRectF(area.left(), subtitleTop, area.width(), metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      subtitle);

    // The legend runs along the bottom: each type's pattern, name, and count.
    const double legendHeight = 28;
    const double legendTop = area.bottom() - legendHeight;
    double x = area.left();
    for (int i = 0; i < WellTypeCount; ++i) {
        const WellType type = WellType(i);
        const QString text = QString("%1 (%2)").arg(tr(Plate::typeDisplayName(type))).arg(plate.count(type));
        paintWell(painter, QRectF(x, legendTop + 5, 18, 18), type, palette);
        painter->setFont(body);
        painter->setPen(palette.text);
        const double width = metrics.horizontalAdvance(text);
        painter->drawText(QRectF(x + 24, legendTop, width + 2, legendHeight), Qt::AlignLeft | Qt::AlignVCenter, text);
        x += 24 + width + 28;
    }

    const QRectF plateArea(area.left(), subtitleTop + metrics.height() + 16, area.width(),
                           legendTop - 16 - (subtitleTop + metrics.height() + 16));
    // Labels always print when the plate has them, so pick a layout that
    // keeps them even for 384 wells.
    PlateLayout layout = plateLayout(plateArea, plate);
    layout.labels = hasText(plate);
    paintPlate(painter, layout, plate, palette, {}, 10);
    painter->restore();
}

bool exportCsv(const Plate &plate, const QString &path, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(plate.toCsv()) < 0 || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool exportSvg(const Plate &plate, const QString &path, const QString &title, const QColor &accent, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QSizeF size = printPageSize();
    QSvgGenerator generator;
    generator.setOutputDevice(&file);
    generator.setSize(size.toSize());
    generator.setViewBox(QRectF(QPointF(0, 0), size));
    generator.setResolution(96);
    generator.setTitle(title);
    generator.setDescription(QString("%1-well plate map").arg(plate.wellCount()));
    QPainter painter;
    if (!painter.begin(&generator)) {
        if (error)
            *error = "Could not start the SVG.";
        file.cancelWriting();
        return false;
    }
    paintPrintMap(&painter, QRectF(QPointF(0, 0), size), plate, title, accent);
    painter.end();
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool exportPdf(const Plate &plate, const QString &path, const QString &title, const QColor &accent, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    {
        // At 96 dpi the page has the same coordinates as the SVG, so both
        // exports lay out identically.
        QPdfWriter writer(&file);
        writer.setTitle(title);
        writer.setCreator("Plate");
        writer.setResolution(96);
        writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF()));
        QPainter painter;
        if (!painter.begin(&writer)) {
            if (error)
                *error = "Could not start the PDF.";
            file.cancelWriting();
            return false;
        }
        paintPrintMap(&painter, QRectF(QPointF(0, 0), printPageSize()), plate, title, accent);
        painter.end();
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
