#include "WorkbookManager.h"
#include "BroadcastController.h"
#include "MeetingScheduleModel.h"
#include "SongSearchUtils.h"
#include "JwLibraryPaths.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTextDocument>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QCryptographicHash>
#include <QHash>
#include <QSet>
#include <cstring>
#include <climits>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

#include "miniz.h"
#include "miniz_zip.h"

namespace {

// Only src/*.cpp files besides main.cpp are compiled as separate
// translation units in this project's CMakeLists -- miniz's own .c files
// are added directly as sources there too, so no extra "compiled amalgam"
// wrapper is needed here; this file just calls the public mz_zip_reader_*
// API declared in miniz_zip.h.

bool isExtractableEntry(const QString &name)
{
    static const QStringList exts = {
        QStringLiteral(".xhtml"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".png"), QStringLiteral(".gif"), QStringLiteral(".svg")
    };
    for (const QString &ext : exts) {
        if (name.endsWith(ext, Qt::CaseInsensitive)) return true;
    }
    return false;
}

// Extracts every .xhtml text file plus every image referenced by articles
// (jpg/png/gif/svg under images/) -- the latter needed so a chosen article's
// own in-content photos/illustrations can be pulled out and shown in the app,
// not just its text.
QMap<QString, QByteArray> extractFilesFromEpub(const QByteArray &zipData)
{
    QMap<QString, QByteArray> result;
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, zipData.constData(), static_cast<size_t>(zipData.size()), 0))
        return result;

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;
        const QString name = QString::fromUtf8(stat.m_filename);
        if (!isExtractableEntry(name))
            continue;
        size_t size = 0;
        void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (data) {
            result.insert(name, QByteArray(reinterpret_cast<const char *>(data), static_cast<int>(size)));
            mz_free(data);
        }
    }
    mz_zip_reader_end(&zip);
    return result;
}

// toc.xhtml lists chapters in true reading/chronological order with the
// week's date range as the link text -- more reliable than sorting
// filenames (confirmed live: the Watchtower epub's own chapter filenames
// are NOT in chronological order, e.g. ...242.xhtml sorts before
// ...244.xhtml by name but reads after it in the real week sequence).
struct TocEntry { QString href; QString text; };

QList<TocEntry> parseToc(const QByteArray &tocXhtml)
{
    QList<TocEntry> entries;
    static const QRegularExpression linkRe(
        QStringLiteral(R"RX(<a\s+href="([^"]+\.xhtml)"[^>]*>([^<]+)</a>)RX"),
        QRegularExpression::CaseInsensitiveOption);
    const QString html = QString::fromUtf8(tocXhtml);
    auto it = linkRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString text = m.captured(2).trimmed();
        // Real dated week/article entries always contain a digit (the day
        // number); front-matter links ("Table of Contents", "Study
        // Edition", the full publication title) don't.
        bool hasDigit = false;
        for (const QChar &c : text) { if (c.isDigit()) { hasDigit = true; break; } }
        if (!hasDigit) continue;
        entries.append({m.captured(1), text});
    }
    return entries;
}

QString plainTextFromHtml(const QString &markup)
{
    QTextDocument document;
    document.setHtml(markup);
    return document.toPlainText().simplified();
}

bool inflateJwpubContent(const QByteArray &compressed, QString *html)
{
    constexpr mz_ulong maxOutputSize = 32 * 1024 * 1024;
    QByteArray candidate = compressed;

    for (int paddingAttempt = 0; paddingAttempt < 2; ++paddingAttempt) {
        mz_ulong capacity = 64 * 1024;
        while (capacity <= maxOutputSize) {
            QByteArray output(static_cast<int>(capacity), '\0');
            mz_ulong outputSize = capacity;
            const int result = mz_uncompress(
                reinterpret_cast<unsigned char *>(output.data()), &outputSize,
                reinterpret_cast<const unsigned char *>(candidate.constData()),
                static_cast<mz_ulong>(candidate.size()));
            if (result == MZ_OK) {
                output.resize(static_cast<int>(outputSize));
                *html = QString::fromUtf8(output);
                return true;
            }
            if (result != MZ_BUF_ERROR)
                break;
            if (capacity == maxOutputSize)
                break;
            capacity = qMin(capacity * 2, maxOutputSize);
        }

        if (paddingAttempt != 0 || candidate.isEmpty())
            break;
        const int paddingSize = static_cast<unsigned char>(candidate.back());
        if (paddingSize <= 0 || paddingSize > 16 || paddingSize > candidate.size())
            break;
        const QByteArray padding = candidate.right(paddingSize);
        if (padding != QByteArray(paddingSize, static_cast<char>(paddingSize)))
            break;
        candidate.chop(paddingSize);
    }

    return false;
}

bool decryptJwpubContent(const QByteArray &encrypted, int mepsLanguageIndex,
                        const QString &symbol, int year, const QString &issueTagNumber,
                        QString *html)
{
#ifdef Q_OS_WIN
    if (encrypted.isEmpty() || (encrypted.size() % 16) != 0
        || symbol.isEmpty() || mepsLanguageIndex < 0 || year <= 0) {
        qWarning() << "WorkbookManager: invalid local JWPUB content or publication metadata";
        return false;
    }

    QStringList keyParts{
        QString::number(mepsLanguageIndex),
        symbol,
        QString::number(year)
    };
    if (!issueTagNumber.isEmpty() && issueTagNumber != QStringLiteral("0"))
        keyParts.append(issueTagNumber);

    // JW Library derives the z-a content key from the publication metadata.
    QByteArray keyMaterial = QCryptographicHash::hash(
        keyParts.join(QLatin1Char('_')).toUtf8(), QCryptographicHash::Sha256);
    static const QByteArray xorKey = QByteArray::fromHex(
        "11cbb5587e32846d4c26790c633da289f66fe5842a3a585ce1bc3a294af5ada7");
    for (qsizetype i = 0; i < keyMaterial.size(); ++i)
        keyMaterial[i] = static_cast<char>(keyMaterial.at(i) ^ xorKey.at(i));

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qWarning() << "WorkbookManager: couldn't initialize AES for local workbook content"
                   << Qt::hex << status;
        return false;
    }

    status = BCryptSetProperty(
        algorithm, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_CBC)),
        sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (!BCRYPT_SUCCESS(status)) {
        qWarning() << "WorkbookManager: couldn't configure AES-CBC for local workbook content"
                   << Qt::hex << status;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    DWORD keyObjectSize = 0;
    DWORD resultSize = 0;
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&keyObjectSize),
                               sizeof(keyObjectSize), &resultSize, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qWarning() << "WorkbookManager: couldn't query AES key storage size" << Qt::hex << status;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    QByteArray keyObject(static_cast<qsizetype>(keyObjectSize), '\0');
    BCRYPT_KEY_HANDLE key = nullptr;
    status = BCryptGenerateSymmetricKey(
        algorithm, &key,
        reinterpret_cast<PUCHAR>(keyObject.data()), keyObjectSize,
        reinterpret_cast<PUCHAR>(keyMaterial.data()), 16, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qWarning() << "WorkbookManager: couldn't create AES key for local workbook content"
                   << Qt::hex << status;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    QByteArray iv = keyMaterial.mid(16, 16);
    QByteArray decrypted(encrypted.size(), '\0');
    ULONG decryptedSize = 0;
    status = BCryptDecrypt(
        key, reinterpret_cast<PUCHAR>(const_cast<char *>(encrypted.constData())),
        static_cast<ULONG>(encrypted.size()), nullptr,
        reinterpret_cast<PUCHAR>(iv.data()), static_cast<ULONG>(iv.size()),
        reinterpret_cast<PUCHAR>(decrypted.data()), static_cast<ULONG>(decrypted.size()),
        &decryptedSize, 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qWarning() << "WorkbookManager: couldn't decrypt local workbook content" << Qt::hex << status;
        return false;
    }
    decrypted.resize(static_cast<qsizetype>(decryptedSize));
    if (!inflateJwpubContent(decrypted, html)) {
        qWarning() << "WorkbookManager: couldn't decompress local workbook content";
        return false;
    }
    return true;
#else
    qWarning() << "WorkbookManager: local JWPUB assignment extraction is only supported on Windows";
    Q_UNUSED(encrypted)
    Q_UNUSED(mepsLanguageIndex)
    Q_UNUSED(symbol)
    Q_UNUSED(year)
    Q_UNUSED(issueTagNumber)
    Q_UNUSED(html)
    return false;
#endif
}

