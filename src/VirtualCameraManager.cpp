#include "VirtualCameraManager.h"
#include "ObsVirtualCamWriter.h"
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QImage>
#include <QDebug>
#include <algorithm>

namespace {
// The OBS shared-memory queue's buffer size is fixed at creation time, so
// we normalize every captured frame to this resolution regardless of the
// source window's actual pixel size (matches VirtualCameraWindow.qml's own
// fixed 1920x1080, but stays correct even if that ever changes).
constexpr int kTargetWidth = 1920;
constexpr int kTargetHeight = 1080;
constexpr double kTargetFps = 30.0;
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
    
    m_window = window;
    m_isBroadcasting = true;
    m_pboIndex = 0;
    m_hasPendingRead = false;

    m_writer = std::make_unique<ObsVirtualCamWriter>();
    if (!m_writer->start(kTargetWidth, kTargetHeight, kTargetFps)) {
        // Most likely cause: OBS Studio's own "Start Virtual Camera" (or
        // another producer) already holds the shared memory -- only one
        // producer at a time is possible, matching OBS's own semantics.
        qWarning() << "VirtualCameraManager: failed to start OBS Virtual Camera producer "
                      "(is OBS Studio's own virtual camera already running?)";
        m_writer.reset();
        m_isBroadcasting = false;
        m_window = nullptr;
        return;
    }

    connect(m_window.data(), &QQuickWindow::afterRendering, this, &VirtualCameraManager::onAfterRendering, Qt::DirectConnection);
}

void VirtualCameraManager::stop()
{
    if (!m_isBroadcasting) return;
    m_isBroadcasting = false;
    if (m_window) {
        disconnect(m_window.data(), &QQuickWindow::afterRendering, this, &VirtualCameraManager::onAfterRendering);
    }
    
    if (m_processFuture.isRunning()) {
        m_processFuture.waitForFinished();
    }

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
}

void VirtualCameraManager::onAfterRendering()
{
    if (!m_isBroadcasting || !m_window || !QOpenGLContext::currentContext()) return;

    initializeOpenGLFunctions();

    if (!m_pbo[0].isCreated()) m_pbo[0].create();
    if (!m_pbo[1].isCreated()) m_pbo[1].create();

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
    if (m_hasPendingRead) {
        m_pbo[nextIndex].bind();
        GLubyte* ptr = static_cast<GLubyte*>(m_pbo[nextIndex].map(QOpenGLBuffer::ReadOnly));
        if (ptr) {
            // Copy data off GPU mapped memory quickly
            QByteArray frameData(reinterpret_cast<const char*>(ptr), dataSize);
            m_pbo[nextIndex].unmap();
            
            // Dispatch to worker thread to avoid stuttering QML render thread
            if (!m_processFuture.isRunning()) {
                m_processFuture = QtConcurrent::run(&VirtualCameraManager::processFrame, this, frameData, size);
            }
        }
        m_pbo[nextIndex].release();
    }

    m_hasPendingRead = true;
    m_pboIndex = nextIndex;
}

void VirtualCameraManager::processFrame(const QByteArray &rgbaData, const QSize &size)
{
    if (!m_writer) return;

    QImage img(reinterpret_cast<const uchar *>(rgbaData.data()), size.width(), size.height(), QImage::Format_RGBA8888);
    QImage flipped = img.mirrored(false, true); // OpenGL readback is bottom-up

    QImage frame = flipped;
    if (frame.width() != kTargetWidth || frame.height() != kTargetHeight)
        frame = flipped.scaled(kTargetWidth, kTargetHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Format_RGBA8888 is always 4 bytes/pixel, so Qt never pads rows for it,
    // and .scaled() preserves the format -- safe to treat both as tightly
    // packed (bytesPerLine == width*4).

    // Convert to NV12 (Y plane followed by interleaved U/V), which is what
    // OBS's shared-memory queue expects. Reads raw scanline bytes directly
    // (R,G,B,A in that byte order for Format_RGBA8888) rather than casting
    // to QRgb* and using qRed/qGreen/qBlue -- those assume ARGB32 packing,
    // not RGBA8888's literal byte layout, which was a real bug in an earlier
    // version of this conversion (silently swapped/misread channels).
    const int w = kTargetWidth;
    const int h = kTargetHeight;
    QByteArray nv12(static_cast<int>(w * h * 3 / 2), Qt::Uninitialized);
    auto *y = reinterpret_cast<uint8_t *>(nv12.data());
    uint8_t *uv = y + (w * h);

    for (int j = 0; j < h; ++j) {
        const uchar *line = frame.constScanLine(j);
        for (int i = 0; i < w; ++i) {
            const uchar *px = line + i * 4;
            const int r = px[0], g = px[1], b = px[2];

            const int yVal = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            y[j * w + i] = static_cast<uint8_t>(std::clamp(yVal, 0, 255));

            if ((j % 2) == 0 && (i % 2) == 0) {
                // Average all 4 pixels in this 2x2 block rather than just
                // sampling the top-left one -- point-sampling aliases color
                // edges (sharp-colored text/logos) more than a proper box
                // filter does; w/h are always even (kTargetWidth/Height), so
                // the +1 row/col reads are safe.
                const uchar *lineBelow = frame.constScanLine(j + 1);
                const uchar *pxRight = px + 4;
                const uchar *pxBelow = lineBelow + i * 4;
                const uchar *pxBelowRight = lineBelow + (i + 1) * 4;
                const int rAvg = (r + pxRight[0] + pxBelow[0] + pxBelowRight[0] + 2) / 4;
                const int gAvg = (g + pxRight[1] + pxBelow[1] + pxBelowRight[1] + 2) / 4;
                const int bAvg = (b + pxRight[2] + pxBelow[2] + pxBelowRight[2] + 2) / 4;

                const int uVal = ((-38 * rAvg - 74 * gAvg + 112 * bAvg + 128) >> 8) + 128;
                const int vVal = ((112 * rAvg - 94 * gAvg - 18 * bAvg + 128) >> 8) + 128;
                const int uvIdx = (j / 2) * w + i;
                uv[uvIdx] = static_cast<uint8_t>(std::clamp(uVal, 0, 255));
                uv[uvIdx + 1] = static_cast<uint8_t>(std::clamp(vVal, 0, 255));
            }
        }
    }

    m_writer->writeFrame(reinterpret_cast<const uint8_t *>(nv12.constData()));
}
