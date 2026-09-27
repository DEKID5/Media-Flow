#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QSet>

enum class MediaType {
    Video,
    Image,
    Audio,
    Input
};

/**
 * @brief The MediaExtractor class recursively indexes JW Library directories
 * and watches them for real-time changes via QFileSystemWatcher.
 */
class MediaExtractor : public QObject
{
    Q_OBJECT
public:
    explicit MediaExtractor(QObject *parent = nullptr);

    /**
     * @brief Starts the recursive scan in a background thread.
     */
    Q_INVOKABLE void startScan();

    /**
     * @brief Scans a single user-selected directory.
     */
    Q_INVOKABLE void scanDirectory(const QString &dirPath);

signals:
    void mediaFound(MediaType type, const QString &name, const QString &absolutePath, const QImage &thumbnail, const QDateTime &creationDate);
    void scanFinished();

private slots:
    void onDirectoryChanged(const QString &path);

private:
    void processFile(const QString &filePath);
    QImage extractThumbnail(const QString &filePath, MediaType type);
    void setupWatcher();
    void scanDir(const QString &dirPath);

    QStringList m_targetDirs;
    QFileSystemWatcher *m_watcher = nullptr;
    QSet<QString> m_indexedPaths;     // Dedup: tracks all files already emitted
    QMutex m_indexMutex;
};
