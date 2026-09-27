#pragma once

#include <QObject>
#include <QVariantMap>
#include <QMap>
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QFileInfo>

/**
 * @brief The SongManager class handles JW song discovery based on naming conventions.
 */
class SongManager : public QObject
{
    Q_OBJECT
public:
    explicit SongManager(QObject *parent = nullptr);

    /**
     * @brief Searches for a JW song video based on number and language code.
     * @param songNumber The song number (e.g. 12)
     * @param languageCode The language code (e.g. "EW" or "English")
     * @return A map containing path, name, type, and thumbnailPath.
     */
    Q_INVOKABLE QVariantMap searchSong(int songNumber, const QString& languageCode, const QVariantList &mediaLibrary = {});
    Q_INVOKABLE QVariantMap findSongFile(int songNumber, const QString& languageCode, const QVariantList &mediaLibrary = {});
    Q_INVOKABLE QVariantList filterMediaByLanguage(const QVariantList& mediaList, const QString& languageCode);

private:
    QString getLanguageCode(const QString& input);
    QString getJWMoviesPath();
    
    QMap<QString, QString> m_langMapping;
};
