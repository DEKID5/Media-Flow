#include "MediaExtractor.h"
#include "JwLibraryPaths.h"
#include <QStandardPaths>
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QtConcurrent>
#include <QDebug>
#include <QMutexLocker>

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

    // Scan JW Library’s install folders instead of assuming one fixed package name.
    for (const QString &root : JwLibraryPaths::packageRoots()) {
        m_targetDirs << root + "/LocalState/Publications";
        m_targetDirs << root + "/LocalState/Data/Media";
    }
    m_targetDirs << home + "/Videos/JWLibrary";
    m_targetDirs << home + "/Movies/JWLibrary";
}

void MediaExtractor::startScan()
{
    auto future = QtConcurrent::run([this]() {
        for (const QString &dirPath : m_targetDirs) {
            scanDir(dirPath);
        }

        // Set up the live watcher after the first scan finishes.
        QMetaObject::invokeMethod(this, &MediaExtractor::setupWatcher, Qt::QueuedConnection);

        emit scanFinished();
    });
    Q_UNUSED(future);
}

void MediaExtractor::scanDirectory(const QString &dirPath)
{
    auto future = QtConcurrent::run([this, dirPath]() {
        scanDir(dirPath);

        // Watch the new directory too.
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
    Q_UNUSED(future);
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
        {
            QMutexLocker locker(&m_indexMutex);
            if (m_indexedPaths.contains(filePath))
                continue;
            m_indexedPaths.insert(filePath);
        }
        processFile(filePath);
    }
}

void MediaExtractor::setupWatcher()
{
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged,
                this, &MediaExtractor::onDirectoryChanged);
    }

    for (const QString &dirPath : m_targetDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        if (!m_watcher->directories().contains(dirPath))
            m_watcher->addPath(dirPath);
        // Watch subfolders as well.
        QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString subdir = it.next();
            if (!m_watcher->directories().contains(subdir))
                m_watcher->addPath(subdir);
        }
    }
    qDebug() << "MediaExtractor: QFileSystemWatcher active on" << m_watcher->directories().count() << "directories";
}

void MediaExtractor::onDirectoryChanged(const QString &path)
{
    qDebug() << "MediaExtractor: Directory changed:" << path;
    auto future = QtConcurrent::run([this, path]() {
        scanDir(path);
        QMetaObject::invokeMethod(this, &MediaExtractor::setupWatcher, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
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

    emit mediaFound(type, name, fi.absoluteFilePath(), creationDate);
}
