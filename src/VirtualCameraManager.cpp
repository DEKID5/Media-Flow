#include "VirtualCameraManager.h"
#include "ObsVirtualCamWriter.h"
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QImage>
#include <QThread>
#include <QMutexLocker>
#include <QDebug>
#include <algorithm>
#include <cstring>

namespace {
// The OBS shared-memory queue's buffer size is fixed at creation time, so
// we normalize every captured frame to this resolution regardless of the
// source window's actual pixel size (matches VirtualCameraWindow.qml's own
// fixed 1920x1080, but stays correct even if that ever changes).
constexpr int kTargetWidth = 1920;
constexpr int kTargetHeight = 1080;
constexpr double kTargetFps = 30.0;
constexpr double kFrameIntervalMs = 1000.0 / kTargetFps;
// Render ticks (e.g. 60 Hz vsync) rarely land exactly on the capture
// deadline; without a little slack 33.29ms-vs-33.33ms misses would skip to the
// following tick and quietly halve the frame rate.
constexpr double kPacingSlackMs = 2.0;
} // namespace

VirtualCameraManager::VirtualCameraManager(QObject *parent)
    : QObject(parent), m_pbo{QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer), QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer)}
{
}

VirtualCameraManager::~VirtualCameraManager()
{
    stop();
}

bool VirtualCameraManager::isBroadcasting() const
{
    return m_isBroadcasting;
}

void VirtualCameraManager::start(QQuickWindow *window)
{
    if (m_isBroadcasting || !window) return;

    m_writer = std::make_unique<ObsVirtualCamWriter>();
    if (!m_writer->start(kTargetWidth, kTargetHeight, kTargetFps)) {
        // Most likely cause: OBS Studio's own "Start Virtual Camera" (or
        // another producer) already holds the shared memory -- only one
        // producer at a time is possible, matching OBS's own semantics.
        qWarning() << "VirtualCameraManager: failed to start OBS Virtual Camera producer "
                      "(is OBS Studio's own virtual camera already running?)";
        m_writer.reset();
        return;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_window = window;
        m_pboIndex = 0;
        m_hasPendingRead = false;
        m_glReady = false;
        m_nextCaptureMs = 0.0;
        m_frameClock.start();
        m_isBroadcasting = true;
    }

    connect(m_window.data(), &QQuickWindow::afterRendering, this, &VirtualCameraManager::onAfterRendering, Qt::DirectConnection);
}

void VirtualCameraManager::stop()
{
    if (!m_isBroadcasting) return;

    QFuture<void> pending;
    {
        // Taking the lock waits out any onAfterRendering() already running on
        // the render thread; once m_isBroadcasting is false under it, no
        // further worker can be started, so `pending` is the last one.
        QMutexLocker lock(&m_mutex);
        m_isBroadcasting = false;
        if (m_window)
            disconnect(m_window.data(), &QQuickWindow::afterRendering, this, &VirtualCameraManager::onAfterRendering);
        pending = m_processFuture;

        if (m_pbo[0].isCreated()) m_pbo[0].destroy();
        if (m_pbo[1].isCreated()) m_pbo[1].destroy();

        // Without this, a restart's first onAfterRendering() sees an unchanged
        // window size and skips PBO reallocation (see the m_lastSize check
        // below), leaving the freshly-recreated-but-never-allocated PBOs from
        // just above in place -- glReadPixels into an unallocated PBO silently
        // produces no usable frame, so the feed never resumes after a
        // stop/start cycle even though start() reports success.
        m_lastSize = QSize();
        m_hasPendingRead = false;
        m_glReady = false;
    }

    if (pending.isRunning())
        pending.waitForFinished();

    // Without this, Zoom (or any other consumer) just keeps showing the
    // last frame we ever wrote -- the shared-memory ring buffer has no
    // "producer went away" signal of its own beyond the state field, and
    // a consumer that doesn't poll that closely enough simply freezes on
    // stale video instead of visibly going black/off. Writing one last
    // black frame before tearing the writer down means whatever's still
    // reading at least shows black, as close to "the camera turned off"
    // as a producer-side close can make it look.
    if (m_writer) {
        const int frameSize = kTargetWidth * kTargetHeight * 3 / 2;
        QByteArray black(frameSize, '\0');
        // NV12 black: Y=16 (limited-range black), U=V=128 (neutral chroma) --
        // all-zero would decode as full-black Y but also zeroed (wrong/
        // saturated) chroma, which some renderers show as a green tint.
        std::fill_n(reinterpret_cast<uint8_t *>(black.data()), kTargetWidth * kTargetHeight, uint8_t{16});
        std::fill_n(reinterpret_cast<uint8_t *>(black.data()) + kTargetWidth * kTargetHeight,
                    frameSize - kTargetWidth * kTargetHeight, uint8_t{128});
        m_writer->writeFrame(reinterpret_cast<const uint8_t *>(black.constData()));
    }

    m_writer.reset();
}