struct WorkbookAssignmentSpan {
    int number = 0;
    QString segmentId;
    int firstParagraph = 0;
    int endParagraph = INT_MAX;
};

struct WorkbookAssignmentData {
    QVariantList scheduleRows;
    QList<WorkbookAssignmentSpan> spans;
};

struct BookStudyAssignment {
    QString segmentId;
    QList<int> chapters;
};

WorkbookAssignmentData parseWorkbookAssignmentData(const QString &chapterHtml,
                                                    const QString &languageCode,
                                                    const QDate &weekStart)
{
    struct Heading {
        int level = 0;
        qsizetype start = 0;
        qsizetype end = 0;
        int paragraph = 0;
        QString markup;
    };

    static const QRegularExpression headingRe(
        QStringLiteral(R"(<h([23])\b([^>]*)>(.*?)</h\1\s*>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression paragraphRe(
        QStringLiteral(R"(\bid\s*=\s*["']p(\d+)["'])"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression numberRe(QStringLiteral(R"(^\s*(\d+)\s*[.)]\s*)"));
    static const QRegularExpression durationRe(
        QStringLiteral(R"(\b(\d{1,2})\s*(?:min(?:ute)?s?\.?)(?!\w))"),
        QRegularExpression::CaseInsensitiveOption);

    QList<Heading> headings;
    auto it = headingRe.globalMatch(chapterHtml);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QRegularExpressionMatch paragraphMatch = paragraphRe.match(match.captured(2));
        headings.append({
            match.captured(1).toInt(),
            match.capturedStart(),
            match.capturedEnd(),
            paragraphMatch.hasMatch() ? paragraphMatch.captured(1).toInt() : 0,
            match.captured(3)
        });
    }

    WorkbookAssignmentData result;
    const QString rowIdPrefix = QStringLiteral("mwb_%1_%2_")
        .arg(languageCode, weekStart.toString(QStringLiteral("yyyyMMdd")));
    for (int i = 0; i < headings.size(); ++i) {
        const Heading &heading = headings[i];
        if (heading.level != 3)
            continue;

        const QString title = plainTextFromHtml(heading.markup);
        const QRegularExpressionMatch numberMatch = numberRe.match(title);
        if (!numberMatch.hasMatch())
            continue;
        const int number = numberMatch.captured(1).toInt();

        QString segmentId;
        if (number >= 1 && number <= 3)
            segmentId = QStringLiteral("m3");
        else if (number >= 4 && number <= 9)
            segmentId = rowIdPrefix + QString::number(number);
        if (segmentId.isEmpty())
            continue;

        int nextHeadingIndex = i + 1;
        while (nextHeadingIndex < headings.size()
               && headings[nextHeadingIndex].level != 2
               && headings[nextHeadingIndex].level != 3) {
            ++nextHeadingIndex;
        }
        const int endParagraph = nextHeadingIndex < headings.size()
            ? headings[nextHeadingIndex].paragraph : INT_MAX;
        result.spans.append({number, segmentId, heading.paragraph, endParagraph});

        if (number < 4 || number > 9)
            continue;

        const qsizetype partEnd = nextHeadingIndex < headings.size()
            ? headings[nextHeadingIndex].start : chapterHtml.size();
        const QString body = plainTextFromHtml(
            chapterHtml.mid(heading.end, partEnd - heading.end));
        const QRegularExpressionMatch durationMatch =
            durationRe.match(title + QLatin1Char(' ') + body);
        const int durationMinutes = durationMatch.hasMatch()
            ? durationMatch.captured(1).toInt() : 0;
        result.scheduleRows.append(QVariantMap{
            {QStringLiteral("number"), number},
            {QStringLiteral("id"), segmentId},
            {QStringLiteral("title"), title},
            {QStringLiteral("durationMinutes"), durationMinutes}
        });
    }
    return result;
}

BookStudyAssignment parseBookStudyAssignment(const QString &chapterHtml,
                                             const WorkbookAssignmentData &assignmentData)
{
    const auto studySpan = std::find_if(
        assignmentData.spans.cbegin(), assignmentData.spans.cend(),
        [](const WorkbookAssignmentSpan &span) { return span.number == 9; });
    if (studySpan == assignmentData.spans.cend())
        return {};

    static const QRegularExpression headingRe(
        QStringLiteral(R"(<h([23])\b([^>]*)>(.*?)</h\1\s*>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression numberRe(QStringLiteral(R"(^\s*(\d+)\s*[.)]\s*)"));
    static const QRegularExpression chapterRe(
        QStringLiteral(R"(\bwcg\b[^0-9]{0,40}\bchap(?:ter)?s?\.?\s*(\d{1,3})(?:\s*(?:-|–|—|,|and|&|to)\s*(\d{1,3}))?)"),
        QRegularExpression::CaseInsensitiveOption);

    QList<QRegularExpressionMatch> headings;
    auto it = headingRe.globalMatch(chapterHtml);
    while (it.hasNext())
        headings.append(it.next());

    BookStudyAssignment result;
    result.segmentId = studySpan->segmentId;
    for (int i = 0; i < headings.size(); ++i) {
        const QRegularExpressionMatch &heading = headings.at(i);
        if (heading.captured(1).toInt() != 3)
            continue;
        const QRegularExpressionMatch numberMatch =
            numberRe.match(plainTextFromHtml(heading.captured(3)));
        if (!numberMatch.hasMatch() || numberMatch.captured(1).toInt() != 9)
            continue;

        int nextHeading = i + 1;
        while (nextHeading < headings.size()
               && headings.at(nextHeading).captured(1).toInt() != 2
               && headings.at(nextHeading).captured(1).toInt() != 3) {
            ++nextHeading;
        }
        const qsizetype sectionEnd = nextHeading < headings.size()
            ? headings.at(nextHeading).capturedStart() : chapterHtml.size();
        const QString sectionText = plainTextFromHtml(
            heading.captured(3) + QLatin1Char(' ')
            + chapterHtml.mid(heading.capturedEnd(), sectionEnd - heading.capturedEnd()));
        const QRegularExpressionMatch chapterMatch = chapterRe.match(sectionText);
        if (!chapterMatch.hasMatch())
            return result;

        const int firstChapter = chapterMatch.captured(1).toInt();
        const int lastChapter = chapterMatch.captured(2).isEmpty()
            ? firstChapter : chapterMatch.captured(2).toInt();
        if (firstChapter < 1 || lastChapter < firstChapter
            || lastChapter - firstChapter > 4) {
            qWarning() << "WorkbookManager: invalid WCG chapter range in Bible Study assignment:"
                       << chapterMatch.captured(0);
            return result;
        }
        for (int chapter = firstChapter; chapter <= lastChapter; ++chapter)
            result.chapters.append(chapter);
        return result;
    }
    return result;
}

QString localVideoPathForMultimedia(const QString &keySymbol, int track,
                                    int mepsLanguageIndex, int issueTagNumber)
{
    for (const QString &packageRoot : JwLibraryPaths::packageRoots()) {
        const QString dbPath = packageRoot + QStringLiteral("/LocalState/Data/mediaCollection.db");
        if (!QFileInfo::exists(dbPath))
            continue;

        const QString connectionName = QUuid::createUuid().toString();
        QString resolvedPath;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                qWarning() << "WorkbookManager: couldn't open JW Library media catalog:"
                           << db.lastError().text();
            } else {
                {
                    QSqlQuery query(db);
                    query.prepare(QStringLiteral(
                        "SELECT v.FilePath FROM MediaKey mk "
                        "JOIN Video v ON v.MediaKeyId = mk.MediaKeyId "
                        "WHERE mk.KeySymbol = ? AND mk.Track = ? "
                        "AND mk.MepsLanguageIndex = ? AND mk.IssueTagNumber = ? "
                        "ORDER BY v.BitRate DESC"));
                    query.addBindValue(keySymbol);
                    query.addBindValue(track);
                    query.addBindValue(mepsLanguageIndex);
                    query.addBindValue(issueTagNumber);
                    if (!query.exec()) {
                        qWarning() << "WorkbookManager: couldn't query JW Library media catalog:"
                                   << query.lastError().text();
                    } else {
                        while (query.next()) {
                            const QString path = query.value(0).toString();
                            if (QFileInfo::exists(path)) {
                                resolvedPath = path;
                                break;
                            }
                        }
                    }
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
        if (!resolvedPath.isEmpty())
            return resolvedPath;
    }
    return {};
}

// English/French/Spanish month names -- the three Latin-script built-in
// languages where a text match is realistically reliable without pulling
// in full QLocale-based localized parsing for every possible language
// (including custom ones the user might add, which QLocale often can't
// name at all, e.g. Twi/Ga/Ewe). Falls back to position-based selection
// (see pickCurrentWeekChapter) when the month word isn't recognized here.
int monthNumberFromWord(const QString &word)
{
    static const QMap<QString, int> months = {
        {"january", 1}, {"february", 2}, {"march", 3}, {"april", 4}, {"may", 5}, {"june", 6},
        {"july", 7}, {"august", 8}, {"september", 9}, {"october", 10}, {"november", 11}, {"december", 12},
        {"janvier", 1}, {"février", 2}, {"fevrier", 2}, {"mars", 3}, {"avril", 4}, {"mai", 5}, {"juin", 6},
        {"juillet", 7}, {"août", 8}, {"aout", 8}, {"septembre", 9}, {"octobre", 10}, {"novembre", 11}, {"décembre", 12}, {"decembre", 12},
        {"enero", 1}, {"febrero", 2}, {"marzo", 3}, {"abril", 4}, {"mayo", 5}, {"junio", 6},
        {"julio", 7}, {"agosto", 8}, {"septiembre", 9}, {"octubre", 10}, {"noviembre", 11}, {"diciembre", 12},
    };
    return months.value(word.toLower(), 0);
}

// Best-effort week-start date from a toc entry's link text (e.g. "January
// 19-25", "MARCH 30–APRIL 5, 2026"). Returns an invalid QDate if the month
// word isn't recognized -- callers fall back to position-based selection.
QDate parseWeekStart(const QString &text, int fallbackYear)
{
    static const QRegularExpression re(R"(([A-Za-zÀ-ÿ]+)\s+(\d{1,2})(?!\d))");
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) return {};
    const int month = monthNumberFromWord(m.captured(1));
    const int day = m.captured(2).toInt();
    if (month < 1 || day < 1) return {};

    int year = fallbackYear;
    static const QRegularExpression yearRe(R"(\b(20\d{2})\b)");
    const QRegularExpressionMatch ym = yearRe.match(text);
    if (ym.hasMatch()) year = ym.captured(1).toInt();

    QDate d(year, month, day);
    return d.isValid() ? d : QDate();
}

// JW Library's package folder is found by scanning %LOCALAPPDATA%\Packages
// rather than a hardcoded name -- see JwLibraryPaths.h. Shared with
// MediaExtractor, which scans the same package folders for local media.
QStringList publicationsRoots()
{
    return JwLibraryPaths::publicationsDirs();
}

// Folders are named "<pub>_<langCode>_<issue>", e.g. "mwb_EW_202609" --
// there can be several cached issues at once, so this returns all of them
// and callers pick whichever issue's own content actually matches today.
QList<QDir> matchingPublicationDirs(const QString &pubPrefix, const QString &lang)
{
    QList<QDir> dirs;
    const QString filter = pubPrefix + QLatin1Char('_') + lang + QStringLiteral("_*");
    for (const QString &root : publicationsRoots()) {
        QDir rootDir(root);
        if (!rootDir.exists()) continue;
        const QFileInfoList entries = rootDir.entryInfoList({filter}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) dirs.append(QDir(fi.absoluteFilePath()));
    }
    return dirs;
}

} // namespace

