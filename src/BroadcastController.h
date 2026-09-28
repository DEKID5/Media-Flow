#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QAudioOutput>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QMediaPlayer>

#include "MediaLibraryModel.h"
#include "MeetingScheduleModel.h"
#include "CameraDeviceModel.h"
#include "BroadcastEngine.h"
#include "StagedMediaProxyModel.h"
#include "MediaAsset.h"
#include "VirtualCameraManager.h"
#include "PinnedFolderModel.h"

class MediaExtractor;
class MediaThumbnailManager;
enum class MediaType;
class QQuickWindow;
class WorkbookManager;

/**
 * @brief The BroadcastController class acts as the central bridge between C++ and QML.
 * Owns all models, the broadcast engine, BGM player, and timer system.
 */
class BroadcastController final : public QObject
{
    Q_OBJECT

    // --- Models ---
    Q_PROPERTY(MediaLibraryModel *mediaLibrary READ mediaLibrary CONSTANT)
    Q_PROPERTY(MeetingScheduleModel *meetingSchedule READ meetingSchedule CONSTANT)
    Q_PROPERTY(CameraDeviceModel *cameraDevices READ cameraDevices CONSTANT)
    Q_PROPERTY(BroadcastEngine *broadcastEngine READ broadcastEngine CONSTANT)
    Q_PROPERTY(QCameraDevice programCameraDevice READ programCameraDevice WRITE setProgramCameraDevice NOTIFY programCameraDeviceChanged)
    // Captured once at startup rather than left to track "system default"
    // live -- Zoom (and other communications apps) can silently change
    // Windows' default audio device role when joining a call, which would
    // otherwise drag the room/audience audio along with it. Room audio is
    // deliberately independent of anything Zoom-related.
    Q_PROPERTY(QAudioDevice roomAudioOutputDevice READ roomAudioOutputDevice CONSTANT)

