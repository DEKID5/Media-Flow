#include "SongSearchUtils.h"

#include <QFileInfo>

namespace {

bool isMediaFile(const QString &fileName)
{
    return fileName.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".m4v"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".mov"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".mkv"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".m4a"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive);
}

bool containsSongStem(const QString &fileName, const QString &stem)
{
    const int start = fileName.indexOf(stem, 0, Qt::CaseInsensitive);
    if (start < 0)
        return false;

    const int nextIndex = start + stem.length();
    if (nextIndex >= fileName.length())
        return true;

    const QChar next = fileName.at(nextIndex);
    return next == QLatin1Char('_') || next == QLatin1Char('.');
}

QString fileNameFromMediaMap(const QVariantMap &media)
{
    const QString path = media.value(QStringLiteral("absolutePath")).toString();
    if (!path.isEmpty()) {
        const QString fileName = QFileInfo(path).fileName();
        if (!fileName.isEmpty())
            return fileName;
    }
    return media.value(QStringLiteral("name")).toString();
}

} // namespace

namespace SongSearchUtils {

QVariantList supportedLanguages()
{
    return {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("English")}, {QStringLiteral("code"), QStringLiteral("E")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Ewe")}, {QStringLiteral("code"), QStringLiteral("EW")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Twi")}, {QStringLiteral("code"), QStringLiteral("TW")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Ga")}, {QStringLiteral("code"), QStringLiteral("GA")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("French")}, {QStringLiteral("code"), QStringLiteral("F")}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Spanish")}, {QStringLiteral("code"), QStringLiteral("S")}}
    };
}

QString normalizeLanguageCode(const QString &language)
{
    const QString trimmed = language.trimmed();
    const QString upper = trimmed.toUpper();
    for (const QVariant &item : supportedLanguages()) {
        const QVariantMap lang = item.toMap();
        if (lang.value(QStringLiteral("code")).toString().compare(upper, Qt::CaseInsensitive) == 0
            || lang.value(QStringLiteral("name")).toString().compare(trimmed, Qt::CaseInsensitive) == 0) {
            return lang.value(QStringLiteral("code")).toString();
        }
    }
    return upper.isEmpty() ? QStringLiteral("E") : upper;
}

QString languageNameForCode(const QString &languageCode)
{
    const QString code = normalizeLanguageCode(languageCode);
    for (const QVariant &item : supportedLanguages()) {
        const QVariantMap lang = item.toMap();
        if (lang.value(QStringLiteral("code")).toString() == code)
            return lang.value(QStringLiteral("name")).toString();
    }
    return code;
}

QVariantList filterMediaByLanguage(const QVariantList &mediaList, const QString &languageCode)
{
    QVariantList filtered;
    const QString marker = QStringLiteral("_%1_").arg(normalizeLanguageCode(languageCode));

    for (const QVariant &item : mediaList) {
        const QVariantMap media = item.toMap();
        const QString fileName = fileNameFromMediaMap(media);
        if (fileName.contains(marker, Qt::CaseInsensitive))
            filtered.append(item);
    }

    return filtered;
}

QVariantMap findSongFile(int songNumber, const QString &languageCode, const QVariantList &mediaLibrary)
{
    QVariantMap result;
    result.insert(QStringLiteral("found"), false);

    if (songNumber <= 0)
        return result;

    const QString code = normalizeLanguageCode(languageCode);
    const QString paddedNumber = QStringLiteral("%1").arg(songNumber, 3, 10, QLatin1Char('0'));
    const QString languageMarker = QStringLiteral("_%1_").arg(code);
    const QString songMarker = QStringLiteral("_%1").arg(paddedNumber);
    const QString exactStem = QStringLiteral("sjjm_%1_%2").arg(code, paddedNumber);

    for (const QVariant &item : mediaLibrary) {
        const QVariantMap media = item.toMap();
        const QString fileName = fileNameFromMediaMap(media);
        if (fileName.isEmpty() || !isMediaFile(fileName))
            continue;

        const bool matchesLanguage = fileName.contains(languageMarker, Qt::CaseInsensitive);
        const bool matchesSong = containsSongStem(fileName, exactStem)
            || (fileName.contains(songMarker, Qt::CaseInsensitive)
                && media.value(QStringLiteral("songNumber")).toInt() == songNumber);

        if (!matchesLanguage || !matchesSong)
            continue;

        result = media;
        result.insert(QStringLiteral("found"), true);
        result.insert(QStringLiteral("name"), fileName);
        result.insert(QStringLiteral("code"), code);
        result.insert(QStringLiteral("songNumber"), songNumber);
        result.insert(QStringLiteral("languageName"), languageNameForCode(code));
        if (!result.contains(QStringLiteral("path")))
            result.insert(QStringLiteral("path"), media.value(QStringLiteral("absolutePath")));
        return result;
    }

    result.insert(QStringLiteral("code"), code);
    result.insert(QStringLiteral("songNumber"), songNumber);
    result.insert(QStringLiteral("languageName"), languageNameForCode(code));
    return result;
}

} // namespace SongSearchUtils
