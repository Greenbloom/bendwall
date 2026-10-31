#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QString>

/* Paints the PNG the long-running Bend process wrote. Mouse position is a
   property the QML page forwards; Bend owns how that becomes a uniform. */
class FrameItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString framePath READ framePath WRITE setFramePath NOTIFY framePathChanged)
    Q_PROPERTY(qreal pointerX READ pointerX WRITE setPointerX NOTIFY pointerChanged)
    Q_PROPERTY(qreal pointerY READ pointerY WRITE setPointerY NOTIFY pointerChanged)

public:
    explicit FrameItem(QQuickItem* parent = nullptr);
    QString framePath() const { return m_path; }
    qreal pointerX() const { return m_px; }
    qreal pointerY() const { return m_py; }
    void setFramePath(const QString& path);
    void setPointerX(qreal x);
    void setPointerY(qreal y);
    void paint(QPainter* painter) override;

signals:
    void framePathChanged();
    void pointerChanged();

private:
    QString m_path;
    QImage m_img;
    qreal m_px = 0;
    qreal m_py = 0;
};
