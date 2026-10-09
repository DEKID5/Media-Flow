#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QFuture>
#include <QSize>
#include <QElapsedTimer>
#include <QMutex>
#include <QByteArray>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class ObsVirtualCamWriter;

class VirtualCameraManager : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit VirtualCameraManager(QObject *parent = nullptr);
    ~VirtualCameraManager() override;

    bool isBroadcasting() const;

public slots:
    void start(QQuickWindow *window);
    void stop();

private slots:
    void onAfterRendering();

private:
    // Converts m_frameBuf (bottom-up RGBA, m_frameSize) into m_nv12 and
    // writes it to the OBS queue. Runs on a worker thread; only ever started
    // while no other job is running, so it owns m_frameBuf/m_nv12 exclusively.
    void processFrame();

    QPointer<QQuickWindow> m_window;
    std::atomic<bool> m_isBroadcasting{false};

    // Serializes onAfterRendering() (render thread) against stop() (GUI
    // thread): protects the PBOs, m_processFuture and the capture state below,
    // and guarantees no new worker can start once stop() has cleared
    // m_isBroadcasting.
    QMutex m_mutex;

    // OpenGL PBOs for async readback
    QOpenGLBuffer m_pbo[2];
    int m_pboIndex = 0;
    bool m_hasPendingRead = false;
    bool m_glReady = false;
    QSize m_lastSize;

    // Frame pacing (capture is capped at kTargetFps even if the window
    // renders faster, e.g. 60 fps video).
    QElapsedTimer m_frameClock;
    double m_nextCaptureMs = 0.0;

    // Reused across frames to avoid per-frame allocations.
    QByteArray m_frameBuf;
    QSize m_frameSize;
    std::vector<uint8_t> m_nv12;

    std::unique_ptr<ObsVirtualCamWriter> m_writer;
    QFuture<void> m_processFuture;
};
