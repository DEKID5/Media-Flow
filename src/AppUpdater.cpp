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

QNetworkRequest makeApiRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "MediaFlow-Updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    return request;
}

QString formatSpeed(double bytesPerSec)
{
    if (bytesPerSec <= 0)
        return QString();
    if (bytesPerSec >= 1024.0 * 1024.0)
        return QStringLiteral("%1 MB/s").arg(QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1));
    return QStringLiteral("%1 KB/s").arg(QString::number(bytesPerSec / 1024.0, 'f', 0));
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

    QNetworkReply *reply = m_network->get(makeApiRequest(kLatestReleaseUrl));
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
    qint64 assetSize = 0;
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() == installerName) {
            installerUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            assetDigest = asset.value(QStringLiteral("digest")).toString();
            assetSize = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
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
    m_expectedTotalBytes = assetSize;
    m_downloadReady = false;
    m_downloadFailed = false;
    m_downloadProgress = -1;
    m_downloadDetail.clear();
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
    m_partPath = m_installerPath + QStringLiteral(".part");

    // Fast check: if the target installer file already exists on disk and its SHA matches, we are done!
    QFileInfo existingInstaller(m_installerPath);
    if (existingInstaller.exists() && existingInstaller.size() > 0) {
        QFile file(m_installerPath);
        if (file.open(QIODevice::ReadOnly)) {
            QCryptographicHash check(QCryptographicHash::Sha256);
            if (check.addData(&file) && check.result().toHex().toLower() == m_expectedSha256) {
                m_downloadReady = true;
                m_downloadProgress = 100;
                m_downloadFailed = false;
                m_downloadDetail = tr("Installer already verified (%1 MB)")
                    .arg(QString::number(existingInstaller.size() / (1024.0 * 1024.0), 'f', 1));
                m_status = tr("Update ready to install (version %1).").arg(m_latestVersion);
                emit updateStateChanged();
                return;
            }
        }
    }

    // Check if we can resume an existing .part file
    qint64 byteOffset = 0;
    m_downloadHash.reset();
    QFileInfo partInfo(m_partPath);
    if (partInfo.exists() && partInfo.size() > 0
        && (m_expectedTotalBytes == 0 || partInfo.size() < m_expectedTotalBytes)) {
        QFile partFile(m_partPath);
        if (partFile.open(QIODevice::ReadOnly)) {
            if (m_downloadHash.addData(&partFile)) {
                byteOffset = partInfo.size();
                qInfo() << "AppUpdater: resuming download from byte" << byteOffset;
            } else {
                m_downloadHash.reset();
            }
        }
    }

    // Open .part file for writing/appending
    m_downloadFile = std::make_unique<QFile>(m_partPath);
    const QIODevice::OpenMode openMode = (byteOffset > 0)
        ? (QIODevice::WriteOnly | QIODevice::Append)
        : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!m_downloadFile->open(openMode)) {
        const QString error = m_downloadFile->errorString();
        m_downloadFile.reset();
        fail(tr("Could not create the installer download: %1").arg(error));
        return;
    }

    m_bytesAtStart = byteOffset;
    m_bytesReceived = byteOffset;
    m_bytesTotal = m_expectedTotalBytes;
    m_downloadProgress = (m_bytesTotal > 0) ? static_cast<int>((m_bytesReceived * 100) / m_bytesTotal) : 0;
    m_downloading = true;
    m_downloadReady = false;
    m_downloadFailed = false;
    m_retryCount = 0;
    m_downloadTimer.restart();

    if (byteOffset > 0 && m_bytesTotal > 0) {
        m_downloadDetail = tr("Resuming at %1 MB of %2 MB…")
            .arg(QString::number(byteOffset / (1024.0 * 1024.0), 'f', 1))
            .arg(QString::number(m_bytesTotal / (1024.0 * 1024.0), 'f', 1));
    } else {
        m_downloadDetail = tr("Connecting to update server…");
    }
    m_status = tr("Downloading version %1…").arg(m_latestVersion);
    emit updateStateChanged();

    startDownloadRequest(m_downloadUrl, byteOffset);
}

void AppUpdater::startDownloadRequest(const QUrl &url, qint64 byteOffset)
{
    if (!m_downloading)
        return;

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MediaFlow-Updater");
    request.setRawHeader("Accept", "*/*");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);

    if (byteOffset > 0) {
        request.setRawHeader("Range", QStringLiteral("bytes=%1-").arg(byteOffset).toLatin1());
    }

    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;

    connect(reply, &QIODevice::readyRead, this, &AppUpdater::writeDownloadData);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, byteOffset](qint64 received, qint64 total) {
        if (!m_downloading)
            return;
        qint64 actualReceived = byteOffset + received;
        qint64 actualTotal = (total > 0) ? (byteOffset + total) : m_expectedTotalBytes;
        updateProgressDisplay(actualReceived, actualTotal);
    });

    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError code) {
        if (!m_downloading)
            return;
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400)
            return; // Redirection handled in finished

        qWarning() << "AppUpdater: download network error" << code << reply->errorString();
        if (m_retryCount < 3) {
            m_retryCount++;
            m_status = tr("Connection interrupted (%1). Retrying (%2/3)…")
                .arg(reply->errorString()).arg(m_retryCount);
            emit updateStateChanged();
            QTimer::singleShot(2000, this, [this]() {
                if (m_downloading && m_downloadUrl.isValid()) {
                    qint64 currentOffset = m_downloadFile ? m_downloadFile->size() : 0;
                    startDownloadRequest(m_downloadUrl, currentOffset);
                }
            });
            return;
        }

        fail(tr("Download failed: %1. Check connection and click RETRY.").arg(reply->errorString()));
    });

    connect(reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors) {
        if (!m_downloading)
            return;
        QStringList msgs;
        for (const auto &err : errors)
            msgs << err.errorString();
        fail(tr("Security error: %1").arg(msgs.join(QStringLiteral(", "))));
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, byteOffset]() {
        if (m_reply != reply)
            return;

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400) {
            QUrl redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            QUrl resolvedTarget = reply->url().resolved(redirectTarget);
            reply->deleteLater();
            m_reply = nullptr;
            if (resolvedTarget.isValid()) {
                qInfo() << "AppUpdater: following redirect to" << resolvedTarget.host();
                startDownloadRequest(resolvedTarget, byteOffset);
                return;
            }
        }

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
    m_bytesReceived += chunk.size();
    updateProgressDisplay(m_bytesReceived, m_bytesTotal);
}

