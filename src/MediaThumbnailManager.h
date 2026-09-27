#pragma once

#include <QObject>
#include <QRunnable>
#include <QThreadPool>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QImageReader>
#include <QImage>

class ThumbnailJob : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ThumbnailJob(const QString &id, const QString &path, const QString &type);
    void run() override;

signals:
    void finished(const QString &id, const QString &cachePath);
    void failed(const QString &id);

private:
    bool extractVideoFrame(const QString &input, const QString &output);
    bool scaleImage(const QString &input, const QString &output);
    bool extractAudioArt(const QString &input, const QString &output);

    QString m_id;
    QString m_path;
    QString m_type;
};

#include <QVideoSink>
#include <QMediaPlayer>
#include <QVideoFrame>

class NativeThumbnailGenerator : public QObject
{
    Q_OBJECT
public:
    explicit NativeThumbnailGenerator(QObject *parent = nullptr);
    void enqueue(const QString &id, const QString &path, const QString &cachePath);

signals:
    void finished(const QString &id, const QString &cachePath, const QString &title);
    void failed(const QString &id);

private slots:
    void processNext();
    void onFrameChanged(const QVideoFrame &frame);
    void onStatusChanged(QMediaPlayer::MediaStatus status);

private:
    struct Request { QString id; QString path; QString cachePath; };
    QList<Request> m_queue;
    QMediaPlayer *m_player;
    QVideoSink *m_sink;
    bool m_processing = false;
    Request m_current;
};

class MediaThumbnailManager : public QObject
{
    Q_OBJECT
public:
    explicit MediaThumbnailManager(QObject *parent = nullptr);
    void enqueue(const QString &id, const QString &path, const QString &type);

signals:
    void thumbnailReady(const QString &id, const QString &cachePath, const QString &title = "");
    void thumbnailFailed(const QString &id);

private:
    QThreadPool m_pool;
    NativeThumbnailGenerator *m_native;
    bool m_hasFfmpeg = false;
};
