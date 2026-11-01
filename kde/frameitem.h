#pragma once

#include "engine.h"

#include <QImage>
#include <QQuickFramebufferObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

/* Live scene. The still PNG is the offscreen harness. A live window steps
   the player with readback off and blits the color texture on the GPU.
   frameImage readback is only for --watch-diff. */
class FrameItem : public QQuickFramebufferObject {
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
    Q_PROPERTY(qreal videoVolume READ videoVolume WRITE setVideoVolume NOTIFY videoVolumeChanged)
    Q_PROPERTY(bool videoMuted READ videoMuted WRITE setVideoMuted NOTIFY videoMutedChanged)
    Q_PROPERTY(int steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(QImage frameImage READ frameImage)

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
    qreal videoVolume() const { return m_vol; }
    bool videoMuted() const { return m_mute; }
    int steps() const { return m_steps; }
    QImage frameImage() const;
    void setFramePath(const QString& path);
    void setPointerX(qreal x);
    void setPointerY(qreal y);
    void setLive(bool on);
    void setItemId(const QString& id);
    void setMediaDir(const QString& dir);
    void setVideoVolume(qreal volume);
    void setVideoMuted(bool on);
    Renderer* createRenderer() const override;

    Engine* engine() { return &m_engine; }
    const QImage& stillImage() const { return m_still; }
    bool havePointer() const { return m_havePtr; }

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
    void videoVolumeChanged();
    void videoMutedChanged();
    void stepsChanged();

private:
    void ensureTimer();
    void reopen();
    void tick();

    QString m_path;
    QImage m_still;
    qreal m_px = 0;
    qreal m_py = 0;
    bool m_havePtr = false;
    bool m_live = false;
    bool m_pinned = false;
    QString m_item;
    QString m_dir;
    QString m_kind;
    QString m_page;
    QString m_stats;
    QVariantList m_audio;
    qreal m_vol = 1;
    bool m_mute = true;
    Engine m_engine;
    QTimer* m_timer = nullptr;
    int m_frames = 0;
    int m_steps = 0;
    long m_ticks = 0;
    qint64 m_mark = 0;
};
