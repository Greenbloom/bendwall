#pragma once

#include "engine.h"

#include <QImage>
#include <QQuickPaintedItem>
#include <QString>
#include <QTimer>
#include <QVariantList>

/* Paints the live scene image. framePath is the still PNG used by the
   offscreen harness. live draws GL frames instead, with an fps readout. */
class FrameItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString framePath READ framePath WRITE setFramePath NOTIFY framePathChanged)
    Q_PROPERTY(qreal pointerX READ pointerX WRITE setPointerX NOTIFY pointerChanged)
    Q_PROPERTY(qreal pointerY READ pointerY WRITE setPointerY NOTIFY pointerChanged)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QString itemId READ itemId WRITE setItemId NOTIFY itemIdChanged)
    Q_PROPERTY(QString mediaDir READ mediaDir WRITE setMediaDir NOTIFY mediaDirChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY kindChanged)
    Q_PROPERTY(QString pageUrl READ pageUrl NOTIFY pageUrlChanged)
    Q_PROPERTY(QString statsText READ statsText NOTIFY statsChanged)
    Q_PROPERTY(QVariantList audioSamples READ audioSamples NOTIFY audioSamplesChanged)

public:
    explicit FrameItem(QQuickItem* parent = nullptr);
    QString framePath() const { return m_path; }
    qreal pointerX() const { return m_px; }
    qreal pointerY() const { return m_py; }
    bool live() const { return m_live; }
    QString itemId() const { return m_item; }
    QString mediaDir() const { return m_dir; }
    QString kind() const { return m_kind; }
    QString pageUrl() const { return m_page; }
    QString statsText() const { return m_stats; }
    QVariantList audioSamples() const { return m_audio; }
    void setFramePath(const QString& path);
    void setPointerX(qreal x);
    void setPointerY(qreal y);
    void setLive(bool on);
    void setItemId(const QString& id);
    void setMediaDir(const QString& dir);
    void paint(QPainter* painter) override;

signals:
    void framePathChanged();
    void pointerChanged();
    void liveChanged();
    void itemIdChanged();
    void mediaDirChanged();
    void kindChanged();
    void pageUrlChanged();
    void statsChanged();
    void audioSamplesChanged();

private:
    void ensureTimer();
    void reopen();
    void tick();

    QString m_path;
    QImage m_img;
    QImage m_still;
    qreal m_px = 0;
    qreal m_py = 0;
    bool m_live = false;
    bool m_pinned = false;
    QString m_item;
    QString m_dir;
    QString m_kind;
    QString m_page;
    QString m_stats;
    QVariantList m_audio;
    Engine m_engine;
    QTimer* m_timer = nullptr;
    int m_frames = 0;
    long m_ticks = 0;
    qint64 m_mark = 0;
};
