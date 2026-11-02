#pragma once

#include <QImage>
#include <QString>
#include <QVariantList>
#include <QVector>

/* dlopen of libbendwall. The wallpaper and the preview share this wrapper.
   The .so is BENDWALL_NATIVE, or the copy installed next to the plugin. */
class Engine {
public:
    bool open(const QString& dir, int w, int h);
    void close();
    bool step(int dtMs);
    void setAudio(const QVector<float>& interleaved);
    /* 0..1. The default the player uses is 1. 0 mutes the soundtrack. */
    void setVideoVolume(float volume);
    void setVideoMute(bool on);
    /* 0 never, -1 duck, 1 force. A missing bw_play_set_video_duck is ignored. */
    void setVideoDuck(int mode);
    /* Share one decoded frame across screens, and do not outrun the display. */
    void setPace(bool on);
    void setRefresh(int hz);
    int paceMs() const;
    /* Live frames skip glReadPixels. Grabs leave this on. */
    void setReadback(bool on);
    void setPointer(float x, float y, float itemW, float itemH, bool valid);
    void profile(int* simUs, int* glUs, int* readUs) const;
    bool blit();
    bool exported() const;
    QImage image() const;
    QString line() const;
    QString kind() const;
    QString page() const;
    QVariantList audio() const;
    bool ready() const { return m_ready; }
    QString error() const { return m_err; }

private:
    bool load();
    bool m_ready = false;
    bool m_hold = false;
    int m_gen = 0;
    QString m_err;
};

long bendwallProcTicks();
double bendwallCpuPercent(long ticks0, long ticks1, long elapsedMs);
/* One decimal and a '.' even when LC_NUMERIC uses a comma. */
QString bendwallFixed1(double v);
