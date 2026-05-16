#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QFuture>
#include <QSize>
#include <memory>

class SharedMemoryWriter;

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
    void processFrame(const QByteArray &rgbaData, const QSize &size);

    QPointer<QQuickWindow> m_window;
    bool m_isBroadcasting = false;
    
    // OpenGL PBOs for async readback
    QOpenGLBuffer m_pbo[2];
    int m_pboIndex = 0;
    bool m_hasPendingRead = false;
    QSize m_lastSize;
    
    std::unique_ptr<SharedMemoryWriter> m_writer;
    QFuture<void> m_processFuture;
};
