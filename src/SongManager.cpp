#include "SongManager.h"
#include "SongSearchUtils.h"
#include <QDirIterator>
#include <QDebug>

SongManager::SongManager(QObject *parent) : QObject(parent)
{
    // Official JW Language Mappings
    m_langMapping["English"] = "E";
    m_langMapping["Ewe"] = "EW";
    m_langMapping["Twi"] = "TW";
    m_langMapping["Ga"] = "GA";
    m_langMapping["French"] = "F";
    m_langMapping["Spanish"] = "S";
}

QVariantMap SongManager::searchSong(int songNumber, const QString& language, const QVariantList &mediaLibrary)
{
    return findSongFile(songNumber, language, mediaLibrary);
}

QVariantMap SongManager::findSongFile(int songNumber, const QString& language, const QVariantList &mediaLibrary)
{
    if (!mediaLibrary.isEmpty())
        return SongSearchUtils::findSongFile(songNumber, language, mediaLibrary);

    QVariantMap result;
    result["found"] = false;

    if (songNumber <= 0) return result;

    // 1. Get official code (handle both full name and code)
    QString code = getLanguageCode(language);
    
    // 2. Zero-pad number to 3 digits (12 -> 012)
    QString paddedNum = QString("%1").arg(songNumber, 3, 10, QLatin1Char('0'));
    
    // 3. Construct pattern: sjjm_LANG_NUMBER (e.g. sjjm_E_012)
    // The requirement says: SJJM_${languageCode}_${songNumber.padStart(3, '0')}
    QString pattern = QString("sjjm_%1_%2").arg(code).arg(paddedNum);
    
    qDebug() << "SongManager findSongFile searching for pattern:" << pattern;

    // 4. Determine search paths
    QString moviesPath = getJWMoviesPath();
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    
    QStringList searchPaths = {
        moviesPath,
        home + "/Videos/JWLibrary",
        home + "/AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e/LocalState/Data/Media"
    };

    // 5. Scan for matches
    for (const QString &dirPath : searchPaths) {
        if (!QDir(dirPath).exists()) continue;
        
        QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QString fileName = QFileInfo(filePath).fileName();
            
            // Strict pattern check
            if (fileName.contains(pattern, Qt::CaseInsensitive) && 
               (fileName.endsWith(".mp4") || fileName.endsWith(".m4v") || fileName.endsWith(".mov"))) 
            {
                qDebug() << "SONG MATCH FOUND:" << filePath;
                result["found"] = true;
                result["absolutePath"] = filePath;
                result["name"] = fileName;
                result["type"] = "video";
                result["code"] = code;
                result["songNumber"] = songNumber;
                result["thumbnailPath"] = ""; 
                return result;
            }
        }
    }

    return result;
}

QVariantList SongManager::filterMediaByLanguage(const QVariantList& mediaList, const QString& languageCode)
{
    return SongSearchUtils::filterMediaByLanguage(mediaList, languageCode);
}

QString SongManager::getLanguageCode(const QString& input)
{
    // If input is already a code (E, EW, etc), return it
    if (m_langMapping.values().contains(input.toUpper())) return input.toUpper();
    
    // If it's a full name, return the mapping
    if (m_langMapping.contains(input)) return m_langMapping[input];
    
    // Default to English if not found
    return "E";
}

QString SongManager::getJWMoviesPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/JWLibrary";
}