void VirtualCameraManager::onAfterRendering()
{
    // Held for the whole callback so stop() (GUI thread) can't tear down the
    // PBOs or race the worker start underneath it. The callback is short.
    QMutexLocker lock(&m_mutex);

    if (!m_isBroadcasting || !m_window || !QOpenGLContext::currentContext()) return;

    // Cap capture at kTargetFps. The window may render faster (60 fps video,
    // the 33ms keep-alive timer), and every capture costs a full-frame
    // readback + copy + colour conversion. Returns before touching the PBO
    // ping-pong state so skipped ticks don't disturb it.
    const double now = static_cast<double>(m_frameClock.nsecsElapsed()) / 1.0e6;
    if (now + kPacingSlackMs < m_nextCaptureMs) return;
    m_nextCaptureMs = std::max(m_nextCaptureMs + kFrameIntervalMs, now);

    if (!m_glReady) {
        initializeOpenGLFunctions();
        m_glReady = true;
    }

    if (!m_pbo[0].isCreated()) { m_pbo[0].create(); m_pbo[0].setUsagePattern(QOpenGLBuffer::StreamRead); }
    if (!m_pbo[1].isCreated()) { m_pbo[1].create(); m_pbo[1].setUsagePattern(QOpenGLBuffer::StreamRead); }

    QSize size = m_window->size() * m_window->devicePixelRatio();
    int dataSize = size.width() * size.height() * 4; // GL_RGBA = 4 bytes/pixel

    m_pbo[m_pboIndex].bind();

    if (m_lastSize != size) {
        m_pbo[m_pboIndex].allocate(dataSize);
        m_pbo[1 - m_pboIndex].bind();
        m_pbo[1 - m_pboIndex].allocate(dataSize);
        m_pbo[m_pboIndex].bind();
        m_lastSize = size;
        m_hasPendingRead = false;
    }

    // Async read pixels into PBO
    glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_pbo[m_pboIndex].release();

    int nextIndex = 1 - m_pboIndex;
    // Only map + copy when a worker is free to take the frame; if it's still
    // busy with the previous one, this frame is simply dropped (the PBO gets
    // overwritten by a later read), saving the map and a full-frame memcpy.
    if (m_hasPendingRead && !m_processFuture.isRunning()) {
        m_pbo[nextIndex].bind();
        const GLubyte* ptr = static_cast<const GLubyte*>(m_pbo[nextIndex].map(QOpenGLBuffer::ReadOnly));
        if (ptr) {
            // Copy data off GPU mapped memory quickly (into a reused buffer)
            if (m_frameBuf.size() != dataSize)
                m_frameBuf.resize(dataSize);
            std::memcpy(m_frameBuf.data(), ptr, static_cast<size_t>(dataSize));
            m_pbo[nextIndex].unmap();
            m_frameSize = size;

            // Dispatch to worker thread to avoid stuttering QML render thread
            m_processFuture = QtConcurrent::run(&VirtualCameraManager::processFrame, this);
        }
        m_pbo[nextIndex].release();
    }

    m_hasPendingRead = true;
    m_pboIndex = nextIndex;
}

