#include "MediaThumbnailManager.h"
#include <QThread>
#include <QUrl>
#include <QCoreApplication>
#include <QTimer>
#include <QMediaMetaData>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// Every ffmpeg invocation below is a background helper the operator should
// never see -- without this, launching a console executable (ffmpeg.exe)
// from this app pops up its own visible console window with raw ffmpeg
// stderr spam for the whole thumbnail-generation duration, confirmed live
// in production (Windows creates a new console for a child console
// process unless explicitly told not to).
void runHidden(QProcess &process)
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
        args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        args->startupInfo->wShowWindow = SW_HIDE;
    });
#else
    Q_UNUSED(process);
#endif
}

QStringList ffmpegCandidates()
{
    QStringList candidates;
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!fromPath.isEmpty())
        candidates.append(fromPath);

    candidates.append({
        QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe"),
        QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe"),
        QStringLiteral("C:/Program Files/ffmpeg/bin/ffmpeg.exe"),
        QStringLiteral("C:/Program Files (x86)/ffmpeg/bin/ffmpeg.exe")
    });
    candidates.removeDuplicates();
    return candidates;
}

QString availableFfmpeg()
{
    for (const QString &candidate : ffmpegCandidates()) {
        QProcess check;
        runHidden(check);
        check.start(candidate, {QStringLiteral("-version")});
        if (check.waitForStarted(500) && check.waitForFinished(1500)
            && check.exitStatus() == QProcess::NormalExit && check.exitCode() == 0) {
            return candidate;
        }
    }
    return {};
}
} // namespace

ThumbnailJob::ThumbnailJob(const QString &id, const QString &path, const QString &type,
                           const QString &ffmpegPath)
    : m_id(id), m_path(path), m_type(type), m_ffmpegPath(ffmpegPath)
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
    if (m_type == "video") success = extractVideoFrame(m_path, cachePath, m_ffmpegPath);
    else if (m_type == "image") success = scaleImage(m_path, cachePath);
    else if (m_type == "audio") success = extractAudioArt(m_path, cachePath);

    if (success) emit finished(m_id, cachePath);
    else emit failed(m_id);
}

bool ThumbnailJob::extractVideoFrame(const QString &input, const QString &output,
                                     const QString &ffmpegPath) {
    if (ffmpegPath.isEmpty()) {
        qWarning() << "ThumbnailJob: ffmpeg is unavailable for" << input;
        return false;
    }

    QProcess ffmpeg;
    runHidden(ffmpeg);
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
    if (m_ffmpegPath.isEmpty()) {
        qWarning() << "ThumbnailJob: ffmpeg is unavailable for audio artwork" << input;
        return false;
    }

    QProcess ffmpeg;
    runHidden(ffmpeg);
    QStringList args;
    // Embedded ID3/cover art comes in whatever size and aspect ratio the
    // publisher shipped (square, portrait, odd sizes are all common) --
    // "-vcodec copy" preserved that as-is, which left it looking tiny/
    // off-center in the fixed 16:9 thumbnail chips. Scale-to-cover then
    // crop to 320x180 here, matching the video frame path exactly, so
    // every thumbnail (video or song cover art) fills its chip the same way.
    args << "-i" << input << "-an"
         << "-vf" << "scale=320:180:force_original_aspect_ratio=increase,crop=320:180"
         << "-y" << output;
    ffmpeg.start(m_ffmpegPath, args);
    if (!ffmpeg.waitForFinished(3000)) { ffmpeg.kill(); return false; }
    return ffmpeg.exitCode() == 0 && QFile::exists(output);
}

MediaThumbnailManager::MediaThumbnailManager(QObject *parent) : QObject(parent) {
    m_pool.setMaxThreadCount(qMax(1, qMin(2, QThread::idealThreadCount())));
    m_native = new NativeThumbnailGenerator(this);
    connect(m_native, &NativeThumbnailGenerator::finished, this, &MediaThumbnailManager::thumbnailReady);
    connect(m_native, &NativeThumbnailGenerator::failed, this, &MediaThumbnailManager::thumbnailFailed);

    // Remember failures on disk so a file that can't be thumbnailed isn't
    // retried (up to 8 s of background playback each) on every launch.
    connect(this, &MediaThumbnailManager::thumbnailReady, this, [this](const QString &id) {
        m_failMarkers.remove(id);
    });
    connect(this, &MediaThumbnailManager::thumbnailFailed, this, [this](const QString &id) {
        const QString marker = m_failMarkers.take(id);
        if (marker.isEmpty()) return;
        QFile f(marker);
        if (f.open(QIODevice::WriteOnly)) f.close();
    });

    m_ffmpegPath = availableFfmpeg();
    if (!m_ffmpegPath.isEmpty())
        qInfo() << "MediaThumbnailManager: using FFmpeg at" << m_ffmpegPath;
    else
        qInfo() << "MediaThumbnailManager: FFmpeg is unavailable; using native video decoder";
}

