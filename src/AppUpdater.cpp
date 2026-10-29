#include "AppUpdater.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

namespace {

const QUrl kLatestReleaseUrl(
    QStringLiteral("https://api.github.com/repos/DEKID5/Media-Flow/releases/latest"));

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "MediaFlow-Updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

}

AppUpdater::AppUpdater(const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_network(new QNetworkAccessManager(this))
{
    QTimer::singleShot(5000, this, &AppUpdater::checkForUpdates);
    auto *periodicCheck = new QTimer(this);
    periodicCheck->setInterval(24 * 60 * 60 * 1000);
    connect(periodicCheck, &QTimer::timeout, this, &AppUpdater::checkForUpdates);
    periodicCheck->start();
}

void AppUpdater::checkForUpdates()
{
    if (m_checking || m_downloading)
        return;

    m_checking = true;
    m_status = tr("Checking for updates…");
    emit updateStateChanged();

    QNetworkReply *reply = m_network->get(makeRequest(kLatestReleaseUrl));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        receiveReleaseMetadata(reply);
    });
}

void AppUpdater::receiveReleaseMetadata(QNetworkReply *reply)
{
    const QByteArray response = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    m_reply = nullptr;
    m_checking = false;

    if (networkError != QNetworkReply::NoError) {
        fail(tr("Could not check for updates: %1").arg(networkErrorText));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(tr("The update service returned an invalid response."));
        return;
    }

    const QJsonObject release = document.object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    QString versionText = tag;
    if (versionText.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        versionText.remove(0, 1);

    const QVersionNumber current = QVersionNumber::fromString(m_currentVersion);
    const QVersionNumber latest = QVersionNumber::fromString(versionText);
    if (current.isNull() || latest.isNull() || versionText.isEmpty()) {
        fail(tr("The published release has an invalid version."));
        return;
    }

    const QString installerName = QStringLiteral("MediaFlow-Setup-%1.exe").arg(versionText);
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    QUrl installerUrl;
    QString assetDigest;
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() == installerName) {
            installerUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            assetDigest = asset.value(QStringLiteral("digest")).toString();
            break;
        }
    }

    const QString body = release.value(QStringLiteral("body")).toString();
    static const QRegularExpression checksumPattern(
        QStringLiteral("SHA-256:\\s*`?([0-9a-fA-F]{64})`?"));
    const QRegularExpressionMatch checksumMatch = checksumPattern.match(body);
    QString checksum = assetDigest;
    if (checksum.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        checksum.remove(0, 7);
    else if (checksumMatch.capturedLength(1) == 64)
        checksum = checksumMatch.captured(1);
    if (!installerUrl.isValid() || installerUrl.scheme() != QStringLiteral("https")
        || !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$")).match(checksum).hasMatch()) {
        fail(tr("This release is missing its installer or SHA-256 checksum."));
        return;
    }

    m_latestVersion = versionText;
    m_releaseNotes = body;
    m_downloadUrl = installerUrl;
    m_expectedSha256 = checksum.toLatin1().toLower();
    m_downloadReady = false;
    m_downloadProgress = -1;
    m_updateAvailable = QVersionNumber::compare(latest, current) > 0;
    if (m_updateAvailable)
        m_status = tr("Version %1 is available.").arg(m_latestVersion);
    else
        m_status = tr("You’re up to date (version %1).").arg(m_currentVersion);
    qInfo() << "AppUpdater: latest stable release is" << m_latestVersion
            << (m_updateAvailable ? "(update available)" : "(current)");
    emit updateStateChanged();
}