void AppUpdater::updateProgressDisplay(qint64 received, qint64 total)
{
    m_bytesReceived = received;
    if (total > 0)
        m_bytesTotal = total;

    if (m_bytesTotal > 0)
        m_downloadProgress = static_cast<int>((m_bytesReceived * 100) / m_bytesTotal);
    else
        m_downloadProgress = -1;

    double elapsedSec = m_downloadTimer.elapsed() / 1000.0;
    double bytesSinceStart = static_cast<double>(m_bytesReceived - m_bytesAtStart);
    double speedBytesPerSec = (elapsedSec > 0.5 && bytesSinceStart > 0) ? (bytesSinceStart / elapsedSec) : 0.0;
    QString speedStr = formatSpeed(speedBytesPerSec);

    double mbReceived = m_bytesReceived / (1024.0 * 1024.0);
    double mbTotal = m_bytesTotal / (1024.0 * 1024.0);

    if (m_bytesTotal > 0) {
        QString etaStr;
        if (speedBytesPerSec > 1024.0) {
            double remainingBytes = static_cast<double>(m_bytesTotal - m_bytesReceived);
            int remainingSec = static_cast<int>(remainingBytes / speedBytesPerSec);
            if (remainingSec >= 60)
                etaStr = tr(" • %1m %2s remaining").arg(remainingSec / 60).arg(remainingSec % 60);
            else if (remainingSec > 0)
                etaStr = tr(" • %1s remaining").arg(remainingSec);
        }

        m_downloadDetail = tr("%1 of %2 MB (%3%)%4%5")
            .arg(QString::number(mbReceived, 'f', 1))
            .arg(QString::number(mbTotal, 'f', 1))
            .arg(m_downloadProgress)
            .arg(speedStr.isEmpty() ? QString() : QStringLiteral(" • ") + speedStr)
            .arg(etaStr);
    } else {
        m_downloadDetail = tr("%1 MB downloaded%2")
            .arg(QString::number(mbReceived, 'f', 1))
            .arg(speedStr.isEmpty() ? QString() : QStringLiteral(" • ") + speedStr);
    }

    emit updateStateChanged();
}

void AppUpdater::finishDownload(QNetworkReply *reply)
{
    writeDownloadData();
    if (m_reply != reply)
        return;

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    m_reply = nullptr;
    m_downloading = false;

    if (m_downloadFile) {
        m_downloadFile->flush();
        m_downloadFile->close();
        m_downloadFile.reset();
    }

    if (networkError != QNetworkReply::NoError || (httpStatus != 200 && httpStatus != 206)) {
        m_downloadFailed = true;
        fail(tr("Installer download failed: %1 (HTTP %2). Click RETRY to resume.")
            .arg(networkErrorText).arg(httpStatus));
        return;
    }

    const QByteArray actualSha256 = m_downloadHash.result().toHex().toLower();
    if (actualSha256 != m_expectedSha256) {
        qWarning() << "AppUpdater: checksum mismatch! Expected:" << m_expectedSha256
                   << "Actual:" << actualSha256;
        QFile::remove(m_partPath);
        m_downloadFailed = true;
        fail(tr("The installer checksum did not match. The corrupted file was discarded. Click RETRY to try again."));
        return;
    }

    // Replace final destination with verified .part
    if (QFile::exists(m_installerPath))
        QFile::remove(m_installerPath);
    if (!QFile::rename(m_partPath, m_installerPath)) {
        m_downloadFailed = true;
        fail(tr("Could not finalize the installer file."));
        return;
    }

    m_downloadReady = true;
    m_downloadFailed = false;
    m_downloadProgress = 100;
    m_downloadDetail = tr("Downloaded %1 MB (verified)")
        .arg(QString::number(m_bytesReceived / (1024.0 * 1024.0), 'f', 1));
    m_status = tr("Update ready to install (version %1).").arg(m_latestVersion);
    qInfo() << "AppUpdater: verified installer for version" << m_latestVersion;
    emit updateStateChanged();
}

void AppUpdater::cancelDownload()
{
    if (!m_downloading && !m_downloadFailed)
        return;

    QPointer<QNetworkReply> reply = m_reply;
    m_reply = nullptr;
    m_downloading = false;
    m_downloadFailed = false;

    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile.reset();
    }
    if (!m_partPath.isEmpty() && QFile::exists(m_partPath)) {
        QFile::remove(m_partPath);
    }

    m_downloadProgress = -1;
    m_downloadDetail.clear();
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
    m_downloading = false;
    setStatus(message);
}
