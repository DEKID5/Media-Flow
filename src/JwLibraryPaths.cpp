#include "JwLibraryPaths.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace JwLibraryPaths {

QStringList packageRoots()
{
    QStringList roots;
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir packagesDir(home + QStringLiteral("/AppData/Local/Packages"));
    if (!packagesDir.exists())
        return roots;

    const QFileInfoList entries = packagesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        if (fi.fileName().contains(QStringLiteral("watchtower"), Qt::CaseInsensitive))
            roots << fi.absoluteFilePath();
    }

    // Fall back to the known package IDs if the scan misses the install folder.
    if (roots.isEmpty()) {
        roots << home + QStringLiteral("/AppData/Local/Packages/48C9FCC0.Watchtower_xzhgwqvnvmbce");
        roots << home + QStringLiteral("/AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e");
    }
    return roots;
}

static QStringList existingSubdirs(const QString &suffix)
{
    QStringList result;
    for (const QString &root : packageRoots()) {
        const QString candidate = root + suffix;
        if (QDir(candidate).exists())
            result << candidate;
    }
    return result;
}

QStringList publicationsDirs()
{
    return existingSubdirs(QStringLiteral("/LocalState/Publications"));
}

QStringList dataMediaDirs()
{
    return existingSubdirs(QStringLiteral("/LocalState/Data/Media"));
}

} // namespace JwLibraryPaths
