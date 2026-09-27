#pragma once

#include <QVariantList>
#include <QVariantMap>
#include <QString>

namespace SongSearchUtils {

QString normalizeLanguageCode(const QString &language);
QString languageNameForCode(const QString &languageCode);
QVariantList supportedLanguages();
QVariantList filterMediaByLanguage(const QVariantList &mediaList, const QString &languageCode);
QVariantMap findSongFile(int songNumber, const QString &languageCode, const QVariantList &mediaLibrary);

}
