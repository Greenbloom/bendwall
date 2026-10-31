#include "frameitem.h"

#include <QPainter>

FrameItem::FrameItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setOpaquePainting(true);
}

void FrameItem::setFramePath(const QString& path) {
    if (m_path == path) return;
    m_path = path;
    m_img = QImage(path);
    emit framePathChanged();
    update();
}

void FrameItem::setPointerX(qreal x) {
    if (m_px == x) return;
    m_px = x;
    emit pointerChanged();
}

void FrameItem::setPointerY(qreal y) {
    if (m_py == y) return;
    m_py = y;
    emit pointerChanged();
}

void FrameItem::paint(QPainter* painter) {
    const QRectF box(0, 0, width(), height());
    if (m_img.isNull()) {
        painter->fillRect(box, QColor(18, 20, 24));
        return;
    }
    painter->drawImage(box, m_img);
}
