#pragma once

#include <QObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QUrl>
#include <memory>

class QNetworkReply;

class AppUpdater final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY updateStateChanged)
    Q_PROPERTY(QString status READ status NOTIFY updateStateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY updateStateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY updateStateChanged)
    Q_PROPERTY(bool downloadReady READ downloadReady NOTIFY updateStateChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY updateStateChanged)

public:
    explicit AppUpdater(const QString &currentVersion, QObject *parent = nullptr);

    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString status() const { return m_status; }
    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool downloading() const { return m_downloading; }
    bool downloadReady() const { return m_downloadReady; }
    int downloadProgress() const { return m_downloadProgress; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void installAndRestart();

signals:
    void updateStateChanged();

private:
    void setStatus(const QString &status);
    void fail(const QString &message);
    void receiveReleaseMetadata(QNetworkReply *reply);
    void writeDownloadData();
    void finishDownload(QNetworkReply *reply);

    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseNotes;
    QString m_status;
    QUrl m_downloadUrl;
    QByteArray m_expectedSha256;
    QString m_installerPath;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QSaveFile> m_downloadFile;
    QCryptographicHash m_downloadHash{QCryptographicHash::Sha256};
    bool m_checking = false;
    bool m_updateAvailable = false;
    bool m_downloading = false;
    bool m_downloadReady = false;
    int m_downloadProgress = -1;
};
