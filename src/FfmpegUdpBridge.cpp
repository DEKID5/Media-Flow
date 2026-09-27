#include "FfmpegUdpBridge.h"

#include <QFileInfo>

FfmpegUdpBridge::FfmpegUdpBridge(QObject *parent)
    : QObject(parent)
{
    const QString env = qEnvironmentVariable("MEDIAFLOW_FFMPEG");
    if (!env.isEmpty())
        m_ffmpegPath = env;
}

void FfmpegUdpBridge::setUdpUrl(const QString &url)
{
    if (m_udpUrl == url)
        return;
    m_udpUrl = url;
    emit udpUrlChanged();
}

void FfmpegUdpBridge::setFfmpegPath(const QString &path)
{
    if (m_ffmpegPath == path)
        return;
    m_ffmpegPath = path;
    emit ffmpegPathChanged();
}

void FfmpegUdpBridge::startWithVideoFile(const QString &filePath)
{
    stop();

    QFileInfo fi(filePath);
    if (!fi.isFile()) {
        m_status = tr("Bridge: file not found.");
        emit statusMessageChanged();
        return;
    }

    const QString ext = fi.suffix().toLower();
    static const QStringList videoExt{QStringLiteral("mp4"), QStringLiteral("m4v")};
    if (!videoExt.contains(ext)) {
        m_status = tr("Bridge: only MP4/M4V is supported for UDP streaming. Use OBS for other formats.");
        emit statusMessageChanged();
        return;
    }

    const QString exe = m_ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : m_ffmpegPath;

    auto *proc = new QProcess(this);
    m_proc = proc;

    connect(proc, &QProcess::errorOccurred, this, &FfmpegUdpBridge::onProcError);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &FfmpegUdpBridge::onFinished);
    connect(proc, &QProcess::readyReadStandardOutput, this, &FfmpegUdpBridge::onReadyRead);

    QStringList args;
    args << QStringLiteral("-re") << QStringLiteral("-i") << fi.absoluteFilePath() << QStringLiteral("-c")
         << QStringLiteral("copy") << QStringLiteral("-f") << QStringLiteral("mpegts") << m_udpUrl;

    proc->setProgram(exe);
    proc->setArguments(args);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    m_status = tr("Bridge: starting ffmpeg → %1").arg(m_udpUrl);
    emit statusMessageChanged();
    emit runningChanged();

    proc->start();
    if (!proc->waitForStarted(3000)) {
        m_status = tr("Bridge: ffmpeg failed to start.");
        emit statusMessageChanged();
        proc->deleteLater();
        m_proc = nullptr;
        emit runningChanged();
    }
}

void FfmpegUdpBridge::startWithCamera(const QString &deviceName)
{
    stop();
    const QString exe = m_ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : m_ffmpegPath;
    auto *proc = new QProcess(this);
    m_proc = proc;
    connect(proc, &QProcess::errorOccurred, this, &FfmpegUdpBridge::onProcError);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FfmpegUdpBridge::onFinished);
    connect(proc, &QProcess::readyReadStandardOutput, this, &FfmpegUdpBridge::onReadyRead);

    QStringList args;
    args << QStringLiteral("-f") << QStringLiteral("dshow") << QStringLiteral("-i") << QStringLiteral("video=%1").arg(deviceName)
         << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-preset") << QStringLiteral("ultrafast")
         << QStringLiteral("-tune") << QStringLiteral("zerolatency") << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-f") << QStringLiteral("mpegts") << m_udpUrl;

    proc->setProgram(exe);
    proc->setArguments(args);
    m_status = tr("Bridge: starting webcam → %1").arg(m_udpUrl);
    emit statusMessageChanged();
    emit runningChanged();
    proc->start();
}

void FfmpegUdpBridge::startWithImage(const QString &filePath)
{
    stop();
    const QString exe = m_ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : m_ffmpegPath;
    auto *proc = new QProcess(this);
    m_proc = proc;
    connect(proc, &QProcess::errorOccurred, this, &FfmpegUdpBridge::onProcError);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FfmpegUdpBridge::onFinished);
    connect(proc, &QProcess::readyReadStandardOutput, this, &FfmpegUdpBridge::onReadyRead);

    QStringList args;
    args << QStringLiteral("-loop") << QStringLiteral("1") << QStringLiteral("-i") << filePath
         << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-preset") << QStringLiteral("ultrafast")
         << QStringLiteral("-tune") << QStringLiteral("zerolatency") << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-f") << QStringLiteral("mpegts") << m_udpUrl;

    proc->setProgram(exe);
    proc->setArguments(args);
    m_status = tr("Bridge: starting image → %1").arg(m_udpUrl);
    emit statusMessageChanged();
    emit runningChanged();
    proc->start();
}

void FfmpegUdpBridge::stop()
{
    if (!m_proc)
        return;
    m_proc->kill();
    m_proc->waitForFinished(3000);
    m_proc->deleteLater();
    m_proc = nullptr;
    m_status = tr("Bridge: stopped.");
    emit statusMessageChanged();
    emit runningChanged();
}

void FfmpegUdpBridge::onProcError(QProcess::ProcessError)
{
    if (m_proc) {
        m_status = tr("Bridge process error: %1").arg(m_proc->errorString());
        emit statusMessageChanged();
    }
}

void FfmpegUdpBridge::onFinished(int code, QProcess::ExitStatus st)
{
    Q_UNUSED(st);
    if (m_proc == sender()) {
        m_status = tr("Bridge exited (code %1).").arg(code);
        emit statusMessageChanged();
        m_proc = nullptr;
        emit runningChanged();
    }
}

void FfmpegUdpBridge::onReadyRead()
{
    auto *p = qobject_cast<QProcess *>(sender());
    if (!p)
        return;
    const QByteArray chunk = p->readAll();
    if (!chunk.trimmed().isEmpty()) {
        m_status = QString::fromUtf8(chunk).trimmed().left(500);
        emit statusMessageChanged();
    }
}
