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

    // Known package ids as a last-resort fallback, in case the scan above
    // ever comes back empty on a machine where Packages\ isn't listable for
    // some reason (e.g. a permissions quirk) -- keeps existing installs
    // working exactly as before even if the dynamic scan fails.
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
