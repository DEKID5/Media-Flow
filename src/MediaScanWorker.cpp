#include "MediaScanWorker.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QVariantMap>
#include <QRegularExpression>
#include <QUuid>
#include <QUrl>
#include <QStandardPaths>
#include <QMediaDevices>
#include <QCameraDevice>

QStringList MediaScanWorker::defaultScanRoots()
{
    QStringList roots;
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    if (!local.isEmpty()) {
        const QString pkg1 = QStringLiteral("WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e");
        const QString base1 = QDir(local).filePath(QStringLiteral("Packages/%1/LocalState").arg(pkg1));
        roots << QDir(base1).filePath(QStringLiteral("Publications"));
        roots << QDir(base1).filePath(QStringLiteral("Data/media"));
        
        const QString pkg2 = QStringLiteral("48C9FCC0.Watchtower_xzhgwqvnvmbce");
        const QString base2 = QDir(local).filePath(QStringLiteral("Packages/%1/LocalState").arg(pkg2));
        roots << QDir(base2).filePath(QStringLiteral("Publications"));
        roots << QDir(base2).filePath(QStringLiteral("Data/Media"));
    }

    if (!movies.isEmpty()) {
        roots << movies;
        roots << QDir(movies).filePath(QStringLiteral("JWLibrary"));
        roots << QDir(movies).filePath(QStringLiteral("JW Library"));
    }
    
    if (!pictures.isEmpty()) {
        roots << pictures;
    }

    // Explicit fallback for common Windows path if QStandardPaths fails
    const QString home = QDir::homePath();
    roots << QDir(home).filePath(QStringLiteral("Videos/JWLibrary"));
    roots << QDir(home).filePath(QStringLiteral("Videos/JW Library"));

    return roots;
}

void MediaScanWorker::scanDir(const QString &dir, const QStringList &extensions, int depth, QVariantList *out)
{
    if (depth > 5)
        return;

    QDir d(dir);
    if (!d.exists())
        return;

    const QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            scanDir(fi.absoluteFilePath(), extensions, depth + 1, out);
            continue;
        }
        const QString ext = fi.suffix().toLower();
        if (!extensions.contains(QLatin1Char('.') + ext))
            continue;

        QVariantMap m;
        m.insert(QStringLiteral("name"), fi.fileName());
        m.insert(QStringLiteral("absolutePath"), fi.absoluteFilePath());
        m.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        m.insert(QStringLiteral("type"), QStringLiteral("image"));
        
        QString type = QStringLiteral("image");
        if (ext == QLatin1String("mp3") || ext == QLatin1String("m4a"))
            type = QStringLiteral("audio");
        else if (ext == QLatin1String("mp4") || ext == QLatin1String("m4v") || ext == QLatin1String("mov"))
            type = QStringLiteral("video");
        
        m.insert(QStringLiteral("type"), type);
        m.insert(QStringLiteral("thumbnailPath"), type == QStringLiteral("image") ? QUrl::fromLocalFile(fi.absoluteFilePath()).toString() : QStringLiteral("qrc:/MediaFlow/qml/assets/video_placeholder.png"));
        m.insert(QStringLiteral("isStaged"), false);

        // Advanced Parsing for JW Songs
        if (type == QStringLiteral("audio") || type == QStringLiteral("video")) {
            // Pattern: sjj([mc])_([A-Z]+)_(\d{3})(?:_r(\d+)P)?
            // Group 1: trackType (m=vocal, c=chorus)
            // Group 2: languageSymbol
            // Group 3: songNumber
            // Group 4: resolution (optional)
            static const QRegularExpression jwRegex(QStringLiteral("sjj([mc])_([A-Z]+)_(\\d{3})(?:_r(\\d+)P)?"), QRegularExpression::CaseInsensitiveOption);
            auto match = jwRegex.match(fi.fileName());
            
            if (match.hasMatch()) {
                m.insert(QStringLiteral("isSong"), true);
                m.insert(QStringLiteral("trackType"), match.captured(1).toLower() == QStringLiteral("m") ? QStringLiteral("vocal") : QStringLiteral("instrumental"));
                m.insert(QStringLiteral("languageCode"), match.captured(2).toUpper());
                m.insert(QStringLiteral("songNumber"), match.captured(3).toInt());
                if (match.lastCapturedIndex() >= 4 && !match.captured(4).isEmpty()) {
                    m.insert(QStringLiteral("resolution"), match.captured(4).toInt());
                }
            } else {
                // Fallback for other potential song formats (snnw etc)
                static const QRegularExpression altRegex(QStringLiteral("(?:snnw|snn|sjj)_([A-Z]+)_(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                auto altMatch = altRegex.match(fi.fileName());
                if (altMatch.hasMatch()) {
                    m.insert(QStringLiteral("isSong"), true);
                    m.insert(QStringLiteral("languageCode"), altMatch.captured(1).toUpper());
                    m.insert(QStringLiteral("songNumber"), altMatch.captured(2).toInt());
                }
            }
        }

        out->append(m);
    }
}

MediaScanWorker::MediaScanWorker(QObject *parent)
    : QObject(parent)
{
}

void MediaScanWorker::scan(const QStringList &extraOrExclusiveRoots, const QString &languageFilter,
                           bool includeDefaultJwPaths)
{
    QStringList roots;
    if (includeDefaultJwPaths)
        roots = defaultScanRoots();
    for (const QString &r : extraOrExclusiveRoots) {
        if (!r.isEmpty())
            roots << r;
    }

    const QSet<QString> uniq(roots.begin(), roots.end());
    QStringList dedup(uniq.begin(), uniq.end());
    dedup.sort();

    const QStringList extensions{QStringLiteral(".mp4"), QStringLiteral(".m4v"), QStringLiteral(".mp3"),
                               QStringLiteral(".jpg"), QStringLiteral(".png"), QStringLiteral(".jpeg"),
                               QStringLiteral(".webp")};

    QVariantList all;
    for (const QString &root : dedup) {
        if (!QDir(root).exists())
            continue;
        scanDir(root, extensions, 0, &all);
    }

    // Removed restrictive language filtering from scan to ensure all videos are visible.
    // Filtering can be done UI-side if needed.

    QVariantList finalAssets;
    
    // Hardware Inputs
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cameras) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), cam.id());
        m.insert(QStringLiteral("name"), cam.description());
        m.insert(QStringLiteral("type"), QStringLiteral("input"));
        m.insert(QStringLiteral("absolutePath"), cam.id());
        m.insert(QStringLiteral("thumbnailPath"), QStringLiteral("qrc:/MediaFlow/qml/assets/cam_placeholder.png"));
        m.insert(QStringLiteral("isStaged"), true);
        finalAssets.append(m);
    }

    // Scanned Files
    for (const QVariant &v : all) {
        finalAssets.append(v);
    }

    emit scanFinished(finalAssets);
}
