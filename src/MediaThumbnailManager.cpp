#include "MediaThumbnailManager.h"
#include <QThread>
#include <QUrl>
#include <QCoreApplication>
#include <QTimer>
#include <QMediaMetaData>

ThumbnailJob::ThumbnailJob(const QString &id, const QString &path, const QString &type)
    : m_id(id), m_path(path), m_type(type) 
{
    setAutoDelete(true);
}

void ThumbnailJob::run() {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/MediaThumbnails";
    QDir().mkpath(cacheDir);

    QFileInfo info(m_path);
    if (!info.exists()) { emit failed(m_id); return; }

    QString hashInput = m_path + QString::number(info.size()) + QString::number(info.lastModified().toMSecsSinceEpoch());
    QString hash = QCryptographicHash::hash(hashInput.toUtf8(), QCryptographicHash::Md5).toHex();
    QString cachePath = cacheDir + "/" + hash + ".jpg";

    if (QFile::exists(cachePath)) {
        emit finished(m_id, cachePath);
        return;
    }

    bool success = false;
    if (m_type == "video") success = extractVideoFrame(m_path, cachePath);
    else if (m_type == "image") success = scaleImage(m_path, cachePath);
    else if (m_type == "audio") success = extractAudioArt(m_path, cachePath);

    if (success) emit finished(m_id, cachePath);
    else emit failed(m_id);
}