void MediaThumbnailManager::enqueue(const QString &id, const QString &path, const QString &type) {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/MediaThumbnails";
    QDir().mkpath(cacheDir);
    QFileInfo info(path);
    QString hashInput = path + QString::number(info.size()) + QString::number(info.lastModified().toMSecsSinceEpoch());
    QString hash = QCryptographicHash::hash(hashInput.toUtf8(), QCryptographicHash::Md5).toHex();
    QString cachePath = cacheDir + "/" + hash + ".jpg";

    const bool hasCachedThumbnail = QFile::exists(cachePath);
    if (hasCachedThumbnail) {
        QTimer::singleShot(0, this, [this, id, cachePath]() {
            emit thumbnailReady(id, cachePath);
        });
        if (type != QStringLiteral("audio"))
            return;
    }

    // Keyed to whether ffmpeg is available, so installing it later retries.
    const QString failMarker = cachePath
        + (type == QStringLiteral("audio")
               ? (m_ffmpegPath.isEmpty() ? ".failnative-audio" : ".failff-audio")
               : (m_ffmpegPath.isEmpty() ? ".failnative" : ".failff"));
    if (QFile::exists(failMarker) && !(type == QStringLiteral("audio") && hasCachedThumbnail)) {
        emit thumbnailFailed(id);
        return;
    }
    m_failMarkers.insert(id, failMarker);

    if (type == QStringLiteral("audio")) {
        m_native->enqueueAudio(id, path, cachePath);
    } else if (type == "video" && m_ffmpegPath.isEmpty()) {
        m_native->enqueue(id, path, cachePath);
    } else {
        auto job = new ThumbnailJob(id, path, type, m_ffmpegPath);
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
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &NativeThumbnailGenerator::onMetaDataChanged);
}

void NativeThumbnailGenerator::enqueue(const QString &id, const QString &path, const QString &cachePath) {
    m_queue.append({id, path, cachePath, false});
    if (!m_processing) processNext();
}

void NativeThumbnailGenerator::enqueueAudio(const QString &id, const QString &path, const QString &cachePath) {
    m_queue.append({id, path, cachePath, true});
    if (!m_processing) processNext();
}

void NativeThumbnailGenerator::processNext() {
    if (m_queue.isEmpty()) { m_processing = false; return; }
    m_processing = true;
    m_current = m_queue.takeFirst();
    const quint64 generation = ++m_generation;

    qDebug() << "NativeThumbnailGenerator: Processing" << m_current.path;
    m_player->setSource(QUrl::fromLocalFile(m_current.path));
    if (!m_current.audio)
        m_player->play();

    // Safety timeout. Guarded by generation so a timeout left over from a
    // previous (already-finished-or-abandoned) job can't stop whatever job
    // is running by the time it fires.
    QTimer::singleShot(8000, this, [this, generation](){
        if (m_processing && generation == m_generation) {
            qWarning() << "NativeThumbnailGenerator: Timeout/Failed for" << m_current.path;
            emit failed(m_current.id);
            m_processing = false;
            m_player->stop();
            m_player->setSource(QUrl());
            processNext();
        }
    });
}

void NativeThumbnailGenerator::onMetaDataChanged()
{
    if (m_processing && m_current.audio)
        finishAudioArtwork();
}

bool NativeThumbnailGenerator::finishAudioArtwork(bool allowTitleOnly)
{
    if (!m_processing || !m_current.audio)
        return false;

    const QMediaMetaData metadata = m_player->metaData();
    QImage cover = metadata.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (cover.isNull())
        cover = metadata.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    const QString title = metadata.value(QMediaMetaData::Title).toString().trimmed();
    if (cover.isNull() && (!allowTitleOnly || title.isEmpty()))
        return false;

    QString thumbnailPath;
    if (!cover.isNull()) {
        const QSize targetSize(320, 320);
        cover = cover.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRect cropRect((cover.width() - targetSize.width()) / 2,
                             (cover.height() - targetSize.height()) / 2,
                             targetSize.width(), targetSize.height());
        if (!cover.copy(cropRect).save(m_current.cachePath, "JPG", 90))
            return false;
        thumbnailPath = m_current.cachePath;
    }

    emit finished(m_current.id, thumbnailPath, title);
    m_processing = false;
    m_player->stop();
    m_player->setSource(QUrl());
    QTimer::singleShot(100, this, &NativeThumbnailGenerator::processNext);
    return true;
}

void NativeThumbnailGenerator::onFrameChanged(const QVideoFrame &frame) {
    if (!m_processing) return;

    // Grab a frame a little into playback (not frame 0, which is often black/blank),
    // scaled to the clip's own length so short clips (many real meeting videos
    // are well under 2 minutes) don't get starved waiting for a position they'll
    // never reach.
    const qint64 duration = m_player->duration();
    const qint64 targetPosition = duration > 0 ? qMin<qint64>(2000, duration / 10) : 500;
    if (m_player->position() < targetPosition) {
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
                m_player->setSource(QUrl());
                QTimer::singleShot(100, this, &NativeThumbnailGenerator::processNext);
            }
        }
    }
}

void NativeThumbnailGenerator::onStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::LoadedMedia) {
        if (m_current.audio) {
            if (finishAudioArtwork(true))
                return;

            const quint64 generation = m_generation;
            QTimer::singleShot(750, this, [this, generation]() {
                if (!m_processing || generation != m_generation || !m_current.audio)
                    return;
                if (finishAudioArtwork(true))
                    return;

                emit failed(m_current.id);
                m_processing = false;
                m_player->stop();
                m_player->setSource(QUrl());
                QTimer::singleShot(100, this, &NativeThumbnailGenerator::processNext);
            });
            return;
        }

        const qint64 duration = m_player->duration();
        const qint64 targetPosition = duration > 0 ? qMin<qint64>(2000, duration / 10) : 500;
        m_player->setPosition(targetPosition);
    }
}
