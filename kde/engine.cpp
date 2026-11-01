#include "engine.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

namespace {

struct Api {
    void* so = nullptr;
    int (*open)(const char*, int, int) = nullptr;
    void (*audio)(const float*, int) = nullptr;
    void (*video_volume)(float) = nullptr;
    void (*video_mute)(int) = nullptr;
    int (*step)(int) = nullptr;
    int (*rgba)(unsigned char*, int) = nullptr;
    int (*size)(int*, int*) = nullptr;
    const char* (*line)() = nullptr;
    const char* (*kind)() = nullptr;
    const char* (*page)() = nullptr;
    int (*audio_out)(float*) = nullptr;
    void (*close)() = nullptr;
    QString err;
    bool tried = false;
};

template <typename T>
T sym(void* so, const char* name) {
    return reinterpret_cast<T>(dlsym(so, name));
}

Api& api() {
    static Api a;
    if (a.tried) return a;
    a.tried = true;
    QStringList paths;
    const QByteArray env = qgetenv("BENDWALL_NATIVE");
    if (!env.isEmpty()) paths << QString::fromLocal8Bit(env);
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&api), &info) && info.dli_fname) {
        const QDir dir = QFileInfo(QString::fromUtf8(info.dli_fname)).absoluteDir();
        paths << dir.filePath("libbendwall.so");
    }
    const QString app = QCoreApplication::applicationDirPath();
    paths << (app + "/libbendwall.so");
    paths << (app + "/../../host/libbendwall.so");
    paths << QStringLiteral("host/libbendwall.so");
    paths << QStringLiteral("libbendwall.so");
    QString tried;
    for (const QString& path : paths) {
        if (path.isEmpty()) continue;
        if (path != QStringLiteral("libbendwall.so") && path != QStringLiteral("host/libbendwall.so")
            && !QFile::exists(path))
            continue;
        tried += path + "\n";
        a.so = dlopen(path.toLocal8Bit().constData(), RTLD_NOW);
        if (a.so) break;
    }
    if (!a.so) {
        a.err = QString("dlopen failed\n%1%2").arg(tried, QString::fromLocal8Bit(dlerror() ? dlerror() : ""));
        return a;
    }
    a.open = sym<int (*)(const char*, int, int)>(a.so, "bw_play_open");
    a.audio = sym<void (*)(const float*, int)>(a.so, "bw_play_audio");
    a.video_volume = sym<void (*)(float)>(a.so, "bw_play_set_video_volume");
    a.video_mute = sym<void (*)(int)>(a.so, "bw_play_set_video_mute");
    a.step = sym<int (*)(int)>(a.so, "bw_play_step");
    a.rgba = sym<int (*)(unsigned char*, int)>(a.so, "bw_play_rgba");
    a.size = sym<int (*)(int*, int*)>(a.so, "bw_play_size");
    a.line = sym<const char* (*)()>(a.so, "bw_play_line");
    a.kind = sym<const char* (*)()>(a.so, "bw_play_kind");
    a.page = sym<const char* (*)()>(a.so, "bw_play_page");
    a.audio_out = sym<int (*)(float*)>(a.so, "bw_play_audio_out");
    a.close = sym<void (*)()>(a.so, "bw_play_close");
    if (!a.open || !a.step || !a.rgba || !a.size || !a.line || !a.kind || !a.close) {
        a.err = QStringLiteral("libbendwall.so is missing the player");
        a.open = nullptr;
    }
    return a;
}

}  // namespace

long bendwallProcTicks() {
    FILE* f = fopen("/proc/self/stat", "r");
    if (!f) return 0;
    int c;
    while ((c = fgetc(f)) != EOF && c != ')') {}
    char state;
    unsigned long ppid, pgrp, session, tty, tpgid, flags;
    unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;
    const int n = fscanf(f, " %c %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                         &state, &ppid, &pgrp, &session, &tty, &tpgid, &flags,
                         &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime);
    fclose(f);
    if (n != 13) return 0;
    return (long)(utime + stime);
}

double bendwallCpuPercent(long ticks0, long ticks1, long elapsedMs) {
    const long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0 || elapsedMs <= 0 || ticks1 < ticks0) return 0;
    return (double)(ticks1 - ticks0) / (double)clk / ((double)elapsedMs / 1000.0) * 100.0;
}

QString bendwallFixed1(double v) {
    char buf[64];
    int neg = 0;
    long scaled;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    scaled = (long)(v * 10.0 + 0.5);
    snprintf(buf, sizeof buf, "%s%ld.%ld", neg ? "-" : "", scaled / 10, scaled % 10);
    return QString::fromLatin1(buf);
}

bool Engine::load() {
    Api& a = api();
    m_err = a.err;
    return a.open != nullptr;
}

bool Engine::open(const QString& dir, int w, int h) {
    if (!load()) return false;
    Api& a = api();
    const QByteArray utf = dir.toUtf8();
    m_ready = a.open(utf.constData(), w, h) == 1;
    if (!m_ready) m_err = QString::fromUtf8(a.line ? a.line() : "open failed");
    return m_ready;
}

void Engine::close() {
    Api& a = api();
    if (a.close) a.close();
    m_ready = false;
}

bool Engine::step(int dtMs) {
    Api& a = api();
    if (!a.step) return false;
    m_ready = a.step(dtMs) == 1;
    return m_ready;
}

void Engine::setVideoVolume(float volume) {
    Api& a = api();
    if (volume < 0.f) volume = 0.f;
    if (volume > 1.f) volume = 1.f;
    if (a.video_volume) a.video_volume(volume);
}

void Engine::setVideoMute(bool on) {
    Api& a = api();
    if (a.video_mute) a.video_mute(on ? 1 : 0);
}

void Engine::setAudio(const QVector<float>& interleaved) {
    Api& a = api();
    if (!a.audio) return;
    if (interleaved.isEmpty()) a.audio(nullptr, 0);
    else a.audio(interleaved.constData(), interleaved.size());
}

QImage Engine::image() const {
    Api& a = api();
    int w = 0, h = 0;
    if (!a.size || !a.rgba || !a.size(&w, &h) || w < 1 || h < 1) return QImage();
    QByteArray buf(w * h * 4, 0);
    if (a.rgba(reinterpret_cast<unsigned char*>(buf.data()), buf.size()) < w * h * 4) return QImage();
    const QImage src(reinterpret_cast<const uchar*>(buf.constData()), w, h, w * 4, QImage::Format_RGBA8888);
    return src.copy();
}

QString Engine::line() const {
    Api& a = api();
    return a.line ? QString::fromUtf8(a.line()) : QString();
}

QString Engine::kind() const {
    Api& a = api();
    return a.kind ? QString::fromUtf8(a.kind()) : QString();
}

QString Engine::page() const {
    Api& a = api();
    if (!a.page) return QString();
    const QString path = QString::fromUtf8(a.page());
    if (path.isEmpty()) return QString();
    return QUrl::fromLocalFile(path).toString();
}

QVariantList Engine::audio() const {
    QVariantList out;
    Api& a = api();
    float s[128];
    if (!a.audio_out || !a.audio_out(s)) return out;
    for (int i = 0; i < 128; i++) out << (double)s[i];
    return out;
}