WorkbookManager::WorkbookManager(BroadcastController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(12 * 60 * 60 * 1000); // check twice a day; issues only change weekly/monthly anyway
    connect(m_timer, &QTimer::timeout, this, &WorkbookManager::refreshNow);
}

void WorkbookManager::startAutoRefresh()
{
    m_timer->start();
    refreshNow();
}

void WorkbookManager::setStatus(const QString &text)
{
    if (m_status == text) return;
    m_status = text;
    emit statusChanged();
}

QString WorkbookManager::currentLanguageCode() const
{
    return m_controller ? m_controller->currentLanguageCode() : QStringLiteral("E");
}

QString WorkbookManager::mwbIssueFor(const QDate &date)
{
    // Bimonthly, starting on odd months (Jan/Feb, Mar/Apr, ... Nov/Dec) --
    // never crosses a calendar year boundary, so no year adjustment needed.
    int month = date.month();
    if (month % 2 == 0) month -= 1;
    return QStringLiteral("%1%2").arg(date.year()).arg(month, 2, 10, QLatin1Char('0'));
}

QString WorkbookManager::wIssueFor(const QDate &date)
{
    // The Watchtower Study edition is dated ~2 months ahead of the week its
    // articles are actually studied -- confirmed live: the September 2026
    // issue's articles carry November 2026 study dates, while this week's
    // (studied in September) article is inside the July 2026 issue. So the
    // issue to fetch for a given study date is 2 months *earlier*.
    const QDate issueDate = date.addMonths(-2);
    return QStringLiteral("%1%2").arg(issueDate.year()).arg(issueDate.month(), 2, 10, QLatin1Char('0'));
}

void WorkbookManager::refreshNow()
{
    setStatus(tr("Checking for this week's workbook…"));
    const QString lang = currentLanguageCode();
    const QDate targetDate = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    if (m_applyPartsLanguage != lang || m_applyPartsDate != targetDate) {
        m_controller->setWorkbookScheduleRows({});
        m_controller->clearWorkbookSongLinks();
        for (const QString &segmentId : {QStringLiteral("m3"), QStringLiteral("m6"),
                                         QStringLiteral("m8"), QStringLiteral("m9")}) {
            m_controller->setWorkbookMediaForSegment(segmentId, {});
        }
        m_applyPartsLanguage = lang;
        m_applyPartsDate = targetDate;
        m_applyPartsLoaded = false;
    }
    applyFromLocalBookStudy(lang);
    // Prefer the local workbook database for song linking, week matching, and
    // the timed Apply Yourself assignments when its content can be decoded.
    const bool hasLocalMwb = applyFromLocalMwb(lang, targetDate);
    if (!hasLocalMwb && !m_selectedDate.isValid())
        fetchPublication(QStringLiteral("mwb"));
    else if (!m_applyPartsLoaded)
        fetchPublication(QStringLiteral("mwb"), hasLocalMwb);
    if (!applyFromLocalWatchtower(lang, targetDate) && !m_selectedDate.isValid())
        fetchPublication(QStringLiteral("w"));
}

void WorkbookManager::selectWeek(const QDate &date)
{
    m_selectedDate = date;
    refreshNow();
}

