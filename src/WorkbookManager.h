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

// Fetches the current workbook, links songs, and matches local media for each segment.
class WorkbookManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit WorkbookManager(BroadcastController *controller, QObject *parent = nullptr);

    QString status() const { return m_status; }

    // Start the refresh loop after the media library has had time to index files.
    void startAutoRefresh();

    Q_INVOKABLE void refreshNow();

    // Returns the week list for the current language, including local workbook weeks.
    QVariantList availableWeeks() const;
    // Force a specific workbook week, or return to automatic date matching.
    void selectWeek(const QDate &date);

    // Look up the public talk outline and attach its media to the weekly schedule.
    Q_INVOKABLE void lookupPublicTalk(int talkNumber);

signals:
    void statusChanged();
    void publicTalkResolved(int talkNumber, const QString &title, bool hasMedia);

private:
    void setStatus(const QString &text);
    void fetchPublication(const QString &pubCode, bool supplementalOnly = false);
    void applyMwbEpub(const QByteArray &epubData, bool supplementalOnly = false);
    void applyWatchtowerEpub(const QByteArray &epubData);
    void applyWatchtowerImages(const QMap<QString, QByteArray> &files,
                                const QString &articleHtml, const QString &articleStem);
    void commitWatchtowerImages(const QList<QPair<QString, QString>> &idPathPairs);
    // Use local JW Library content when available; otherwise fall back to fetching.
    bool applyFromLocalMwb(const QString &lang, const QDate &targetDate);
    bool applyFromLocalWatchtower(const QString &lang, const QDate &targetDate);
    QStringList bookStudyImages(const QString &lang, const QList<int> &chapters,
                                const QString &segmentId);
    // Match the local Bible study book by publication type, not by week number.
    bool applyFromLocalBookStudy(const QString &lang);
    QString currentLanguageCode() const;
    static QString mwbIssueFor(const QDate &date);
    static QString wIssueFor(const QDate &date);

    BroadcastController *m_controller;
    QNetworkAccessManager *m_network;
    QTimer *m_timer;
    QString m_status;
    // Invalid means auto-pick the current week; a chosen date overrides that.
    QDate m_selectedDate;
    QString m_applyPartsLanguage;
    QDate m_applyPartsDate;
    bool m_applyPartsLoaded = false;
    // Track the last Watchtower article images so stale ones can be cleaned up.
    QStringList m_lastWatchtowerImageIds;
    QString m_bookStudyPublicationFolder;
};
