#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>

class FfmpegUdpBridge final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString udpUrl READ udpUrl WRITE setUdpUrl NOTIFY udpUrlChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath WRITE setFfmpegPath NOTIFY ffmpegPathChanged)

public:
    explicit FfmpegUdpBridge(QObject *parent = nullptr);

    QString statusMessage() const { return m_status; }
    bool running() const { return m_proc && m_proc->state() != QProcess::NotRunning; }
    QString udpUrl() const { return m_udpUrl; }
    void setUdpUrl(const QString &url);
    QString ffmpegPath() const { return m_ffmpegPath; }
    void setFfmpegPath(const QString &path);

public slots:
    void startWithVideoFile(const QString &filePath);
    void startWithCamera(const QString &deviceName);
    void startWithImage(const QString &filePath);
    void stop();

signals:
    void statusMessageChanged();
    void runningChanged();
    void udpUrlChanged();
    void ffmpegPathChanged();

private slots:
    void onProcError(QProcess::ProcessError);
    void onFinished(int code, QProcess::ExitStatus st);
    void onReadyRead();

private:
    QString m_udpUrl = QStringLiteral("udp://127.0.0.1:6000?pkt_size=1316");
    QString m_ffmpegPath; // empty = try "ffmpeg" on PATH
    QString m_status;
    QPointer<QProcess> m_proc;
};
