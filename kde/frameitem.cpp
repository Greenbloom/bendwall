#include "frameitem.h"

#include "props.h"

#include <QDateTime>
#include <QQuickWindow>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QtQuick/qquickopenglutils.h>

namespace {

class FrameRenderer : public QQuickFramebufferObject::Renderer {
public:
    void synchronize(QQuickFramebufferObject* item) override {
        auto* frame = static_cast<FrameItem*>(item);
        m_live = frame->live();
        m_still = frame->stillImage();
        m_engine = frame->engine();
    }

    void render() override {
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
        if (gl) {
            gl->glDisable(GL_DEPTH_TEST);
            gl->glDisable(GL_BLEND);
            gl->glClearColor(18.f / 255.f, 20.f / 255.f, 24.f / 255.f, 1.f);
            gl->glClear(GL_COLOR_BUFFER_BIT);
        }
        if (m_live && m_engine && m_engine->blit()) {
            if (ctx) QQuickOpenGLUtils::resetOpenGLState();
            return;
        }
        drawImage(m_live && m_engine ? m_engine->image() : m_still);
        if (ctx) QQuickOpenGLUtils::resetOpenGLState();
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        QSize s = size;
        if (s.width() < 1) s.setWidth(1);
        if (s.height() < 1) s.setHeight(1);
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        return new QOpenGLFramebufferObject(s, fmt);
    }

private:
    void drawImage(const QImage& img) {
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx || img.isNull()) return;
        QOpenGLFunctions* gl = ctx->functions();
        if (!m_prog) {
            m_prog = new QOpenGLShaderProgram();
            m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex,
                "#version 330 core\n"
                "layout(location=0) in vec2 a;\n"
                "layout(location=1) in vec2 t;\n"
                "out vec2 v;\n"
                "void main(){ v = t; gl_Position = vec4(a, 0.0, 1.0); }\n");
            m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment,
                "#version 330 core\n"
                "in vec2 v;\n"
                "uniform sampler2D tex;\n"
                "out vec4 frag;\n"
                "void main(){ frag = texture(tex, v); }\n");
            m_prog->link();
            const float quad[16] = {
                -1.f, -1.f, 0.f, 0.f,
                 1.f, -1.f, 1.f, 0.f,
                -1.f,  1.f, 0.f, 1.f,
                 1.f,  1.f, 1.f, 1.f
            };
            m_buf.create();
            m_buf.bind();
            m_buf.allocate(quad, sizeof quad);
            m_buf.release();
        }
        QImage rgba = img.convertToFormat(QImage::Format_RGBA8888).mirrored();
        if (m_tex == 0) gl->glGenTextures(1, &m_tex);
        gl->glBindTexture(GL_TEXTURE_2D, m_tex);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
        m_prog->bind();
        m_prog->setUniformValue("tex", 0);
        m_buf.bind();
        m_prog->enableAttributeArray(0);
        m_prog->enableAttributeArray(1);
        m_prog->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * (int)sizeof(float));
        m_prog->setAttributeBuffer(1, GL_FLOAT, 2 * (int)sizeof(float), 2, 4 * (int)sizeof(float));
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_buf.release();
        m_prog->release();
        gl->glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool m_live = false;
    Engine* m_engine = nullptr;
    QImage m_still;
    QOpenGLShaderProgram* m_prog = nullptr;
    QOpenGLBuffer m_buf{QOpenGLBuffer::VertexBuffer};
    GLuint m_tex = 0;
};

}  // namespace

FrameItem::FrameItem(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    setMirrorVertically(true);
    setFlag(ItemHasContents, true);
}

QImage FrameItem::frameImage() const {
    if (!m_live) return m_still;
    return m_engine.image();
}

void FrameItem::setFramePath(const QString& path) {
    if (m_path == path) return;
    m_path = path;
    m_still = QImage(path);
    emit framePathChanged();
    update();
}

void FrameItem::setPointerX(qreal x) {
    if (m_px == x && m_havePtr) return;
    m_px = x;
    m_havePtr = true;
    emit pointerChanged();
}

void FrameItem::setPointerY(qreal y) {
    if (m_py == y && m_havePtr) return;
    m_py = y;
    m_havePtr = true;
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

void FrameItem::setVideoVolume(qreal volume) {
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    if (qAbs(m_vol - volume) < 0.0001) return;
    m_vol = volume;
    m_engine.setVideoVolume((float)m_vol);
    emit videoVolumeChanged();
}

void FrameItem::setVideoMuted(bool on) {
    if (m_mute == on) return;
    m_mute = on;
    m_engine.setVideoMute(m_mute);
    emit videoMutedChanged();
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
    m_steps = 0;
}

QQuickFramebufferObject::Renderer* FrameItem::createRenderer() const {
    return new FrameRenderer();
}

void FrameItem::tick() {
    if (!m_live) return;
    int w = (int)width();
    int h = (int)height();
    /* Anchors can leave the item at 0×0 until a layout pass. The window is
       already the wallpaper size, and a 0×0 item used to skip every tick,
       so the clock never moved. */
    if ((w < 8 || h < 8) && window()) {
        if (w < 8) w = (int)window()->width();
        if (h < 8) h = (int)window()->height();
    }
    if (w < 8 || h < 8) return;
    m_engine.setVideoVolume((float)m_vol);
    m_engine.setVideoMute(m_mute);
    m_engine.setReadback(false);
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
    m_engine.setPointer((float)m_px, (float)m_py, (float)w, (float)h, m_havePtr);
    if (m_engine.step(33)) {
        m_steps++;
        emit stepsChanged();
    }
    m_audio = m_engine.audio();
    emit audioSamplesChanged();
    m_frames++;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_mark >= 500) {
        const long ticks = bendwallProcTicks();
        const double fps = m_frames * 1000.0 / (double)(now - m_mark);
        const double cpu = bendwallCpuPercent(m_ticks, ticks, now - m_mark);
        int sim = 0, glus = 0, rd = 0;
        m_engine.profile(&sim, &glus, &rd);
        m_stats = QString("%1 fps   %2% cpu   sim %3  gl %4  read %5  %6")
                      .arg(bendwallFixed1(fps))
                      .arg(bendwallFixed1(cpu))
                      .arg(bendwallFixed1(sim / 1000.0))
                      .arg(bendwallFixed1(glus / 1000.0))
                      .arg(bendwallFixed1(rd / 1000.0))
                      .arg(m_kind);
        m_frames = 0;
        m_ticks = ticks;
        m_mark = now;
        emit statsChanged();
    }
    update();
}
