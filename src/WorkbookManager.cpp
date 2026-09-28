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
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QSet>
#include <cstring>
#include <climits>
#include <algorithm>

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
    static const QRegularExpression re(R"(([A-Za-zÀ-ÿ]+)\s+(\d{1,2}))");
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
    // A publication JW Library has already downloaded locally is strictly
    // better than fetching our own EPUB copy: it's guaranteed to be in the
    // exact right language (no 404s for languages jw.org's EPUB export
    // doesn't cover), its images are already sitting on disk as plain jpgs
    // (no zip/regex extraction needed), and matching by each document's own
    // date text sidesteps any issue-numbering/offset assumptions entirely.
    // Only fall back to the network fetch if nothing local matches -- that
    // fallback only ever targets today, since a manually-picked past/future
    // week is presumed to already be downloaded locally if it's available
    // at all.
    if (!applyFromLocalMwb(lang, targetDate) && !m_selectedDate.isValid())
        fetchPublication(QStringLiteral("mwb"));
    if (!applyFromLocalWatchtower(lang, targetDate) && !m_selectedDate.isValid())
        fetchPublication(QStringLiteral("w"));
    // No network fallback here -- the Congregation Bible Study book isn't
    // published via the mwb/w EPUB endpoints at all, so this only ever
    // works when it's already downloaded locally in JW Library.
    applyFromLocalBookStudy(lang);
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
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connName);
        }
    }

    QList<QDate> sorted = dates.values();
    std::sort(sorted.begin(), sorted.end());

    QVariantList result;
    for (const QDate &start : sorted) {
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
    QString bestFolder;
    const QString filter = QStringLiteral("*_") + lang;
    for (const QString &root : publicationsRoots()) {
        QDir rootDir(root);
        if (!rootDir.exists()) continue;
        const QFileInfoList entries = rootDir.entryInfoList({filter}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) {
            QFile manifest(fi.absoluteFilePath() + QStringLiteral("/manifest.json"));
            if (!manifest.open(QIODevice::ReadOnly)) continue;
            const QJsonObject pub = QJsonDocument::fromJson(manifest.readAll())
                .object().value(QStringLiteral("publication")).toObject();
            if (pub.value(QStringLiteral("publicationType")).toString() == QStringLiteral("Book")) {
                bestFolder = fi.absoluteFilePath();
                break;
            }
        }
        if (!bestFolder.isEmpty()) break;
    }

    if (bestFolder.isEmpty()) return false;
    m_controller->setWorkbookPublicationFolder(QStringLiteral("cbs"), bestFolder);
    return true;
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
                QSqlQuery q(db);
                // mwb has no separate date field -- the week's own Title
                // *is* the date range (e.g. "September 28-October 4").
                if (q.exec(QStringLiteral("SELECT DocumentId, Title FROM Document ORDER BY DocumentId"))) {
                    while (q.next()) {
                        const int docId = q.value(0).toInt();
                        const QString title = q.value(1).toString();
                        const QDate start = parseWeekStart(title, targetDate.year());
                        if (!start.isValid()) continue;
                        const int diff = static_cast<int>(qAbs(start.daysTo(targetDate)));
                        if (diff < best.diff) best = {dbPath, docId, title, diff};
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
            QSqlQuery q(db);
            // "sjjm" is the song-video/audio publication symbol; its Track
            // number is the song number, in document (opening/middle/
            // concluding) order -- confirmed against a real local database.
            q.prepare(QStringLiteral(
                "SELECT m.Track FROM DocumentMultimedia dm "
                "JOIN Multimedia m ON dm.MultimediaId = m.MultimediaId "
                "WHERE dm.DocumentId = ? AND m.KeySymbol = 'sjjm' "
                "ORDER BY dm.MultimediaId"));
            q.addBindValue(best.docId);
            if (q.exec()) { while (q.next()) songNumbers.append(q.value(0).toInt()); }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    if (songNumbers.size() > 0) m_controller->resolveWeeklySong(songNumbers[0], lang, QStringLiteral("m1"));
    if (songNumbers.size() > 1) m_controller->resolveWeeklySong(songNumbers[1], lang, QStringLiteral("m7"));
    if (songNumbers.size() > 2) m_controller->resolveWeeklySong(songNumbers[2], lang, QStringLiteral("m11"));

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
                        const int diff = static_cast<int>(qAbs(start.daysTo(targetDate)));
                        if (diff < best.diff) best = {dir.absolutePath(), dbPath, docId, title, diff};
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
            QSqlQuery sq(db);
            sq.prepare(QStringLiteral(
                "SELECT m.Track FROM DocumentMultimedia dm "
                "JOIN Multimedia m ON dm.MultimediaId = m.MultimediaId "
                "WHERE dm.DocumentId = ? AND m.KeySymbol = 'sjjm' "
                "ORDER BY dm.MultimediaId"));
            sq.addBindValue(best.docId);
            if (sq.exec()) { while (sq.next()) songNumbers.append(sq.value(0).toInt()); }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // Auto-link just the magazine cover as a placeholder thumbnail, not the
    // article's own in-content photos -- the operator adds whichever of
    // those they actually want for this week's Study themselves via "ADD JW
    // IMAGE", which already opens straight into this same folder.
    {
        const QDir folderDir(best.folder);
        const QStringList covers = folderDir.entryList({QStringLiteral("*_cvr.jpg")}, QDir::Files);
        if (!covers.isEmpty()) {
            images.append({QStringLiteral("wtimg_cover_%1").arg(covers.first()),
                            folderDir.absoluteFilePath(covers.first())});
        }
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

    setStatus(tr("Watchtower Study loaded: %1").arg(best.title.isEmpty() ? tr("(untitled)") : best.title));
    return true;
}

void WorkbookManager::fetchPublication(const QString &pubCode)
{
    const QDate today = QDate::currentDate();
    const QString issue = (pubCode == QStringLiteral("mwb")) ? mwbIssueFor(today) : wIssueFor(today);
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
    connect(metaReply, &QNetworkReply::finished, this, [this, metaReply, pubCode, pubLabel, langName, lang]() {
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
                setStatus(tr("%1 isn't available in %2 for this issue").arg(pubLabel, langName));
            } else {
                setStatus(tr("Couldn't reach jw.org for %1 (%2)").arg(pubLabel, metaReply->errorString()));
            }
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(metaReply->readAll()).object();
        const QJsonArray epubEntries = root["files"].toObject()[lang].toObject()["EPUB"].toArray();
        const QString epubUrl = epubEntries.isEmpty()
            ? QString()
            : epubEntries.at(0).toObject()["file"].toObject()["url"].toString();
        if (epubUrl.isEmpty()) {
            setStatus(tr("%1 isn't available in %2 for this issue").arg(pubLabel, langName));
            return;
        }

        QNetworkReply *epubReply = m_network->get(QNetworkRequest(QUrl(epubUrl)));
        connect(epubReply, &QNetworkReply::finished, this, [this, epubReply, pubCode, pubLabel]() {
            epubReply->deleteLater();
            if (epubReply->error() != QNetworkReply::NoError) {
                setStatus(tr("Download failed for %1 (%2)").arg(pubLabel, epubReply->errorString()));
                return;
            }
            const QByteArray data = epubReply->readAll();
            if (pubCode == QStringLiteral("mwb"))
                applyMwbEpub(data);
            else
                applyWatchtowerEpub(data);
        });
    });
}

void WorkbookManager::applyMwbEpub(const QByteArray &epubData)
{
    const QMap<QString, QByteArray> files = extractFilesFromEpub(epubData);
    QByteArray toc;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        if (it.key().endsWith(QStringLiteral("toc.xhtml"))) { toc = it.value(); break; }
    }
    if (toc.isEmpty()) { setStatus(tr("Meeting Workbook: couldn't find its table of contents")); return; }

    const QList<TocEntry> weeks = parseToc(toc);
    if (weeks.isEmpty()) { setStatus(tr("Meeting Workbook: no weeks found")); return; }

    const QDate today = QDate::currentDate();
    const int issueYear = today.year();

    int bestIndex = -1;
    int bestDiff = INT_MAX;
    for (int i = 0; i < weeks.size(); ++i) {
        const QDate start = parseWeekStart(weeks[i].text, issueYear);
        if (!start.isValid()) continue;
        const int diff = static_cast<int>(qAbs(start.daysTo(today)));
        if (diff < bestDiff) { bestDiff = diff; bestIndex = i; }
    }
    // Text parsing failed for every entry (unrecognized month names, e.g. a
    // language monthNumberFromWord doesn't cover) -- fall back to position:
    // assume roughly one week per chapter starting near the issue's first
    // month, which keeps this working (approximately) for every language
    // rather than not at all.
    if (bestIndex == -1) {
        const QDate issueStart(issueYear, mwbIssueFor(today).mid(4, 2).toInt(), 1);
        int approxWeek = static_cast<int>(issueStart.daysTo(today) / 7);
        bestIndex = std::clamp(approxWeek, 0, static_cast<int>(weeks.size()) - 1);
    }

    const QString chosenHref = weeks[bestIndex].href;
    QByteArray chapter;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        if (it.key().endsWith(chosenHref)) { chapter = it.value(); break; }
    }
    if (chapter.isEmpty()) { setStatus(tr("Meeting Workbook: couldn't load this week's content")); return; }

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

    const QString lang = currentLanguageCode();
    if (songs.size() > 0) m_controller->resolveWeeklySong(songs[0], lang, QStringLiteral("m1"));
    if (songs.size() > 1) m_controller->resolveWeeklySong(songs[1], lang, QStringLiteral("m7"));
    if (songs.size() > 2) m_controller->resolveWeeklySong(songs[2], lang, QStringLiteral("m11"));

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

        const int diff = static_cast<int>(qAbs(start.daysTo(today)));
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
    while (sit.hasNext())
        songs.append(sit.next().captured(1).toInt());

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
    int index = 0;
    for (const auto &pair : idPathPairs) {
        const QString &id = pair.first;
        const QString &path = pair.second;
        if (m_controller->mediaLibrary()->idOfPath(path).isEmpty()) {
            QVariantMap m;
            m["id"] = id;
            m["name"] = tr("Watchtower Study Image %1").arg(index + 1);
            m["type"] = "image";
            m["absolutePath"] = path;
            m["thumbnailPath"] = path;
            m["isStaged"] = false;
            m["isImported"] = true;
            m["category"] = tr("Watchtower Study");
            m_controller->mediaLibrary()->appendFromVariantList({m});
        }
        newIds.append(id);
        ++index;
    }

    m_controller->meetingSchedule()->setLinkedMediaForId(QStringLiteral("w4"), newIds);
    m_lastWatchtowerImageIds = newIds;
    m_controller->persistWorkbookChanges();
}