void VirtualCameraManager::processFrame()
{
    if (!m_writer) return;

    const int w = kTargetWidth;
    const int h = kTargetHeight;

    // The GL readback is bottom-up RGBA8888 (always 4 bytes/pixel, rows tightly
    // packed). Output row j is read from source row (h-1-j), which does the
    // vertical flip for free instead of via a separate mirrored() copy.
    const uchar *base = reinterpret_cast<const uchar *>(m_frameBuf.constData());
    qsizetype stride = static_cast<qsizetype>(m_frameSize.width()) * 4;
    QImage scaled;
    if (m_frameSize.width() != w || m_frameSize.height() != h) {
        // Window isn't exactly 1920x1080 in device pixels (see
        // BroadcastController::openZoomWindow, which normally makes it so).
        // Scaling is orientation-agnostic, so it can happen before the flip.
        const QImage img(base, m_frameSize.width(), m_frameSize.height(), QImage::Format_RGBA8888);
        scaled = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        base = scaled.constBits();
        stride = scaled.bytesPerLine();
    }

    // Convert to NV12 (Y plane followed by interleaved U/V), which is what
    // OBS's shared-memory queue expects. Reads raw scanline bytes directly
    // (R,G,B,A in that byte order for Format_RGBA8888) rather than casting
    // to QRgb* and using qRed/qGreen/qBlue -- those assume ARGB32 packing,
    // not RGBA8888's literal byte layout, which was a real bug in an earlier
    // version of this conversion (silently swapped/misread channels).
    m_nv12.resize(static_cast<size_t>(w) * h * 3 / 2);
    uint8_t *yPlane = m_nv12.data();
    uint8_t *uvPlane = yPlane + static_cast<size_t>(w) * h;

    // j0/j1 are even: each iteration handles two luma rows plus their one
    // chroma row. BT.601 limited range; the integer results stay within
    // [16,235] (Y) and [16,240] (U/V) so no clamping is needed, which also
    // lets the compiler vectorize the luma loop (it has no branches).
    auto convertRows = [&](int j0, int j1) {
        for (int j = j0; j < j1; j += 2) {
            const uchar *r0 = base + static_cast<qsizetype>(h - 1 - j) * stride;
            const uchar *r1 = base + static_cast<qsizetype>(h - 2 - j) * stride;
            uint8_t *y0 = yPlane + static_cast<size_t>(j) * w;
            uint8_t *y1 = y0 + w;

            for (int i = 0; i < w; ++i) {
                const uchar *p0 = r0 + i * 4;
                const uchar *p1 = r1 + i * 4;
                y0[i] = static_cast<uint8_t>(((66 * p0[0] + 129 * p0[1] + 25 * p0[2] + 128) >> 8) + 16);
                y1[i] = static_cast<uint8_t>(((66 * p1[0] + 129 * p1[1] + 25 * p1[2] + 128) >> 8) + 16);
            }

            // Average all 4 pixels in each 2x2 block rather than just
            // sampling the top-left one -- point-sampling aliases color
            // edges (sharp-colored text/logos) more than a proper box
            // filter does.
            uint8_t *uv = uvPlane + static_cast<size_t>(j / 2) * w;
            for (int i = 0; i < w; i += 2) {
                const uchar *a = r0 + i * 4;
                const uchar *b = r1 + i * 4;
                const int rAvg = (a[0] + a[4] + b[0] + b[4] + 2) >> 2;
                const int gAvg = (a[1] + a[5] + b[1] + b[5] + 2) >> 2;
                const int bAvg = (a[2] + a[6] + b[2] + b[6] + 2) >> 2;
                uv[i]     = static_cast<uint8_t>(((-38 * rAvg - 74 * gAvg + 112 * bAvg + 128) >> 8) + 128);
                uv[i + 1] = static_cast<uint8_t>(((112 * rAvg - 94 * gAvg - 18 * bAvg + 128) >> 8) + 128);
            }
        }
    };

    // Split into horizontal bands (each an even number of rows) and convert
    // them in parallel; leaves a core free on small machines.
    const int pairs = h / 2;
    const int bandCount = std::clamp(QThread::idealThreadCount() - 1, 1, 4);
    // Uses QtConcurrent::run (header-only) rather than blockingMap: blockingMap
    // needs Qt6Concurrent.dll at runtime, which this app doesn't deploy.
    auto bandRows = [&](int b) { convertRows((pairs * b / bandCount) * 2, (pairs * (b + 1) / bandCount) * 2); };
    QList<QFuture<void>> running;
    for (int b = 1; b < bandCount; ++b)
        running.append(QtConcurrent::run(bandRows, b));
    bandRows(0); // this worker thread takes the first band itself
    for (QFuture<void> &f : running)
        f.waitForFinished();

    m_writer->writeFrame(m_nv12.data());
}