void AppUpdater::downloadUpdate()
{
    if (!m_updateAvailable || m_downloading || !m_downloadUrl.isValid())
        return;

    const QString tempDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDirectory.isEmpty()) {
        fail(tr("Windows did not provide a temporary folder for the installer."));
        return;
    }

    m_installerPath = QDir(tempDirectory).filePath(
        QStringLiteral("MediaFlow-Setup-%1.exe").arg(m_latestVersion));
    m_downloadFile = std::make_unique<QSaveFile>(m_installerPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        const QString error = m_downloadFile->errorString();
        m_downloadFile.reset();
        fail(tr("Could not create the installer download: %1").arg(error));
        return;
    }

    m_downloadHash.reset();
    m_downloadProgress = 0;
    m_downloading = true;
    m_downloadReady = false;
    m_status = tr("Downloading version %1…").arg(m_latestVersion);
    emit updateStateChanged();

    QNetworkReply *reply = m_network->get(makeRequest(m_downloadUrl));
    m_reply = reply;
    connect(reply, &QIODevice::readyRead, this, &AppUpdater::writeDownloadData);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            m_downloadProgress = static_cast<int>((received * 100) / total);
        else
            m_downloadProgress = -1;
        emit updateStateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        finishDownload(reply);
    });
}

void AppUpdater::writeDownloadData()
{
    if (!m_reply || !m_downloadFile)
        return;

    const QByteArray chunk = m_reply->readAll();
    if (chunk.isEmpty())
        return;
    if (m_downloadFile->write(chunk) != chunk.size()) {
        const QString error = m_downloadFile->errorString();
        cancelDownload();
        fail(tr("Could not save the installer: %1").arg(error));
        return;
    }
    m_downloadHash.addData(chunk);
}

void AppUpdater::finishDownload(QNetworkReply *reply)
{
    writeDownloadData();
    if (m_reply != reply)
        return;

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    m_reply = nullptr;
    m_downloading = false;

    if (networkError != QNetworkReply::NoError) {
        if (m_downloadFile) {
            m_downloadFile->cancelWriting();
            m_downloadFile.reset();
        }
        fail(tr("Installer download failed: %1").arg(networkErrorText));
        return;
    }

    const QByteArray actualSha256 = m_downloadHash.result().toHex();
    if (actualSha256 != m_expectedSha256) {
        if (m_downloadFile) {
            m_downloadFile->cancelWriting();
            m_downloadFile.reset();
        }
        fail(tr("The installer checksum does not match. The file was discarded."));
        return;
    }

    if (!m_downloadFile || !m_downloadFile->commit()) {
        const QString error = m_downloadFile ? m_downloadFile->errorString() : QString();
        m_downloadFile.reset();
        fail(tr("Could not finalize the installer download: %1").arg(error));
        return;
    }
    m_downloadFile.reset();

    m_downloadReady = true;
    m_downloadProgress = 100;
    m_status = tr("Download verified. Ready to install version %1.").arg(m_latestVersion);
    qInfo() << "AppUpdater: verified installer for version" << m_latestVersion;
    emit updateStateChanged();
}

void AppUpdater::cancelDownload()
{
    if (!m_downloading)
        return;

    QPointer<QNetworkReply> reply = m_reply;
    m_reply = nullptr;
    m_downloading = false;
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->cancelWriting();
        m_downloadFile.reset();
    }
    m_downloadProgress = -1;
    m_status = tr("Download cancelled.");
    emit updateStateChanged();
}

void AppUpdater::installAndRestart()
{
    if (!m_downloadReady || !QFileInfo::exists(m_installerPath))
        return;

    const bool started = QProcess::startDetached(m_installerPath, {QStringLiteral("/CLOSEAPPLICATIONS")});
    if (!started) {
        fail(tr("Could not start the installer. You can run it manually from %1.")
                 .arg(QDir::toNativeSeparators(m_installerPath)));
        return;
    }

    m_status = tr("Installer started. MediaFlow will close; finish setup to restart the app.");
    emit updateStateChanged();
    QTimer::singleShot(500, QCoreApplication::instance(), &QCoreApplication::quit);
}

void AppUpdater::setStatus(const QString &status)
{
    m_status = status;
    emit updateStateChanged();
}

void AppUpdater::fail(const QString &message)
{
    qWarning() << "AppUpdater:" << message;
    setStatus(message);
}