bool ThumbnailJob::extractVideoFrame(const QString &input, const QString &output) {
    QString ffmpegPath = "ffmpeg"; // Default
    
    // Search in common Windows paths if default fails or to be sure
    QStringList commonPaths = {
        "ffmpeg.exe",
        "C:/ffmpeg/bin/ffmpeg.exe",
        "C:/Program Files/ffmpeg/bin/ffmpeg.exe",
        "C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe",
        QCoreApplication::applicationDirPath() + "/ffmpeg.exe"
    };

    bool found = false;
    for (const QString &p : commonPaths) {
        QProcess check;
        check.start(p, {"-version"});
        if (check.waitForFinished(500) && check.exitCode() == 0) {
            ffmpegPath = p;
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "ThumbnailJob: ffmpeg NOT FOUND in PATH or common locations.";
        return false;
    }

    QProcess ffmpeg;
    QStringList args;
    args << "-ss" << "00:00:01" << "-i" << input << "-vframes" << "1" << "-q:v" << "2" << "-s" << "320x180" << "-y" << output;
    
    ffmpeg.start(ffmpegPath, args);
    if (!ffmpeg.waitForFinished(8000)) {
        qWarning() << "ThumbnailJob: ffmpeg timeout for" << input;
        ffmpeg.kill();
        return false;
    }
    
    if (ffmpeg.exitCode() != 0) {
        qWarning() << "ThumbnailJob: ffmpeg failed for" << input << "Error:" << ffmpeg.readAllStandardError();
        return false;
    }
    
    return QFile::exists(output);
}

bool ThumbnailJob::scaleImage(const QString &input, const QString &output) {
    QImageReader reader(input);
    if (!reader.canRead()) return false;
    reader.setScaledSize(reader.size().scaled(320, 180, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (img.isNull()) return false;
    return img.save(output, "JPG", 85);
}

bool ThumbnailJob::extractAudioArt(const QString &input, const QString &output) {
    QProcess ffmpeg;
    QStringList args;
    args << "-i" << input << "-an" << "-vcodec" << "copy" << "-y" << output;
    ffmpeg.start("ffmpeg", args);
    if (!ffmpeg.waitForFinished(3000)) { ffmpeg.kill(); return false; }
    return ffmpeg.exitCode() == 0 && QFile::exists(output);
}

MediaThumbnailManager::MediaThumbnailManager(QObject *parent) : QObject(parent) {
    m_pool.setMaxThreadCount(qMax(1, QThread::idealThreadCount() - 1));
    m_native = new NativeThumbnailGenerator(this);
    connect(m_native, &NativeThumbnailGenerator::finished, this, &MediaThumbnailManager::thumbnailReady);
    connect(m_native, &NativeThumbnailGenerator::failed, this, &MediaThumbnailManager::thumbnailFailed);

    // Check for ffmpeg once
    QProcess check;
    check.start("ffmpeg", {"-version"});
    m_hasFfmpeg = check.waitForFinished(500) && check.exitCode() == 0;
    if (!m_hasFfmpeg) {
        QStringList extra = {"C:/ffmpeg/bin/ffmpeg.exe", "C:/Program Files/ffmpeg/bin/ffmpeg.exe"};
        for (const QString &p : extra) {
            if (QFile::exists(p)) { m_hasFfmpeg = true; break; }
        }
    }
}

void MediaThumbnailManager::enqueue(const QString &id, const QString &path, const QString &type) {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/MediaThumbnails";
    QDir().mkpath(cacheDir);
    QFileInfo info(path);
    QString hashInput = path + QString::number(info.size()) + QString::number(info.lastModified().toMSecsSinceEpoch());
    QString hash = QCryptographicHash::hash(hashInput.toUtf8(), QCryptographicHash::Md5).toHex();
    QString cachePath = cacheDir + "/" + hash + ".jpg";

    if (QFile::exists(cachePath)) {
        emit thumbnailReady(id, cachePath);
        return;
    }

    if (type == "video" && !m_hasFfmpeg) {
        m_native->enqueue(id, path, cachePath);
    } else {
        auto job = new ThumbnailJob(id, path, type);
        connect(job, &ThumbnailJob::finished, this, [this](const QString &id, const QString &path){
            emit thumbnailReady(id, path, ""); // No title from ThumbnailJob (ffmpeg) yet
        }, Qt::QueuedConnection);
        connect(job, &ThumbnailJob::failed, this, &MediaThumbnailManager::thumbnailFailed, Qt::QueuedConnection);
        m_pool.start(job);
    }
}

// Native Generator Implementation
NativeThumbnailGenerator::NativeThumbnailGenerator(QObject *parent) : QObject(parent) {
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoOutput(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &NativeThumbnailGenerator::onFrameChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &NativeThumbnailGenerator::onStatusChanged);
}

void NativeThumbnailGenerator::enqueue(const QString &id, const QString &path, const QString &cachePath) {
    m_queue.append({id, path, cachePath});
    if (!m_processing) processNext();
}

void NativeThumbnailGenerator::processNext() {
    if (m_queue.isEmpty()) { m_processing = false; return; }
    m_processing = true;
    m_current = m_queue.takeFirst();
    
    qDebug() << "NativeThumbnailGenerator: Processing" << m_current.path;
    m_player->setSource(QUrl::fromLocalFile(m_current.path));
    m_player->play(); 
    
    // Safety timeout
    QTimer::singleShot(8000, this, [this](){
        if (m_processing) {
            qWarning() << "NativeThumbnailGenerator: Timeout/Failed for" << m_current.path;
            m_processing = false;
            m_player->stop();
            processNext();
        }
    });
}

void NativeThumbnailGenerator::onFrameChanged(const QVideoFrame &frame) {
    if (!m_processing) return;
    
    // Only accept frames once we've reached at least 115 seconds
    if (m_player->position() < 115000) {
        return; 
    }

    if (frame.isValid()) {
        QImage img = frame.toImage();
        if (!img.isNull()) {
            img = img.scaled(640, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (img.save(m_current.cachePath, "JPG", 90)) {
                QString title = m_player->metaData().value(QMediaMetaData::Title).toString();
                qDebug() << "NativeThumbnailGenerator: Success for" << m_current.id << "Title:" << title;
                emit finished(m_current.id, m_current.cachePath, title);
                m_processing = false;
                m_player->stop();
                QTimer::singleShot(100, this, &NativeThumbnailGenerator::processNext);
            }
        }
    }
}

void NativeThumbnailGenerator::onStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::LoadedMedia) {
        m_player->setPosition(120000); // 120 seconds in
    }
}