QVariantList WorkbookManager::availableWeeks() const
{
    const QString lang = currentLanguageCode();
    QSet<QDate> dates;

    for (const QString &pubPrefix : {QStringLiteral("mwb"), QStringLiteral("w")}) {
        for (const QDir &dir : matchingPublicationDirs(pubPrefix, lang)) {
            const QStringList dbFiles = dir.entryList({QStringLiteral("*.db")}, QDir::Files);
            if (dbFiles.isEmpty()) continue;
            const QString dbPath = dir.absoluteFilePath(dbFiles.first());
            const QString connName = QUuid::createUuid().toString();
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
                db.setDatabaseName(dbPath);
                if (db.open()) {
                    // QSqlQuery scoped separately so its destructor (which
                    // finalizes the underlying SQLite statement) runs before
                    // db.close() -- calling close() while the query object
                    // is still alive, even after .clear(), is what Qt's
                    // "connection ... is still in use" warning is about.
                    {
                        QSqlQuery q(db);
                        // mwb's date text is in Title; w's is in the separate
                        // ContextTitle column (see applyFromLocal{Mwb,Watchtower}).
                        const bool isW = (pubPrefix == QStringLiteral("w"));
                        if (q.exec(isW ? QStringLiteral("SELECT ContextTitle FROM Document")
                                       : QStringLiteral("SELECT Title FROM Document"))) {
                            while (q.next()) {
                                const QDate start = parseWeekStart(q.value(0).toString(), QDate::currentDate().year());
                                if (start.isValid()) dates.insert(start);
                            }
                        }
                    }
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connName);
        }
    }

    QList<QDate> sorted = dates.values();
    const QDate today = QDate::currentDate();
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate yearEnd(today.year(), 12, 31);
    const int daysUntilMonday = (8 - monthStart.dayOfWeek()) % 7;
    for (QDate weekStart = monthStart.addDays(daysUntilMonday);
         weekStart <= yearEnd;
         weekStart = weekStart.addDays(7)) {
        dates.insert(weekStart);
    }

    sorted = dates.values();
    std::sort(sorted.begin(), sorted.end());

    QVariantList result;
    for (const QDate &start : sorted) {
        if (start < monthStart || start > yearEnd)
            continue;
        QVariantMap entry;
        entry["iso"] = start.toString(Qt::ISODate);
        entry["label"] = start.toString(QStringLiteral("MMM d")) + QStringLiteral(" – ")
            + start.addDays(6).toString(QStringLiteral("MMM d, yyyy"));
        result.append(entry);
    }
    return result;
}

bool WorkbookManager::applyFromLocalBookStudy(const QString &lang)
{
    m_bookStudyPublicationFolder.clear();
    const QString folderName = QStringLiteral("wcg_") + lang;
    for (const QString &root : publicationsRoots()) {
        QDir rootDir(root);
        if (!rootDir.exists()) continue;
        const QFileInfoList entries = rootDir.entryInfoList(
            {folderName}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) {
            QFile manifest(fi.absoluteFilePath() + QStringLiteral("/manifest.json"));
            if (!manifest.open(QIODevice::ReadOnly)) continue;

            const QJsonObject pub = QJsonDocument::fromJson(manifest.readAll())
                .object().value(QStringLiteral("publication")).toObject();
            if (pub.value(QStringLiteral("publicationType")).toString() != QStringLiteral("Book")
                || pub.value(QStringLiteral("uniqueEnglishSymbol")).toString()
                    != QStringLiteral("wcg"))
                continue;

            m_bookStudyPublicationFolder = fi.absoluteFilePath();
            m_controller->setWorkbookPublicationFolder(QStringLiteral("cbs"), fi.absoluteFilePath());
            return true;
        }
    }

    m_controller->setWorkbookPublicationFolder(QStringLiteral("cbs"), {});
    return false;
}

