#include "MediaExtractor.h"
#include <QStandardPaths>
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QUrl>
#include <QtConcurrent>
#include <QDebug>

static const QStringList kNameFilters = {
    // Videos
    "*.mp4", "*.mov", "*.mkv", "*.avi",
    // Images
    "*.jpg", "*.jpeg", "*.png", "*.webp",
    // Audio
    "*.mp3", "*.m4a", "*.wav"
};

MediaExtractor::MediaExtractor(QObject *parent)
    : QObject(parent)
{
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    m_targetDirs << home + "/AppData/Local/Packages/48C9FCC0.Watchtower_xzhgwqvnvmbce/LocalState/Publications";
    m_targetDirs << home + "/AppData/Local/Packages/48C9FCC0.Watchtower_xzhgwqvnvmbce/LocalState/Data/Media";
    m_targetDirs << home + "/AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e/LocalState/Publications";
    m_targetDirs << home + "/AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e/LocalState/Data/Media";
    m_targetDirs << home + "/Videos/JWLibrary";
    m_targetDirs << home + "/Movies/JWLibrary";
}

void MediaExtractor::startScan()
{
    QtConcurrent::run([this]() {
        for (const QString &dirPath : m_targetDirs) {
            scanDir(dirPath);
        }

        // Setup live watcher after initial scan completes
        QMetaObject::invokeMethod(this, &MediaExtractor::setupWatcher, Qt::QueuedConnection);

        emit scanFinished();
    });
}

void MediaExtractor::scanDirectory(const QString &dirPath)
{
    QtConcurrent::run([this, dirPath]() {
        scanDir(dirPath);

        // Also watch this new directory
        QMetaObject::invokeMethod(this, [this, dirPath]() {
            if (!m_watcher) {
                m_watcher = new QFileSystemWatcher(this);
                connect(m_watcher, &QFileSystemWatcher::directoryChanged,
                        this, &MediaExtractor::onDirectoryChanged);
            }
            QDir dir(dirPath);
            if (dir.exists()) {
                m_watcher->addPath(dirPath);
                // Watch subdirs too
                QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                while (it.hasNext()) m_watcher->addPath(it.next());
            }
        }, Qt::QueuedConnection);

        emit scanFinished();
    });
}

void MediaExtractor::scanDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        qDebug() << "MediaExtractor: Directory does not exist:" << dirPath;
        return;
    }

    qDebug() << "MediaExtractor: Scanning directory:" << dirPath;
    QDirIterator it(dirPath, kNameFilters, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        // Dedup: skip already-indexed files
        if (m_indexedPaths.contains(filePath)) continue;
        m_indexedPaths.insert(filePath);
        processFile(filePath);
    }
}

void MediaExtractor::setupWatcher()
{
    if (m_watcher) return;
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &MediaExtractor::onDirectoryChanged);

    for (const QString &dirPath : m_targetDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        m_watcher->addPath(dirPath);
        // Also watch subdirectories
        QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            m_watcher->addPath(it.next());
        }
    }
    qDebug() << "MediaExtractor: QFileSystemWatcher active on" << m_watcher->directories().count() << "directories";
}

void MediaExtractor::onDirectoryChanged(const QString &path)
{
    qDebug() << "MediaExtractor: Directory changed:" << path;
    // Rescan just this directory for new files (non-recursive for speed)
    QDirIterator it(path, kNameFilters, QDir::Files);
    while (it.hasNext()) {
        QString filePath = it.next();
        if (m_indexedPaths.contains(filePath)) continue;
        m_indexedPaths.insert(filePath);
        processFile(filePath);
        qDebug() << "MediaExtractor: New file detected:" << filePath;
    }
}

void MediaExtractor::processFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    MediaType type;

    if (ext == "mp4" || ext == "mov" || ext == "mkv" || ext == "avi") {
        type = MediaType::Video;
    } else if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp") {
        type = MediaType::Image;
    } else if (ext == "mp3" || ext == "m4a" || ext == "wav") {
        type = MediaType::Audio;
    } else {
        return;
    }

    QString name = fi.baseName();
    QDateTime creationDate = fi.birthTime();
    if (!creationDate.isValid()) creationDate = fi.lastModified();

    QImage thumbnail = extractThumbnail(filePath, type);

    emit mediaFound(type, name, fi.absoluteFilePath(), thumbnail, creationDate);
}

QImage MediaExtractor::extractThumbnail(const QString &filePath, MediaType type)
{
    QFileInfo fi(filePath);

    if (type == MediaType::Image) {
        QImageReader reader(filePath);
        reader.setScaledSize(QSize(320, 180));
        if (reader.canRead()) return reader.read();
    } else if (type == MediaType::Video) {
        // Strategy: Look for JW Library companion thumbnails
        QStringList candidates;
        candidates << fi.absolutePath() + "/" + fi.baseName() + ".jpg";
        candidates << fi.absolutePath() + "/" + fi.baseName() + ".png";

        for (const QString &c : candidates) {
            if (QFile::exists(c)) {
                QImage img(c);
                if (!img.isNull()) return img.scaled(320, 180, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            }
        }

        // Fallback: try QImageReader on video (works on some platforms)
        QImageReader videoReader(filePath);
        if (videoReader.canRead()) {
            videoReader.setScaledSize(QSize(320, 180));
            return videoReader.read();
        }

        return QImage(":/MediaFlow/qml/assets/video_placeholder.png");
    } else if (type == MediaType::Audio) {
        return QImage(":/MediaFlow/qml/assets/meeting_placeholder.png");
    }

    return QImage();
}
