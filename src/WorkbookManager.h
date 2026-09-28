#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QPair>
#include <QVariantList>
#include <QByteArray>
#include <QDate>
#include <QTimer>

class QNetworkAccessManager;
class BroadcastController;

/**
 * @brief Automatically fetches the current week's Life and Ministry Meeting
 * Workbook (mwb) and Watchtower Study (w) content from JW.org's public,
 * unauthenticated pub-media API, and applies the parsed song numbers /
 * article titles into MeetingScheduleModel -- then reuses
 * BroadcastController::resolveWeeklySong (the same language-aware matching
 * the manual song-search popup uses) to auto-link the actual local video for
 * each song number, so the right media just shows up in the right segment
 * without the operator looking anything up by hand.
 *
 * Both publications are downloaded as plain EPUB (not the encrypted .jwpub
 * format) -- a standard, unencrypted zip of XHTML files, requiring no
 * cryptography, just zip extraction (see third_party/miniz) and text
 * parsing.
 */
class WorkbookManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit WorkbookManager(BroadcastController *controller, QObject *parent = nullptr);

    QString status() const { return m_status; }

    // Kicks off an immediate check plus a recurring periodic one. Call once,
    // shortly after startup (after the media library has had a chance to
    // index local files, since auto-linking needs them already indexed).
    void startAutoRefresh();

    Q_INVOKABLE void refreshNow();

    // All week-start dates found across every locally downloaded mwb/w
    // publication for the current language, each as {"label", "iso"} --
    // powers the week-picker dropdown. Sorted ascending.
    QVariantList availableWeeks() const;
    // Pins the workbook to a specific week (from availableWeeks) instead of
    // "closest to today", and re-runs the lookup immediately. An invalid
    // date goes back to automatic (today-based) matching -- including on
    // every future periodic refresh, until selectWeek is called again.
    void selectWeek(const QDate &date);

signals:
    void statusChanged();

private:
    void setStatus(const QString &text);
    void fetchPublication(const QString &pubCode);
    void applyMwbEpub(const QByteArray &epubData);
    void applyWatchtowerEpub(const QByteArray &epubData);
    void applyWatchtowerImages(const QMap<QString, QByteArray> &files,
                                const QString &articleHtml, const QString &articleStem);
    void commitWatchtowerImages(const QList<QPair<QString, QString>> &idPathPairs);
    // Prefer publications JW Library has already downloaded locally (exact
    // language, no month-offset guessing, images already extracted to disk
    // as plain jpgs) over the network EPUB fetch. Returns false if nothing
    // matching is found locally, so the caller can fall back to fetching.
    // targetDate is the week to match against -- QDate::currentDate() during
    // normal auto-refresh, or the operator's manual pick from selectWeek().
    bool applyFromLocalMwb(const QString &lang, const QDate &targetDate);
    bool applyFromLocalWatchtower(const QString &lang, const QDate &targetDate);
    // The Congregation Bible Study book has no weekly issue to match --
    // it's whichever locally downloaded publication is a standalone "Book"
    // (per JW Library's own manifest.json), as opposed to the Bible,
    // songbook, daily text, or a dated mwb/w periodical.
    bool applyFromLocalBookStudy(const QString &lang);
    QString currentLanguageCode() const;
    static QString mwbIssueFor(const QDate &date);
    static QString wIssueFor(const QDate &date);

    BroadcastController *m_controller;
    QNetworkAccessManager *m_network;
    QTimer *m_timer;
    QString m_status;
    // Invalid = automatic (always matches today); set by selectWeek() when
    // the operator manually picks a week from the dropdown.
    QDate m_selectedDate;
    // Ids of the article images linked into the Watchtower Study segment by
    // the previous run -- removed from the library if a later run picks a
    // different article, so stale images don't pile up week after week.
    QStringList m_lastWatchtowerImageIds;
};
