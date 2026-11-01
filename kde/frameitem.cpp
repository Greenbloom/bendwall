#include "frameitem.h"

#include "props.h"

#include <QDateTime>
#include <QFont>
#include <QPainter>

FrameItem::FrameItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setRenderTarget(QQuickPaintedItem::Image);
    setOpaquePainting(true);
}

void FrameItem::setFramePath(const QString& path) {
    if (m_path == path) return;
    m_path = path;
    m_still = QImage(path);
    if (!m_live) m_img = m_still;
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

void FrameItem::setLive(bool on) {
    if (m_live == on) return;
    m_live = on;
    if (on) ensureTimer();
    else if (m_timer) m_timer->stop();
    emit liveChanged();
    update();
}

void FrameItem::setItemId(const QString& id) {
    if (m_item == id) return;
    m_item = id;
    emit itemIdChanged();
    if (m_pinned) return;
    m_dir = itemDirectory(id);
    if (m_live) reopen();
}

void FrameItem::setMediaDir(const QString& dir) {
    if (m_pinned && m_dir == dir) return;
    m_dir = dir;
    m_pinned = true;
    emit mediaDirChanged();
    if (m_live) reopen();
}

void FrameItem::ensureTimer() {
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this] { tick(); });
    }
    if (!m_timer->isActive()) {
        m_mark = QDateTime::currentMSecsSinceEpoch();
        m_ticks = bendwallProcTicks();
        m_frames = 0;
        m_timer->start(33);
    }
}

void FrameItem::reopen() {
    m_engine.close();
    m_kind.clear();
    m_page.clear();
}

void FrameItem::tick() {
    if (!m_live) return;
    const int w = (int)width();
    const int h = (int)height();
    if (w < 8 || h < 8) return;
    if (!m_engine.ready()) {
        QString dir = m_dir;
        if (dir.isEmpty()) dir = itemDirectory(m_item);
        if (!m_engine.open(dir, w, h)) {
            m_stats = m_engine.error().isEmpty() ? QStringLiteral("Bendwall engine failed") : m_engine.error();
            emit statsChanged();
            update();
            return;
        }
        m_kind = m_engine.kind();
        m_page = m_engine.page();
        emit kindChanged();
        emit pageUrlChanged();
    }
    m_engine.step(33);
    m_img = m_engine.image();
    m_audio = m_engine.audio();
    emit audioSamplesChanged();
    m_frames++;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_mark >= 500) {
        const long ticks = bendwallProcTicks();
        const double fps = m_frames * 1000.0 / (double)(now - m_mark);
        const double cpu = bendwallCpuPercent(m_ticks, ticks, now - m_mark);
        m_stats = QString("%1 fps   %2% cpu   %3")
                      .arg(bendwallFixed1(fps))
                      .arg(bendwallFixed1(cpu))
                      .arg(m_kind);
        m_frames = 0;
        m_ticks = ticks;
        m_mark = now;
        emit statsChanged();
    }
    update();
}

void FrameItem::paint(QPainter* painter) {
    const QRectF box(0, 0, width(), height());
    const QImage& src = m_live ? m_img : (m_img.isNull() ? m_still : m_img);
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    if (src.isNull()) painter->fillRect(box, QColor(18, 20, 24));
    else painter->drawImage(box, src);
    if (!m_live || m_stats.isEmpty()) return;
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 150));
    painter->drawRoundedRect(QRectF(8, 8, 280, 28), 4, 4);
    painter->setPen(QColor(232, 230, 227));
    QFont font = painter->font();
    font.setPixelSize(13);
    painter->setFont(font);
    painter->drawText(QRectF(16, 8, 264, 28), Qt::AlignVCenter, m_stats);
}