QStringList WorkbookManager::bookStudyImages(const QString &lang,
                                             const QList<int> &chapters,
                                             const QString &segmentId)
{
    QStringList mediaIds;
    if (chapters.isEmpty())
        return mediaIds;
    if (m_bookStudyPublicationFolder.isEmpty()) {
        qWarning() << "WorkbookManager: cannot link WCG chapter images; publication is not downloaded for"
                   << lang;
        return mediaIds;
    }

    const QStringList dbFiles = QDir(m_bookStudyPublicationFolder)
        .entryList({QStringLiteral("wcg_*.db")}, QDir::Files);
    if (dbFiles.isEmpty()) {
        qWarning() << "WorkbookManager: WCG publication database is missing from"
                   << m_bookStudyPublicationFolder;
        return mediaIds;
    }

    const QString dbPath = QDir(m_bookStudyPublicationFolder).absoluteFilePath(dbFiles.first());
    const QString connectionName = QUuid::createUuid().toString();
    QSet<QString> seenPaths;
    int linkedImageCount = 0;
    int missingImageCount = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            qWarning() << "WorkbookManager: couldn't open WCG publication database:"
                       << db.lastError().text();
        } else {
            for (const int chapter : chapters) {
                int documentId = -1;
                qint64 mepsDocumentId = 0;
                {
                    QSqlQuery documentQuery(db);
                    documentQuery.prepare(QStringLiteral(
                        "SELECT d.DocumentId, d.MepsDocumentId "
                        "FROM Document d JOIN Publication p ON p.PublicationId = d.PublicationId "
                        "WHERE d.ChapterNumber = ? AND LOWER(p.UniqueEnglishSymbol) = 'wcg' "
                        "ORDER BY d.DocumentId LIMIT 1"));
                    documentQuery.addBindValue(chapter);
                    if (documentQuery.exec() && documentQuery.next()) {
                        documentId = documentQuery.value(0).toInt();
                        mepsDocumentId = documentQuery.value(1).toLongLong();
                    } else if (documentQuery.lastError().isValid()) {
                        qWarning() << "WorkbookManager: couldn't find WCG chapter" << chapter
                                   << "in the local publication:"
                                   << documentQuery.lastError().text();
                    }
                }
                if (documentId < 0 || mepsDocumentId <= 0) {
                    qWarning() << "WorkbookManager: WCG chapter" << chapter
                               << "is unavailable in the local publication";
                    continue;
                }

                QSqlQuery imageQuery(db);
                imageQuery.prepare(QStringLiteral(
                    "SELECT m.FilePath, m.MimeType FROM DocumentMultimedia dm "
                    "JOIN Multimedia m ON m.MultimediaId = dm.MultimediaId "
                    "WHERE dm.DocumentId = ? AND LOWER(m.MimeType) LIKE 'image/%' "
                    "ORDER BY dm.DocumentMultimediaId"));
                imageQuery.addBindValue(documentId);
                if (!imageQuery.exec()) {
                    qWarning() << "WorkbookManager: couldn't read WCG chapter image references:"
                               << imageQuery.lastError().text();
                    continue;
                }

                while (imageQuery.next()) {
                    const QString relativePath = imageQuery.value(0).toString();
                    const QString mimeType = imageQuery.value(1).toString();
                    const QFileInfo relativeInfo(relativePath);
                    const QString suffix = relativeInfo.suffix().toLower();
                    if (!mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive)
                        || (suffix != QStringLiteral("jpg")
                            && suffix != QStringLiteral("jpeg")
                            && suffix != QStringLiteral("png")
                            && suffix != QStringLiteral("webp"))) {
                        continue;
                    }

                    // WCG's chapter-specific illustrations carry the chapter
                    // document's MEPS id in their filename. The DB also
                    // associates some shared publication media with each
                    // chapter, so exclude those rather than showing them as
                    // if they belonged to the selected chapter.
                    const QString expectedPrefix =
                        QString::number(mepsDocumentId) + QLatin1Char('_');
                    if (!relativeInfo.fileName().startsWith(expectedPrefix))
                        continue;

                    QString path = relativePath;
                    if (QDir::isRelativePath(path))
                        path = QDir(m_bookStudyPublicationFolder).filePath(path);
                    const QFileInfo imageInfo(path);
                    const QString absolutePath = imageInfo.absoluteFilePath();
                    if (seenPaths.contains(absolutePath))
                        continue;
                    seenPaths.insert(absolutePath);
                    if (!imageInfo.exists() || !imageInfo.isFile()) {
                        ++missingImageCount;
                        qWarning() << "WorkbookManager: WCG chapter illustration is unavailable:"
                                   << absolutePath;
                        continue;
                    }

                    const QString mediaId = m_controller->ensureWorkbookMedia(absolutePath);
                    if (!mediaId.isEmpty() && !mediaIds.contains(mediaId)) {
                        mediaIds.append(mediaId);
                        ++linkedImageCount;
                    }
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    if (linkedImageCount == 0) {
        qWarning() << "WorkbookManager: no supported local images were linked for WCG chapter(s)"
                   << chapters << "to segment" << segmentId;
    } else {
        qInfo() << "WorkbookManager: linked" << linkedImageCount
                << "chapter-specific WCG images for chapter(s)" << chapters
                << "to segment" << segmentId;
    }
    if (missingImageCount > 0) {
        qWarning() << "WorkbookManager:" << missingImageCount
                   << "WCG chapter image file(s) were missing on disk";
    }
    return mediaIds;
}

bool WorkbookManager::applyFromLocalMwb(const QString &lang, const QDate &targetDate)
{
    struct Best { QString dbPath; int docId = -1; QString title; int diff = INT_MAX; };
    Best best;

    for (const QDir &dir : matchingPublicationDirs(QStringLiteral("mwb"), lang)) {
        const QStringList dbFiles = dir.entryList({QStringLiteral("*.db")}, QDir::Files);
        if (dbFiles.isEmpty()) continue;
        const QString dbPath = dir.absoluteFilePath(dbFiles.first());
        const QString connName = QUuid::createUuid().toString();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            if (db.open()) {
                {
                    QSqlQuery q(db);
                    // mwb has no separate date field -- the week's own Title
                    // *is* the date range (e.g. "September 28-October 4").
                    if (q.exec(QStringLiteral("SELECT DocumentId, Title FROM Document ORDER BY DocumentId"))) {
                        while (q.next()) {
                            const int docId = q.value(0).toInt();
                            const QString title = q.value(1).toString();
                            const QDate start = parseWeekStart(title, targetDate.year());
                            if (!start.isValid()) continue;
                            // Must be the week that actually contains
                            // targetDate (Monday on/before it, within 7
                            // days) -- nearest absolute distance would
                            // otherwise roll over as early as Thursday.
                            const qint64 daysFromStart = start.daysTo(targetDate);
                            if (daysFromStart < 0 || daysFromStart >= 7) continue;
                            const int diff = static_cast<int>(daysFromStart);
                            if (diff < best.diff) best = {dbPath, docId, title, diff};
                        }
                    }
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connName);
    }

    if (best.docId < 0) return false;

    m_controller->setWorkbookPublicationFolder(QStringLiteral("mwb"), QFileInfo(best.dbPath).absolutePath());

    QList<int> songNumbers;
    {
        const QString connName = QUuid::createUuid().toString();
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(best.dbPath);
        if (db.open()) {
            {
                QSqlQuery q(db);
                // "sjjm" is the song-video/audio publication symbol; its Track
                // number is the song number, in document (opening/middle/
                // concluding) order -- confirmed against a real local database.
                // Capped at 3 via LIMIT so an unrelated extra sjjm-tagged row
                // elsewhere in the document can't silently shift which song
                // lands on which segment (m1/m7/m11).
                q.prepare(QStringLiteral(
                    "SELECT m.Track FROM DocumentMultimedia dm "
                    "JOIN Multimedia m ON dm.MultimediaId = m.MultimediaId "
                    "WHERE dm.DocumentId = ? AND m.KeySymbol = 'sjjm' "
                    "ORDER BY dm.MultimediaId LIMIT 3"));
                q.addBindValue(best.docId);
                if (q.exec()) { while (q.next()) songNumbers.append(q.value(0).toInt()); }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }
    if (songNumbers.size() != 3) {
        qWarning() << "WorkbookManager: expected 3 songs (opening/middle/concluding) in mwb document, found" << songNumbers.size();
    }

    if (songNumbers.size() > 0) m_controller->resolveWeeklySong(songNumbers[0], lang, QStringLiteral("m1"));
    if (songNumbers.size() > 1) m_controller->resolveWeeklySong(songNumbers[1], lang, QStringLiteral("m7"));
    if (songNumbers.size() > 2) m_controller->resolveWeeklySong(songNumbers[2], lang, QStringLiteral("m11"));

    if (!m_applyPartsLoaded) {
        const QString connName = QUuid::createUuid().toString();
        QByteArray encryptedChapter;
        int mepsLanguageIndex = 0;
        int publicationYear = 0;
        int issueTagNumber = 0;
        QString publicationSymbol;
        QList<QVariantMap> multimedia;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(best.dbPath);
            if (db.open()) {
                {
                    QSqlQuery query(db);
                    query.prepare(QStringLiteral(
                        "SELECT d.Content, p.MepsLanguageIndex, p.Symbol, p.Year, p.IssueTagNumber "
                        "FROM Document d JOIN Publication p ON d.PublicationId = p.PublicationId "
                        "WHERE d.DocumentId = ?"));
                    query.addBindValue(best.docId);
                    if (query.exec() && query.next()) {
                        encryptedChapter = query.value(0).toByteArray();
                        mepsLanguageIndex = query.value(1).toInt();
                        publicationSymbol = query.value(2).toString();
                        publicationYear = query.value(3).toInt();
                        issueTagNumber = query.value(4).toInt();
                    } else {
                        qWarning() << "WorkbookManager: couldn't read selected local workbook content:"
                                   << query.lastError().text();
                    }
                }

                {
                    QSqlQuery query(db);
                    query.prepare(QStringLiteral(
                        "SELECT m.MimeType, m.FilePath, m.KeySymbol, m.Track, "
                        "m.MepsLanguageIndex, m.IssueTagNumber, dm.BeginParagraphOrdinal "
                        "FROM DocumentMultimedia dm "
                        "JOIN Multimedia m ON m.MultimediaId = dm.MultimediaId "
                        "WHERE dm.DocumentId = ? ORDER BY dm.DocumentMultimediaId"));
                    query.addBindValue(best.docId);
                    if (query.exec()) {
                        while (query.next()) {
                            multimedia.append(QVariantMap{
                                {QStringLiteral("mimeType"), query.value(0).toString()},
                                {QStringLiteral("filePath"), query.value(1).toString()},
                                {QStringLiteral("keySymbol"), query.value(2).toString()},
                                {QStringLiteral("track"), query.value(3).toInt()},
                                {QStringLiteral("languageIndex"), query.value(4).toInt()},
                                {QStringLiteral("issueTagNumber"), query.value(5).toInt()},
                                {QStringLiteral("paragraph"), query.value(6).toInt()}
                            });
                        }
                    } else {
                        qWarning() << "WorkbookManager: couldn't read local workbook media references:"
                                   << query.lastError().text();
                    }
                }
                db.close();
            } else {
                qWarning() << "WorkbookManager: couldn't reopen local workbook for assignment content:"
                           << db.lastError().text();
            }
        }
        QSqlDatabase::removeDatabase(connName);

        QString chapterHtml;
        if (!encryptedChapter.isEmpty()
            && decryptJwpubContent(encryptedChapter, mepsLanguageIndex, publicationSymbol,
                                   publicationYear, QString::number(issueTagNumber), &chapterHtml)) {
            QDate weekStart = parseWeekStart(best.title, targetDate.year());
            if (!weekStart.isValid())
                weekStart = targetDate.addDays(1 - targetDate.dayOfWeek());
            const WorkbookAssignmentData assignmentData =
                parseWorkbookAssignmentData(chapterHtml, lang, weekStart);
            m_controller->setWorkbookScheduleRows(assignmentData.scheduleRows);
            m_applyPartsLoaded = true;

            QHash<QString, QStringList> linkedMedia;
            for (const QVariantMap &item : multimedia) {
                const QString mimeType = item.value(QStringLiteral("mimeType")).toString();
                const QString keySymbol = item.value(QStringLiteral("keySymbol")).toString();
                if (keySymbol.compare(QStringLiteral("sjjm"), Qt::CaseInsensitive) == 0)
                    continue;
                if (!mimeType.startsWith(QStringLiteral("image/"))
                    && !mimeType.startsWith(QStringLiteral("video/")))
                    continue;

                const int paragraph = item.value(QStringLiteral("paragraph")).toInt();
                if (paragraph <= 0)
                    continue;
                const auto span = std::find_if(
                    assignmentData.spans.cbegin(), assignmentData.spans.cend(),
                    [paragraph](const WorkbookAssignmentSpan &candidate) {
                        return candidate.firstParagraph > 0
                            && paragraph >= candidate.firstParagraph
                            && paragraph < candidate.endParagraph;
                    });
                if (span == assignmentData.spans.cend())
                    continue;

                QString path = item.value(QStringLiteral("filePath")).toString();
                if (mimeType.startsWith(QStringLiteral("image/"))) {
                    if (QDir::isRelativePath(path))
                        path = QDir(QFileInfo(best.dbPath).absolutePath()).filePath(path);
                } else {
                    path = localVideoPathForMultimedia(
                        keySymbol, item.value(QStringLiteral("track")).toInt(),
                        item.value(QStringLiteral("languageIndex")).toInt(),
                        item.value(QStringLiteral("issueTagNumber")).toInt());
                }
                if (path.isEmpty() || !QFileInfo::exists(path))
                    continue;

                const QString mediaId = m_controller->ensureWorkbookMedia(path);
                if (!mediaId.isEmpty() && !linkedMedia[span->segmentId].contains(mediaId))
                    linkedMedia[span->segmentId].append(mediaId);
            }

            QStringList segmentIds{QStringLiteral("m3")};
            for (const QVariant &rowValue : assignmentData.scheduleRows)
                segmentIds.append(rowValue.toMap().value(QStringLiteral("id")).toString());

            const BookStudyAssignment studyAssignment =
                parseBookStudyAssignment(chapterHtml, assignmentData);
            if (!studyAssignment.chapters.isEmpty()) {
                const QStringList chapterMedia = bookStudyImages(
                    lang, studyAssignment.chapters, studyAssignment.segmentId);
                for (const QString &mediaId : chapterMedia) {
                    if (!linkedMedia[studyAssignment.segmentId].contains(mediaId))
                        linkedMedia[studyAssignment.segmentId].append(mediaId);
                }
            } else if (!studyAssignment.segmentId.isEmpty()) {
                qWarning() << "WorkbookManager: couldn't identify a WCG chapter in Bible Study assignment"
                           << studyAssignment.segmentId << "for" << best.title;
            }

            for (const QString &segmentId : segmentIds) {
                m_controller->setWorkbookMediaForSegment(segmentId, linkedMedia.value(segmentId));
            }
            m_controller->persistWorkbookChanges();
            int linkedMediaCount = 0;
            for (auto it = linkedMedia.cbegin(); it != linkedMedia.cend(); ++it)
                linkedMediaCount += it.value().size();
            qInfo() << "WorkbookManager: loaded" << assignmentData.scheduleRows.size()
                    << "independent timed meeting rows from the local workbook for" << best.title
                    << "and linked" << linkedMediaCount
                    << "non-song media files";
        }
    }

    setStatus(tr("Meeting Workbook loaded: %1").arg(best.title));
    return true;
}

bool WorkbookManager::applyFromLocalWatchtower(const QString &lang, const QDate &targetDate)
{
    struct Best { QString folder, dbPath; int docId = -1; QString title; int diff = INT_MAX; };
    Best best;

    for (const QDir &dir : matchingPublicationDirs(QStringLiteral("w"), lang)) {
        const QStringList dbFiles = dir.entryList({QStringLiteral("*.db")}, QDir::Files);
        if (dbFiles.isEmpty()) continue;
        const QString dbPath = dir.absoluteFilePath(dbFiles.first());
        const QString connName = QUuid::createUuid().toString();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            if (db.open()) {
                {
                    QSqlQuery q(db);
                    // Unlike mwb, the Watchtower article's Title is its real
                    // headline -- the study date lives in the separate
                    // ContextTitle column instead (e.g. "SEPTEMBER 28-OCTOBER
                    // 4, 2026"), confirmed against a real local database.
                    if (q.exec(QStringLiteral("SELECT DocumentId, Title, ContextTitle FROM Document ORDER BY DocumentId"))) {
                        while (q.next()) {
                            const int docId = q.value(0).toInt();
                            const QString title = q.value(1).toString();
                            const QString contextTitle = q.value(2).toString();
                            if (contextTitle.isEmpty()) continue;
                            const QDate start = parseWeekStart(contextTitle, targetDate.year());
                            if (!start.isValid()) continue;
                            // Same week-containment fix as the mwb lookup
                            // above -- only a week whose Monday is on or
                            // before targetDate, within 7 days, may win.
                            const qint64 daysFromStart = start.daysTo(targetDate);
                            if (daysFromStart < 0 || daysFromStart >= 7) continue;
                            const int diff = static_cast<int>(daysFromStart);
                            if (diff < best.diff) best = {dir.absolutePath(), dbPath, docId, title, diff};
                        }
                    }
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connName);
    }

    if (best.docId < 0) return false;

    m_controller->setWorkbookPublicationFolder(QStringLiteral("w"), best.folder);

    QList<int> songNumbers;
    QList<QPair<QString, QString>> images;
    {
        const QString connName = QUuid::createUuid().toString();
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(best.dbPath);
        if (db.open()) {
            {
                QSqlQuery sq(db);
                // Capped at 2 via LIMIT -- see the comment below on why this
                // article only ever carries the middle/closing songs -- so an
                // unrelated extra sjjm-tagged row can't silently shift w6 to
                // the wrong song.
                sq.prepare(QStringLiteral(
                    "SELECT m.Track FROM DocumentMultimedia dm "
                    "JOIN Multimedia m ON dm.MultimediaId = m.MultimediaId "
                    "WHERE dm.DocumentId = ? AND m.KeySymbol = 'sjjm' "
                    "ORDER BY dm.MultimediaId LIMIT 2"));
                sq.addBindValue(best.docId);
                if (sq.exec()) { while (sq.next()) songNumbers.append(sq.value(0).toInt()); }
                else {
                    qWarning() << "WorkbookManager: couldn't read Watchtower song references:"
                               << sq.lastError().text();
                }
            }
            {
                QSqlQuery imageQuery(db);
                imageQuery.prepare(QStringLiteral(
                    "SELECT m.FilePath FROM DocumentMultimedia dm "
                    "JOIN Multimedia m ON dm.MultimediaId = m.MultimediaId "
                    "WHERE dm.DocumentId = ? AND LOWER(m.MimeType) LIKE 'image/%' "
                    "ORDER BY dm.DocumentMultimediaId"));
                imageQuery.addBindValue(best.docId);
                if (imageQuery.exec()) {
                    while (imageQuery.next()) {
                        QString path = imageQuery.value(0).toString();
                        if (path.isEmpty())
                            continue;
                        if (QDir::isRelativePath(path))
                            path = QDir(best.folder).filePath(path);
                        const QFileInfo imageInfo(path);
                        if (!imageInfo.exists() || !imageInfo.isFile()) {
                            qWarning() << "WorkbookManager: Watchtower illustration is unavailable:"
                                       << imageInfo.absoluteFilePath();
                            continue;
                        }
                        images.append({
                            QStringLiteral("wtimg_%1_%2")
                                .arg(best.docId)
                                .arg(imageInfo.fileName()),
                            imageInfo.absoluteFilePath()
                        });
                    }
                } else {
                    qWarning() << "WorkbookManager: couldn't read Watchtower image references:"
                               << imageQuery.lastError().text();
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }
    if (songNumbers.size() != 2) {
        qWarning() << "WorkbookManager: expected 2 songs (middle/closing) in Watchtower document, found" << songNumbers.size();
    }

    // The Watchtower Study article carries exactly two of the meeting's
    // three songs -- the one sung right before the Study itself (the
    // meeting's *middle* song, w3, since the Public Talk comes first) and
    // the closing song (w6). The opening song (w1, before the Public Talk)
    // isn't part of this article's own content and stays a manual pick.
    if (songNumbers.size() > 0) m_controller->resolveWeeklySong(songNumbers.first(), lang, QStringLiteral("w3"));
    if (songNumbers.size() > 1) m_controller->resolveWeeklySong(songNumbers.last(), lang, QStringLiteral("w6"));

    if (!best.title.isEmpty())
        m_controller->meetingSchedule()->updateSegmentTitle(QStringLiteral("w4"), best.title.toUpper());

    commitWatchtowerImages(images);
    qInfo() << "WorkbookManager: linked" << images.size()
            << "downloaded Watchtower article images for" << best.title;

    setStatus(tr("Watchtower Study loaded: %1").arg(best.title.isEmpty() ? tr("(untitled)") : best.title));
    return true;
}

void WorkbookManager::fetchPublication(const QString &pubCode, bool supplementalOnly)
{
    const QDate targetDate = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    const QString issue = (pubCode == QStringLiteral("mwb"))
        ? mwbIssueFor(targetDate) : wIssueFor(QDate::currentDate());
    const QString lang = currentLanguageCode();

    QUrl url(QStringLiteral("https://app.jw-cdn.org/apis/pub-media/GETPUBMEDIALINKS"));
    QUrlQuery q;
    q.addQueryItem("output", "json");
    q.addQueryItem("fileformat", "EPUB");
    q.addQueryItem("alllangs", "0");
    q.addQueryItem("langwritten", lang);
    q.addQueryItem("pub", pubCode);
    q.addQueryItem("issue", issue);
    url.setQuery(q);

    const QString pubLabel = (pubCode == QStringLiteral("mwb"))
        ? tr("Meeting Workbook") : tr("Watchtower Study");
    const QString langName = SongSearchUtils::languageNameForCode(lang);

    QNetworkReply *metaReply = m_network->get(QNetworkRequest(url));
    connect(metaReply, &QNetworkReply::finished, this, [this, metaReply, pubCode, pubLabel, langName, lang, supplementalOnly]() {
        metaReply->deleteLater();
        const int httpStatus = metaReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (metaReply->error() != QNetworkReply::NoError) {
            // A 404 here specifically means jw.org has no edition of this
            // publication in this language for this issue -- a real
            // availability gap (confirmed live: this happens for some
            // languages/publications), not a connectivity problem, so it
            // gets a plain-language message instead of the raw network
            // error text.
            if (httpStatus == 404) {
                const QString error = tr("%1 isn't available in %2 for this issue").arg(pubLabel, langName);
                if (supplementalOnly)
                    qWarning() << "WorkbookManager:" << error;
                else
                    setStatus(error);
            } else {
                const QString error = tr("Couldn't reach jw.org for %1 (%2)").arg(pubLabel, metaReply->errorString());
                if (supplementalOnly)
                    qWarning() << "WorkbookManager:" << error;
                else
                    setStatus(error);
            }
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(metaReply->readAll()).object();
        const QJsonArray epubEntries = root["files"].toObject()[lang].toObject()["EPUB"].toArray();
        const QString epubUrl = epubEntries.isEmpty()
            ? QString()
            : epubEntries.at(0).toObject()["file"].toObject()["url"].toString();
        if (epubUrl.isEmpty()) {
            const QString error = tr("%1 isn't available in %2 for this issue").arg(pubLabel, langName);
            if (supplementalOnly)
                qWarning() << "WorkbookManager:" << error;
            else
                setStatus(error);
            return;
        }

        QNetworkReply *epubReply = m_network->get(QNetworkRequest(QUrl(epubUrl)));
        connect(epubReply, &QNetworkReply::finished, this, [this, epubReply, pubCode, pubLabel, supplementalOnly]() {
            epubReply->deleteLater();
            if (epubReply->error() != QNetworkReply::NoError) {
                const QString error = tr("Download failed for %1 (%2)").arg(pubLabel, epubReply->errorString());
                if (supplementalOnly)
                    qWarning() << "WorkbookManager:" << error;
                else
                    setStatus(error);
                return;
            }
            const QByteArray data = epubReply->readAll();
            if (pubCode == QStringLiteral("mwb"))
                applyMwbEpub(data, supplementalOnly);
            else
                applyWatchtowerEpub(data);
        });
    });
}

void WorkbookManager::applyMwbEpub(const QByteArray &epubData, bool supplementalOnly)
{
    const QMap<QString, QByteArray> files = extractFilesFromEpub(epubData);
    QByteArray toc;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        if (it.key().endsWith(QStringLiteral("toc.xhtml"))) { toc = it.value(); break; }
    }
    if (toc.isEmpty()) {
        if (supplementalOnly)
            qWarning() << "WorkbookManager: couldn't find the Meeting Workbook EPUB table of contents";
        else
            setStatus(tr("Meeting Workbook: couldn't find its table of contents"));
        return;
    }

    const QList<TocEntry> weeks = parseToc(toc);
    if (weeks.isEmpty()) {
        if (supplementalOnly)
            qWarning() << "WorkbookManager: no weeks found in Meeting Workbook EPUB";
        else
            setStatus(tr("Meeting Workbook: no weeks found"));
        return;
    }

    const QDate targetDate = m_selectedDate.isValid() ? m_selectedDate : QDate::currentDate();
    const int issueYear = targetDate.year();

    int bestIndex = -1;
    int bestDiff = INT_MAX;
    for (int i = 0; i < weeks.size(); ++i) {
        const QDate start = parseWeekStart(weeks[i].text, issueYear);
        if (!start.isValid()) continue;
        // Must be the week that actually CONTAINS the selected date (its
        // Mon-start is on or before it, within the same 7-day span) -- nearest absolute
        // distance would otherwise flip to next week as early as Thursday
        // (once more than 3 days past this week's Monday), rolling the
        // whole app over several days before Monday actually arrives.
        const qint64 daysFromStart = start.daysTo(targetDate);
        if (daysFromStart < 0 || daysFromStart >= 7) continue;
        const int diff = static_cast<int>(daysFromStart);
        if (diff < bestDiff) { bestDiff = diff; bestIndex = i; }
    }
    // Text parsing failed for every entry (unrecognized month names, e.g. a
    // language monthNumberFromWord doesn't cover) -- fall back to position:
    // assume roughly one week per chapter starting near the issue's first
    // month, which keeps this working (approximately) for every language
    // rather than not at all.
    if (bestIndex == -1) {
        const QDate issueStart(issueYear, mwbIssueFor(targetDate).mid(4, 2).toInt(), 1);
        int approxWeek = static_cast<int>(issueStart.daysTo(targetDate) / 7);
        bestIndex = std::clamp(approxWeek, 0, static_cast<int>(weeks.size()) - 1);
    }

    const QString chosenHref = weeks[bestIndex].href;
    QByteArray chapter;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        if (it.key().endsWith(chosenHref)) { chapter = it.value(); break; }
    }
    if (chapter.isEmpty()) {
        if (supplementalOnly)
            qWarning() << "WorkbookManager: couldn't load the selected week's Meeting Workbook content";
        else
            setStatus(tr("Meeting Workbook: couldn't load this week's content"));
        return;
    }

    // Song numbers, in document order -- always exactly [opening, middle,
    // concluding] for the standard midweek meeting format (confirmed
    // against a real live issue), so position alone (not fragile markup
    // matching) is enough to tell them apart.
    QList<int> songs;
    static const QRegularExpression songRe(R"(Song\s+(\d+))", QRegularExpression::CaseInsensitiveOption);
    const QString html = QString::fromUtf8(chapter);
    auto it = songRe.globalMatch(html);
    while (it.hasNext() && songs.size() < 3)
        songs.append(it.next().captured(1).toInt());
    if (songs.size() != 3) {
        qWarning() << "WorkbookManager: expected 3 songs (opening/middle/concluding) in mwb chapter, found" << songs.size();
    }

    const QString lang = currentLanguageCode();
    if (songs.size() > 0) m_controller->resolveWeeklySong(songs[0], lang, QStringLiteral("m1"));
    if (songs.size() > 1) m_controller->resolveWeeklySong(songs[1], lang, QStringLiteral("m7"));
    if (songs.size() > 2) m_controller->resolveWeeklySong(songs[2], lang, QStringLiteral("m11"));

    QDate weekStart = parseWeekStart(weeks[bestIndex].text, targetDate.year());
    if (!weekStart.isValid())
        weekStart = targetDate.addDays(1 - targetDate.dayOfWeek());
    const WorkbookAssignmentData assignmentData =
        parseWorkbookAssignmentData(html, lang, weekStart);
    m_controller->setWorkbookScheduleRows(assignmentData.scheduleRows);
    const BookStudyAssignment studyAssignment = parseBookStudyAssignment(html, assignmentData);
    if (!studyAssignment.chapters.isEmpty()) {
        m_controller->setWorkbookMediaForSegment(
            studyAssignment.segmentId,
            bookStudyImages(lang, studyAssignment.chapters, studyAssignment.segmentId));
    } else if (!studyAssignment.segmentId.isEmpty()) {
        qWarning() << "WorkbookManager: couldn't identify a WCG chapter in Bible Study assignment"
                   << studyAssignment.segmentId << "for" << weeks[bestIndex].text;
    }
    qInfo() << "WorkbookManager: generated" << assignmentData.scheduleRows.size()
            << "independent timed meeting rows from" << weeks[bestIndex].text;
    m_applyPartsLoaded = true;

    setStatus(tr("Meeting Workbook loaded: %1").arg(weeks[bestIndex].text));
}

void WorkbookManager::applyWatchtowerEpub(const QByteArray &epubData)
{
    const QMap<QString, QByteArray> files = extractFilesFromEpub(epubData);

    // Unlike mwb's toc (where link text is a date range, reliably
    // containing a digit), the Watchtower toc's link text is just the
    // article title -- most articles have no digit in their title at all
    // (confirmed live: a real September issue's 6 articles, none titled
    // with a number), so a digit-based filter wrongly excludes everything.
    // Instead, treat every purely-numeric-stem chapter file as a candidate
    // directly and let each chapter's own "contextTtl" date line (which
    // Study Edition/Table of Contents/Page Navigation chapters don't have)
    // naturally separate real articles from front matter.
    static const QRegularExpression stemRe(QStringLiteral(R"RX(([^/]+)\.xhtml$)RX"));
    static const QRegularExpression dateLineRe(
        QStringLiteral(R"(contextTtl[^>]*>.*?<strong>([^<]+)</strong>)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression titleRe(QStringLiteral(R"RX(<title>([^<]+)</title>)RX"),
                                             QRegularExpression::CaseInsensitiveOption);

    const QDate today = QDate::currentDate();
    int bestDiff = INT_MAX;
    QByteArray bestChapter;
    QString bestTitle;
    QString bestStem;

    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        const QRegularExpressionMatch stemMatch = stemRe.match(it.key());
        if (!stemMatch.hasMatch()) continue;
        const QString stem = stemMatch.captured(1);
        bool numeric = false;
        stem.toLongLong(&numeric);
        if (!numeric || stem.endsWith(QStringLiteral("-extracted"))) continue;

        const QString html = QString::fromUtf8(it.value());
        const QRegularExpressionMatch dateMatch = dateLineRe.match(html);
        if (!dateMatch.hasMatch()) continue; // front matter (no date line) -- not a real article
        const QDate start = parseWeekStart(dateMatch.captured(1), today.year());
        if (!start.isValid()) continue;

        // Same week-containment fix as applyMwbEpub above: only a week whose
        // Monday is on or before today, within 7 days, may win -- otherwise
        // this rolls over as early as Thursday instead of on Monday.
        const qint64 daysFromStart = start.daysTo(today);
        if (daysFromStart < 0 || daysFromStart >= 7) continue;
        const int diff = static_cast<int>(daysFromStart);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestChapter = it.value();
            bestStem = stem;
            const QRegularExpressionMatch titleMatch = titleRe.match(html);
            bestTitle = titleMatch.hasMatch() ? titleMatch.captured(1).trimmed() : QString();
        }
    }

    if (bestChapter.isEmpty()) { setStatus(tr("Watchtower Study: no dated article found for this issue")); return; }

    QList<int> songs;
    static const QRegularExpression songRe(R"(SONG\s+(\d+))", QRegularExpression::CaseInsensitiveOption);
    const QString html = QString::fromUtf8(bestChapter);
    auto sit = songRe.globalMatch(html);
    // Capped at 2 -- exactly [middle, closing] -- so an extra "SONG n"
    // occurrence elsewhere in the article's own body text (a footnote, a
    // box, a cross-reference) can't silently shift which song lands on w6.
    // This was previously uncapped, unlike applyMwbEpub's equivalent loop
    // below -- confirmed as the cause of the closing-song mismatch reports.
    while (sit.hasNext() && songs.size() < 2)
        songs.append(sit.next().captured(1).toInt());
    if (songs.size() != 2) {
        qWarning() << "WorkbookManager: expected 2 songs (middle/closing) in Watchtower article, found" << songs.size();
    }

    const QString lang = currentLanguageCode();
    // See applyFromLocalWatchtower: the article's two songs are the middle
    // (w3) and closing (w6) songs, not the opening song (w1), which stays a
    // manual pick.
    if (songs.size() > 0) m_controller->resolveWeeklySong(songs.first(), lang, QStringLiteral("w3"));
    if (songs.size() > 1) m_controller->resolveWeeklySong(songs.last(), lang, QStringLiteral("w6"));

    if (!bestTitle.isEmpty())
        m_controller->meetingSchedule()->updateSegmentTitle(QStringLiteral("w4"), bestTitle.toUpper());

    applyWatchtowerImages(files, html, bestStem);

    setStatus(tr("Watchtower Study loaded: %1").arg(bestTitle.isEmpty() ? tr("(untitled)") : bestTitle));
}

// Pulls the article's own in-content photos/illustrations (the <img>s inside
// its XHTML, e.g. "images/2026485_univ_cnt_1.jpg") out of the already-
// extracted epub file map, saves them to a small on-disk cache, and links
// them into the Watchtower Study segment as ordinary "image" media assets --
// the same asset type/UI path any manually staged image already uses.
// Images left over from a previous, now-superseded article are removed first
// so they don't accumulate week after week.
void WorkbookManager::applyWatchtowerImages(const QMap<QString, QByteArray> &files,
                                             const QString &articleHtml, const QString &articleStem)
{
    if (articleStem.isEmpty()) { commitWatchtowerImages({}); return; }

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/workbook_images");
    QDir().mkpath(cacheDir);

    static const QRegularExpression imgRe(QStringLiteral(R"RX(<img[^>]*\bsrc="([^"]+)")RX"),
                                           QRegularExpression::CaseInsensitiveOption);

    QList<QPair<QString, QString>> images;
    int index = 0;
    auto it = imgRe.globalMatch(articleHtml);
    while (it.hasNext()) {
        const QString src = it.next().captured(1);
        QByteArray data;
        for (auto fit = files.constBegin(); fit != files.constEnd(); ++fit) {
            if (fit.key().endsWith(src)) { data = fit.value(); break; }
        }
        if (data.isEmpty()) { ++index; continue; }

        const QString baseName = src.section(QLatin1Char('/'), -1);
        const QString id = QStringLiteral("wtimg_%1_%2").arg(articleStem).arg(index);
        const QString outPath = cacheDir + QLatin1Char('/') + articleStem + QLatin1Char('_') + baseName;

        if (!QFile::exists(outPath)) {
            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
                out.close();
            } else {
                ++index;
                continue;
            }
        }

        images.append({id, outPath});
        ++index;
    }

    commitWatchtowerImages(images);
}

// Shared by both the local-jwpub path (files already sit on disk) and the
// network EPUB path (files just got written to the cache dir above): stages
// each image as an ordinary "image" media asset -- the same asset type/UI
// path any manually staged image already uses -- and links the full set into
// the Watchtower Study segment. Images left over from a previous, now-
// superseded article are removed first so they don't accumulate week after
// week.
void WorkbookManager::commitWatchtowerImages(const QList<QPair<QString, QString>> &idPathPairs)
{
    for (const QString &staleId : std::as_const(m_lastWatchtowerImageIds))
        m_controller->mediaLibrary()->removeMedia(staleId);
    m_lastWatchtowerImageIds.clear();

    QStringList newIds;
    QStringList newOwnedIds;
    for (const auto &pair : idPathPairs) {
        const QString &path = pair.second;
        const bool alreadyIndexed = m_controller->mediaLibrary()->containsPath(path);
        const QString mediaId = m_controller->ensureWorkbookMedia(path);
        if (mediaId.isEmpty())
            continue;
        m_controller->mediaLibrary()->markImported(mediaId, tr("Watchtower Study"));
        newIds.append(mediaId);
        if (!alreadyIndexed)
            newOwnedIds.append(mediaId);
    }

    m_controller->meetingSchedule()->setLinkedMediaForId(QStringLiteral("w4"), newIds);
    m_lastWatchtowerImageIds = newOwnedIds;
    m_controller->persistWorkbookChanges();
}
