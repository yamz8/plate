#include "plateview.h"

#include <QPainter>
#include <algorithm>

PlateView::PlateView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
}

void PlateView::setBackend(Backend *backend) {
    if (m_backend == backend)
        return;
    if (m_backend)
        disconnect(m_backend, nullptr, this, nullptr);
    m_backend = backend;
    if (m_backend) {
        connect(m_backend, &Backend::plateChanged, this, [this] { update(); });
        connect(m_backend, &Backend::selectionChanged, this, [this] { update(); });
    }
    emit backendChanged();
    update();
}

void PlateView::setAccent(const QColor &accent) {
    if (m_accent == accent)
        return;
    m_accent = accent;
    emit appearanceChanged();
    update();
}

void PlateView::setRadius(int radius) {
    if (m_radius == radius)
        return;
    m_radius = radius;
    emit appearanceChanged();
    update();
}

void PlateView::setHover(const QPoint &hover) {
    if (m_hover == hover)
        return;
    m_hover = hover;
    emit hoverChanged();
    update();
}

PlateLayout PlateView::layout() const {
    return m_backend ? plateLayout(QRectF(0, 0, width(), height()), m_backend->plate()) : PlateLayout();
}

QPoint PlateView::wellAt(qreal x, qreal y) const {
    return layout().wellAt(QPointF(x, y));
}

QPoint PlateView::nearestWell(qreal x, qreal y) const {
    const PlateLayout l = layout();
    if (l.pitch <= 0)
        return {-1, -1};
    const QPointF inside(std::clamp(x, l.grid.left(), l.grid.right() - 1), std::clamp(y, l.grid.top(), l.grid.bottom() - 1));
    return l.wellAt(inside);
}

QPointF PlateView::wellCenter(int row, int column) const {
    return layout().cell(row, column).center();
}

void PlateView::paint(QPainter *painter) {
    if (!m_backend)
        return;
    PlateMarks marks;
    marks.selection = m_backend->selection();
    marks.cursor = QPoint(m_backend->cursorColumn(), m_backend->cursorRow());
    marks.hover = m_hover;
    paintPlate(painter, layout(), m_backend->plate(), screenPalette(m_accent), marks, m_radius);
}

WellSwatch::WellSwatch(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &WellSwatch::changed, this, [this] { update(); });
}

void WellSwatch::paint(QPainter *painter) {
    const double d = std::min(width(), height()) - 1;
    paintWell(painter, QRectF((width() - d) / 2, (height() - d) / 2, d, d),
              WellType(std::clamp(m_type, 0, WellTypeCount - 1)), screenPalette(m_accent));
}
