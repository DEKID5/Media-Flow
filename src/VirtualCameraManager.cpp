#include "VirtualCameraManager.h"
#include "SharedMemoryWriter.h"
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QImage>
#include <algorithm>

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
    
    // Target OBS VirtualCam name and size (1920x1080 NV12)
    size_t nv12Size = 1920 * 1080 * 3 / 2;
    m_writer = std::make_unique<SharedMemoryWriter>(L"OBSVirtualCam_Texture1", nv12Size);

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
    if (!m_writer || !m_writer->IsValid()) return;

    QImage img(reinterpret_cast<const uchar*>(rgbaData.data()), size.width(), size.height(), QImage::Format_RGBA8888);
    QImage flipped = img.mirrored(false, true); // OpenGL is bottom-up
    QImage resized;

    if (flipped.width() != 1920 || flipped.height() != 1080) {
        resized = flipped.scaled(1920, 1080, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        resized = flipped;
    }

    // Convert to NV12
    int targetW = 1920;
    int targetH = 1080;
    size_t nv12Size = targetW * targetH * 3 / 2;
    QByteArray nv12Data(nv12Size, 0);

    uint8_t* yPlane = (uint8_t*)nv12Data.data();
    uint8_t* uvPlane = yPlane + (targetW * targetH);

    for (int j = 0; j < targetH; ++j) {
        const QRgb* scanLine = (const QRgb*)resized.constScanLine(j);
        for (int i = 0; i < targetW; ++i) {
            QRgb pixel = scanLine[i];
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[j * targetW + i] = (uint8_t)std::clamp(y, 0, 255);

            if (j % 2 == 0 && i % 2 == 0) {
                int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

                int uvIdx = (j / 2) * targetW + i;
                uvPlane[uvIdx] = (uint8_t)std::clamp(u, 0, 255);
                uvPlane[uvIdx + 1] = (uint8_t)std::clamp(v, 0, 255);
            }
        }
    }

    // Write buffer straight to shared memory IPC
    m_writer->Write(nv12Data.constData(), nv12Size);
}
