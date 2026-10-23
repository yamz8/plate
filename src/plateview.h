#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

#include "backend.h"
#include "platepainter.h"

// The plate on screen, painted by the same code as the SVG and PDF exports.
// QML handles the pointer and asks wellAt() which well is under it.
class PlateView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Backend *source READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(QColor accent MEMBER m_accent WRITE setAccent NOTIFY appearanceChanged)
    Q_PROPERTY(int radius MEMBER m_radius WRITE setRadius NOTIFY appearanceChanged)
    Q_PROPERTY(QPoint hover MEMBER m_hover WRITE setHover NOTIFY hoverChanged)

public:
    explicit PlateView(QQuickItem *parent = nullptr);

    Backend *backend() const { return m_backend; }
    void setBackend(Backend *backend);
    void setAccent(const QColor &accent);
    void setRadius(int radius);
    void setHover(const QPoint &hover);

    // The well under (x, y) as a point with the column in x and the row in y,
    // or (-1, -1) between and outside the wells.
    Q_INVOKABLE QPoint wellAt(qreal x, qreal y) const;
    // The well nearest (x, y), for drags that leave the plate.
    Q_INVOKABLE QPoint nearestWell(qreal x, qreal y) const;
    // The center of a well, for placing things beside it.
    Q_INVOKABLE QPointF wellCenter(int row, int column) const;

    void paint(QPainter *painter) override;

signals:
    void backendChanged();
    void appearanceChanged();
    void hoverChanged();

private:
    PlateLayout layout() const;

    QPointer<Backend> m_backend;
    QColor m_accent{"#FFD60A"};
    int m_radius = 0;
    QPoint m_hover{-1, -1};
};

// One type's pattern, for the type list and the shortcut overlay.
class WellSwatch : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(int type MEMBER m_type NOTIFY changed)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY changed)

public:
    explicit WellSwatch(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    int m_type = 0;
    QColor m_accent{"#FFD60A"};
};
