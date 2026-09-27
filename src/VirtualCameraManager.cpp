#include "VirtualCameraManager.h"
#include "UnityCaptureWriter.h"
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QImage>

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

    m_writer = std::make_unique<UnityCaptureWriter>();

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

    m_writer.reset();
    
    if (m_pbo[0].isCreated()) m_pbo[0].destroy();
    if (m_pbo[1].isCreated()) m_pbo[1].destroy();
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

    // UnityCapture's FORMAT_UINT8 is DXGI_FORMAT_R8G8B8A8_UNORM — the exact
    // same byte layout as QImage::Format_RGBA8888, so the captured frame can
    // be sent as-is: no YUV/NV12 conversion, no channel reordering.
    QImage img(reinterpret_cast<const uchar *>(rgbaData.data()), size.width(), size.height(), QImage::Format_RGBA8888);
    QImage flipped = img.mirrored(false, true); // OpenGL readback is bottom-up

    // Format_RGBA8888 is always 4 bytes/pixel, so Qt never pads rows for it —
    // constBits() is safe to treat as tightly packed (bytesPerLine == width*4),
    // matching the tightly-packed layout sendFrame() expects.
    m_writer->sendFrame(flipped.width(), flipped.height(), flipped.constBits());
}