    // --- State Properties ---
    Q_PROPERTY(QString selectedSegmentId READ selectedSegmentId WRITE selectSegment NOTIFY selectedSegmentIdChanged)
    Q_PROPERTY(QString meetingType READ meetingType WRITE setMeetingTypeStr NOTIFY meetingTypeChanged)
    Q_PROPERTY(QVariantMap currentLanguage READ currentLanguage WRITE setCurrentLanguage NOTIFY currentLanguageChanged)
    Q_PROPERTY(QString currentLanguageCode READ currentLanguageCode WRITE setCurrentLanguageCode NOTIFY currentLanguageCodeChanged)
    Q_PROPERTY(int masterVolume READ masterVolume WRITE setMasterVolume NOTIFY masterVolumeChanged)
    Q_PROPERTY(bool mixerMuted READ mixerMuted WRITE setMixerMuted NOTIFY mixerMutedChanged)
    Q_PROPERTY(bool isMeetingLive READ isMeetingLive WRITE setMeetingLive NOTIFY isMeetingLiveChanged)
    Q_PROPERTY(bool vcamEnabled READ vcamEnabled NOTIFY vcamEnabledChanged)
    Q_PROPERTY(bool feedExtended READ feedExtended NOTIFY feedExtendedChanged)
    Q_PROPERTY(bool hasSecondaryScreen READ hasSecondaryScreen NOTIFY hasSecondaryScreenChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    // True once the first (startup) library scan has finished -- the splash
    // screen (qml/SplashScreen.qml) waits on this before showing the main
    // window, so the operator doesn't see an empty/flickering dashboard
    // while assets are still being discovered.
    Q_PROPERTY(bool initialScanComplete READ initialScanComplete NOTIFY initialScanCompleteChanged)

    // --- Settings ---
    // -1 = automatic (today's screens.at(1)-or-primary behavior).
    Q_PROPERTY(int extendedFeedScreenIndex READ extendedFeedScreenIndex WRITE setExtendedFeedScreenIndex NOTIFY extendedFeedScreenIndexChanged)
    // Independent from extendedFeedScreenIndex -- the full-screen timer is
    // its own dedicated window (qml/TimerWindow.qml, opened/closed via
    // setTimerFullScreenActive) so it can target a different monitor than
    // wherever Extended Feed/audience content is shown. -1 = automatic
    // (same second-screen-or-primary default as Extended Feed).
    Q_PROPERTY(int timerScreenIndex READ timerScreenIndex WRITE setTimerScreenIndex NOTIFY timerScreenIndexChanged)
    Q_PROPERTY(bool bgmUseCustomFolder READ bgmUseCustomFolder WRITE setBgmUseCustomFolder NOTIFY bgmSettingsChanged)
    Q_PROPERTY(QString bgmCustomFolder READ bgmCustomFolder NOTIFY bgmSettingsChanged)
    // Shown on the Extended Feed screen whenever nothing is live (standby),
    // instead of a plain black screen -- "" means no background configured.
    // extendedFeedBackgroundType is "image" or "video", derived from the
    // chosen file's extension at browse time.
    Q_PROPERTY(QString extendedFeedBackgroundPath READ extendedFeedBackgroundPath NOTIFY extendedFeedBackgroundChanged)
    Q_PROPERTY(QString extendedFeedBackgroundType READ extendedFeedBackgroundType NOTIFY extendedFeedBackgroundChanged)
    Q_PROPERTY(QString workbookStatus READ workbookStatus NOTIFY workbookStatusChanged)
    Q_PROPERTY(bool previewPlaylistActive READ previewPlaylistActive NOTIFY previewPlaylistChanged)
    Q_PROPERTY(QString previewPlaylistFolderName READ previewPlaylistFolderName NOTIFY previewPlaylistChanged)

    // --- Legacy Timer System Removed ---

    // --- BGM System ---
    Q_PROPERTY(QString bgmPath READ bgmPath NOTIFY bgmChanged)
    Q_PROPERTY(QString bgmCoverArt READ bgmCoverArt NOTIFY bgmChanged)
    Q_PROPERTY(bool isPlayingBgm READ isPlayingBgm NOTIFY bgmChanged)
    Q_PROPERTY(QString bgmTrackName READ bgmTrackName NOTIFY bgmChanged)
    Q_PROPERTY(int bgmCount READ bgmCount NOTIFY bgmChanged)
    Q_PROPERTY(bool bgmShuffle READ bgmShuffle WRITE setBgmShuffle NOTIFY bgmShuffleChanged)
    Q_PROPERTY(int bgmPositionMs READ bgmPositionMs NOTIFY bgmPositionChanged)
    Q_PROPERTY(int bgmDurationMs READ bgmDurationMs NOTIFY bgmDurationChanged)
    Q_PROPERTY(QSortFilterProxyModel* stagedMediaProxy READ stagedMediaProxy CONSTANT)
    Q_PROPERTY(PinnedFolderModel* pinnedFolders READ pinnedFolders CONSTANT)

public:
    explicit BroadcastController(QQmlApplicationEngine *engine, QObject *parent = nullptr);
    ~BroadcastController() override;

    // Getters
    MediaLibraryModel *mediaLibrary() const { return m_libraryModel; }
    MeetingScheduleModel *meetingSchedule() const { return m_meetingModel; }
    CameraDeviceModel *cameraDevices() const { return m_cameraModel; }
    BroadcastEngine *broadcastEngine() const { return m_broadcastEngine; }
    QSortFilterProxyModel *stagedMediaProxy() const { return m_filterProxy; }
    PinnedFolderModel *pinnedFolders() const { return m_pinnedFolders; }
    QCameraDevice programCameraDevice() const { return m_programCameraDevice; }
    QAudioDevice roomAudioOutputDevice() const { return m_roomAudioOutputDevice; }

    QString selectedSegmentId() const { return m_selectedSegmentId; }
    QString meetingType() const { return m_meetingType; }
    QVariantMap currentLanguage() const;
    Q_INVOKABLE void setCurrentLanguage(const QVariantMap &language);
    QString currentLanguageCode() const { return m_languageCode; }
    Q_INVOKABLE void setCurrentLanguageCode(const QString &lang);
    int masterVolume() const { return m_masterVolume; }
    bool mixerMuted() const { return m_mixerMuted; }
    bool isMeetingLive() const { return m_isMeetingLive; }
    bool vcamEnabled() const { return m_vcamEnabled; }
    bool feedExtended() const { return m_feedExtended; }
    bool hasSecondaryScreen() const;
    QString scanStatus() const { return m_scanStatus; }
    bool initialScanComplete() const { return m_initialScanComplete; }

    int extendedFeedScreenIndex() const { return m_extendedFeedScreenIndex; }
    void setExtendedFeedScreenIndex(int index);
    int timerScreenIndex() const { return m_timerScreenIndex; }
    void setTimerScreenIndex(int index);
    Q_INVOKABLE void setTimerFullScreenActive(bool active);
    bool bgmUseCustomFolder() const { return m_bgmUseCustomFolder; }
    void setBgmUseCustomFolder(bool enabled);
    QString bgmCustomFolder() const { return m_bgmCustomFolder; }
    Q_INVOKABLE void browseBgmFolder();
    Q_INVOKABLE QVariantList availableScreens() const;
    Q_INVOKABLE void addCustomLanguage(const QString &name, const QString &code);
    Q_INVOKABLE void removeCustomLanguage(const QString &code);

    QString extendedFeedBackgroundPath() const { return m_extendedFeedBackgroundPath; }
    QString extendedFeedBackgroundType() const { return m_extendedFeedBackgroundType; }
    Q_INVOKABLE void browseExtendedFeedBackground();
    Q_INVOKABLE void clearExtendedFeedBackground();

    QString workbookStatus() const;
    Q_INVOKABLE void refreshWorkbook();

    bool previewPlaylistActive() const { return !m_previewPlaylistIds.isEmpty(); }
    QString previewPlaylistFolderName() const { return m_previewPlaylistFolderName; }
    // Weeks found across every locally downloaded mwb/w publication, for the
    // week-picker dropdown; each entry is {"label", "iso"}.
    Q_INVOKABLE QVariantList availableWorkbookWeeks() const;
    // Pins the workbook to that week (an ISO date string from
    // availableWorkbookWeeks) instead of always matching today; an empty
    // string goes back to automatic (today-based) matching.
    Q_INVOKABLE void selectWorkbookWeek(const QString &isoDate);

    // --- Legacy Timer Getters Removed ---

    QString bgmPath() const;
    QString bgmCoverArt() const { return m_bgmCoverArt; }
    bool isPlayingBgm() const;
    QString bgmTrackName() const;
    int bgmCount() const { return m_bgmPlaylist.count(); }
    bool bgmShuffle() const { return m_bgmShuffle; }
    void setBgmShuffle(bool enabled);
    int bgmPositionMs() const;
    int bgmDurationMs() const;

    // --- Sequence/Media API ---
    Q_INVOKABLE void selectSegment(const QString &id);
    Q_INVOKABLE void bindMediaToSequence(const QString &mediaId);
    Q_INVOKABLE void removeMediaFromSequence(const QString &seqId, const QString &mediaId);
    Q_INVOKABLE void reorderSegmentMedia(const QString &fromMediaId, const QString &toMediaId);
    Q_INVOKABLE void browseAndAddMedia(const QString &seqId, const QString &mediaType);

    // --- General Actions ---
    Q_INVOKABLE void setMeetingTypeStr(const QString &type);
    Q_INVOKABLE void setMasterVolume(int v);
    Q_INVOKABLE void setMixerMuted(bool muted);
    Q_INVOKABLE void setMeetingLive(bool live);
    Q_INVOKABLE void toggleMeetingLive() { setMeetingLive(!m_isMeetingLive); }
    Q_INVOKABLE void setProgramCameraDevice(const QCameraDevice &device);

    Q_INVOKABLE void requestScanJwMedia();
    Q_INVOKABLE void requestScanCustomFolder();
    Q_INVOKABLE QVariantMap findSong(int num, const QString &lang, bool prefVideo, const QString &track);
    Q_INVOKABLE QVariantMap getSong(int number, const QString &langCode) const;
    Q_INVOKABLE void stageMedia(const QString &assetId);
    // Continuous folder playback in Preview -- dragging a pinned folder onto
    // the Preview monitor stages its first video/image, then MonitorView
    // calls advancePreviewPlaylist() each time that item finishes (video
    // EndOfMedia, or a dwell timer for images), wrapping back to the start
    // so it keeps going until stopPreviewPlaylist() is called.
    Q_INVOKABLE void playPinnedFolderInPreview(const QString &folderId);
    Q_INVOKABLE void stopPreviewPlaylist();
    Q_INVOKABLE void advancePreviewPlaylist();
    Q_INVOKABLE void previewMediaByPath(const QString &path);
    Q_INVOKABLE void importMediaToFileSystem(const QString &category);
    Q_INVOKABLE QVariantMap addMediaToSegment(const QString &segmentId, const QString &mediaType);
    Q_INVOKABLE void findAndStageSong(int songNumber, const QString &languageCode, const QString &targetSegmentId);
    // Same resolve+link logic as findAndStageSong, minus the user-facing
    // "song not found" signal -- used by the automated weekly workbook
    // fetch (WorkbookManager), where a not-yet-imported video for an
    // upcoming week is expected/normal, not something to pop a warning
    // about every time it runs in the background.
    QString resolveWeeklySong(int songNumber, const QString &languageCode, const QString &targetSegmentId) {
        return resolveSongToSegment(songNumber, languageCode, targetSegmentId, false);
    }
    // resolveWeeklySong persists via saveState() internally (through
    // resolveSongToSegment); WorkbookManager's other writes -- segment
    // title, linked article images -- go straight through
    // MeetingScheduleModel/MediaLibraryModel and need this to actually be
    // remembered across a restart.
    void persistWorkbookChanges() { saveState(); }
    // Lets WorkbookManager tell us which on-disk folder holds the current
    // week's matched publication (mwb, w, or the standalone Congregation
    // Bible Study book), so addMediaToSegment can default the file picker
    // straight into it instead of the whole Publications root.
    void setWorkbookPublicationFolder(const QString &pubType, const QString &folderPath) {
        if (pubType == QStringLiteral("mwb")) m_mwbPublicationFolder = folderPath;
        else if (pubType == QStringLiteral("w")) m_watchtowerPublicationFolder = folderPath;
        else if (pubType == QStringLiteral("cbs")) m_cbsPublicationFolder = folderPath;
    }
    Q_INVOKABLE void renameCategory(const QString &oldName, const QString &newName);
    Q_INVOKABLE void removeMedia(const QString &id);
    Q_INVOKABLE void openAudienceWindow();
    Q_INVOKABLE void closeAudienceWindow();
    Q_INVOKABLE void toggleAudienceWindow();
    Q_INVOKABLE void toggleZoomBroadcast();
    Q_INVOKABLE bool hasVirtualCameraDriver() const;

    Q_INVOKABLE QVariantList getSupportedLanguages() const;
    Q_INVOKABLE QVariantMap getLanguageMap() const;
    // --- Legacy Timer Invokables Removed ---

    // --- BGM ---
    Q_INVOKABLE void toggleBgmPlayback();
    Q_INVOKABLE void nextBgm();
    Q_INVOKABLE void backBgm();
    Q_INVOKABLE void stopBgm();
    Q_INVOKABLE void scanBgmFolder();
    Q_INVOKABLE void addFilesToBgm(const QStringList &paths);
    Q_INVOKABLE void toggleBgmShuffle() { setBgmShuffle(!m_bgmShuffle); }
    Q_INVOKABLE void seekBgm(int ms);

    // --- Pinned Folders ---
    Q_INVOKABLE QString createPinnedFolder(const QString &name);
    Q_INVOKABLE void renamePinnedFolder(const QString &folderId, const QString &newName);
    Q_INVOKABLE void deletePinnedFolder(const QString &folderId);
    Q_INVOKABLE void pinMediaToFolder(const QString &folderId, const QString &mediaId);
    Q_INVOKABLE void unpinMediaFromFolder(const QString &folderId, const QString &mediaId);
    // Multi-select file picker -> imports each file and links it into the folder.
    Q_INVOKABLE void browseAndAddFilesToPinnedFolder(const QString &folderId);
    // For OS drag-and-drop: paths may be local paths or file:// URLs.
    Q_INVOKABLE void importFilesToPinnedFolder(const QString &folderId, const QStringList &pathsOrUrls);

signals:
    void selectedSegmentIdChanged();
    void meetingTypeChanged();
    void currentLanguageCodeChanged();
    void currentLanguageChanged();
    void masterVolumeChanged();
    void mixerMutedChanged();
    void isMeetingLiveChanged();
    void vcamEnabledChanged();
    void programCameraDeviceChanged();
    void scanStatusChanged();
    void initialScanCompleteChanged();
    void hasSecondaryScreenChanged();
    void extendedFeedScreenIndexChanged();
    void timerScreenIndexChanged();
    void bgmSettingsChanged();
    void extendedFeedBackgroundChanged();
    void workbookStatusChanged();
    void previewPlaylistChanged();
    void languagesChanged();
    void songNotFound(int songNumber);
    void songNotFoundInLanguage(int songNumber, const QString &languageName);
    void isProgramPausedChanged();
    void bgmChanged();
    void bgmShuffleChanged();
    void bgmPositionChanged();
    void bgmDurationChanged();
    void feedExtendedChanged();

private slots:
    void updateScreenCount();
    void onMediaFound(MediaType type, const QString &name, const QString &absolutePath, const QImage &thumbnail, const QDateTime &creationDate);
    void onScanFinished();
    void onBgmStatusChanged(QMediaPlayer::MediaStatus status);

private:
    void ensureExtractor();
    void registerCameras();
    void loadBgmTrack(int index);
    void saveState();
    void loadState();
    bool openZoomWindow();
    void indexMediaAsset(const QVariantMap &asset);
    void clearMediaIndexes();
    void applyLanguageCode(const QString &languageCode, bool persist, bool reResolveSongs);
    QString languageName(const QString &languageCode) const;
    QString resolveSongToSegment(int songNumber, const QString &languageCode, const QString &targetSegmentId, bool warnOnMissing);
    void reResolveSongSegmentsForCurrentLanguage();
    // Builds a MediaAsset entry for one file, adds it to the library + index,
    // enqueues its thumbnail, and returns its new id. Shared by every
    // multi-file import path (browse dialogs and OS drag-and-drop).
    QString importOneFile(const QString &absolutePath, const QString &category);
    static QString normalizeDroppedPath(const QString &pathOrUrl);

    QQmlApplicationEngine *m_engine;
    MediaLibraryModel *m_libraryModel = nullptr;
    StagedMediaProxyModel *m_filterProxy;
    MeetingScheduleModel *m_meetingModel = nullptr;
    CameraDeviceModel *m_cameraModel;
    BroadcastEngine *m_broadcastEngine;
    VirtualCameraManager *m_vcamManager;
    PinnedFolderModel *m_pinnedFolders = nullptr;

    QString m_selectedSegmentId;
    QString m_meetingType = "midweek";
    QString m_languageCode = "E";
    int m_masterVolume = 100;
    bool m_mixerMuted = false;
    bool m_isMeetingLive = false;
    bool m_vcamEnabled = false;
    bool m_feedExtended = false;
    QString m_scanStatus;
    bool m_initialScanComplete = false;
    QCameraDevice m_programCameraDevice;
    int m_extendedFeedScreenIndex = -1;
    int m_timerScreenIndex = -1;
    QPointer<QQuickWindow> m_timerWindow;
    bool m_bgmUseCustomFolder = false;
    QString m_bgmCustomFolder;
    QString m_extendedFeedBackgroundPath;
    QString m_extendedFeedBackgroundType;
    WorkbookManager *m_workbookManager = nullptr;
    bool m_workbookAutoRefreshStarted = false;
    QStringList m_previewPlaylistIds;
    int m_previewPlaylistIndex = -1;
    QString m_previewPlaylistFolderName;
    QString m_mwbPublicationFolder;
    QString m_watchtowerPublicationFolder;
    QString m_cbsPublicationFolder;
    QVariantList m_customLanguages;
    QAudioDevice m_roomAudioOutputDevice;

    // BGM
    QMediaPlayer *m_bgmPlayer = nullptr;
    QAudioOutput *m_bgmAudio = nullptr;
    QStringList m_bgmPlaylist;
    QString m_bgmCoverArt;
    int m_bgmIndex = 0;
    bool m_bgmShuffle = false;
    QList<int> m_bgmShuffleHistory; // indices played this shuffle session, for "back"

    // Thumbnail extractor
    MediaThumbnailManager *m_thumbManager = nullptr;

    MediaExtractor *m_extractor = nullptr;
    QPointer<QQuickWindow> m_audienceWindow;
    QPointer<QQuickWindow> m_zoomWindow;
    QTimer *m_duckingExemptionTimer = nullptr;
    QHash<QString, QVariantMap> m_mediaIndexByPath;
    QHash<QString, QVariantMap> m_songIndex;
};
