#pragma once

#include <QStringList>

/**
 * @brief Locates JW Library's own data folders on this machine.
 *
 * JW Library ships as a Windows Store (MSIX) app, whose per-user data lives
 * under a package folder named "<publisher-id>.<AppName>_<hash>" inside
 * %LOCALAPPDATA%\Packages\ -- both the publisher id and the random hash
 * suffix have changed across JW Library releases/distributions (two
 * different values are already known to exist in the wild), and could
 * change again on a machine this app has never seen. Hardcoding exact
 * package folder names is fragile; instead this scans %LOCALAPPDATA%\Packages
 * for any folder whose name contains "watchtower" (case-insensitive) --
 * true for every known JW Library package id -- so a fresh install on a
 * machine with a different package suffix still gets found automatically.
 */
namespace JwLibraryPaths {

// Every matching package's own root folder, e.g.
// ".../AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e".
QStringList packageRoots();

// Each matching package's LocalState/Publications folder, only those that
// actually exist on disk.
QStringList publicationsDirs();

// Each matching package's LocalState/Data/Media folder, only those that
// actually exist on disk.
QStringList dataMediaDirs();

} // namespace JwLibraryPaths
