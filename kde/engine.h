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
    /* 0..1. 0 leaves video silent. The player does not open an audio stream. */
    void setVideoVolume(float volume);
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
    QString m_err;
};

long bendwallProcTicks();
double bendwallCpuPercent(long ticks0, long ticks1, long elapsedMs);
/* One decimal and a '.' even when LC_NUMERIC uses a comma. */
QString bendwallFixed1(double v);
