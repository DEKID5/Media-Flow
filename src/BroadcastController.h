#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QAudioOutput>
#include <QMediaPlayer>

#include "MediaLibraryModel.h"
#include "MediaLibraryProxyModel.h"
#include "MeetingScheduleModel.h"
#include "CameraDeviceModel.h"
#include "BroadcastEngine.h"
#include "StagedMediaProxyModel.h"
#include "MediaAsset.h"
#include "FfmpegUdpBridge.h"

class MediaExtractor;
class MediaThumbnailManager;
enum class MediaType;
class QQuickWindow;

/**
 * @brief The BroadcastController class acts as the central bridge between C++ and QML.
 * Owns all models, the broadcast engine, BGM player, and timer system.
 */
class BroadcastController final : public QObject
{
    Q_OBJECT

    // --- Models ---
    Q_PROPERTY(MediaLibraryModel *mediaLibrary READ mediaLibrary CONSTANT)
    Q_PROPERTY(MediaLibraryProxyModel *filterableMedia READ filterableMedia CONSTANT)
    Q_PROPERTY(MeetingScheduleModel *meetingSchedule READ meetingSchedule CONSTANT)
    Q_PROPERTY(CameraDeviceModel *cameraDevices READ cameraDevices CONSTANT)
    Q_PROPERTY(BroadcastEngine *broadcastEngine READ broadcastEngine CONSTANT)

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

    // --- Legacy Timer System Removed ---

    // --- BGM System ---
    Q_PROPERTY(QString bgmPath READ bgmPath NOTIFY bgmChanged)
    Q_PROPERTY(bool isPlayingBgm READ isPlayingBgm NOTIFY bgmChanged)
    Q_PROPERTY(QString bgmTrackName READ bgmTrackName NOTIFY bgmChanged)
    Q_PROPERTY(int bgmCount READ bgmCount NOTIFY bgmChanged)
    Q_PROPERTY(QSortFilterProxyModel* stagedMediaProxy READ stagedMediaProxy CONSTANT)

public:
    explicit BroadcastController(QQmlApplicationEngine *engine, QObject *parent = nullptr);
    ~BroadcastController() override;

    // Getters
    MediaLibraryModel *mediaLibrary() const { return m_libraryModel; }
    MediaLibraryProxyModel *filterableMedia() const { return m_proxyModel; }
    MeetingScheduleModel *meetingSchedule() const { return m_meetingModel; }
    CameraDeviceModel *cameraDevices() const { return m_cameraModel; }
    BroadcastEngine *broadcastEngine() const { return m_broadcastEngine; }
    QSortFilterProxyModel *stagedMediaProxy() const { return m_filterProxy; }

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

    // --- Legacy Timer Getters Removed ---

    QString bgmPath() const;
    bool isPlayingBgm() const;
    QString bgmTrackName() const;
    int bgmCount() const { return m_bgmPlaylist.count(); }

    // --- Sequence/Media API ---
    Q_INVOKABLE void selectSegment(const QString &id);
    Q_INVOKABLE void bindMediaToSequence(const QString &mediaId);
    Q_INVOKABLE void removeMediaFromSequence(const QString &seqId, const QString &mediaId);
    Q_INVOKABLE void browseAndAddMedia(const QString &seqId, const QString &mediaType);

    // --- General Actions ---
    Q_INVOKABLE void setMeetingTypeStr(const QString &type);
    Q_INVOKABLE void setMasterVolume(int v);
    Q_INVOKABLE void setMixerMuted(bool muted);
    Q_INVOKABLE void setMeetingLive(bool live);
    Q_INVOKABLE void toggleMeetingLive() { setMeetingLive(!m_isMeetingLive); }

    Q_INVOKABLE void requestScanJwMedia();
    Q_INVOKABLE void requestScanCustomFolder();
    Q_INVOKABLE QVariantMap findSong(int num, const QString &lang, bool prefVideo, const QString &track);
    Q_INVOKABLE void stageMedia(const QString &assetId);
    Q_INVOKABLE void previewMediaByPath(const QString &path);
    Q_INVOKABLE void importMediaToFileSystem(const QString &category);
    Q_INVOKABLE QVariantMap addMediaToSegment(const QString &segmentId, const QString &mediaType);
    Q_INVOKABLE void findAndStageSong(int songNumber, const QString &languageCode, const QString &targetSegmentId);
    Q_INVOKABLE void renameCategory(const QString &oldName, const QString &newName);
    Q_INVOKABLE void removeMedia(const QString &id);
    Q_INVOKABLE void resetApp();

    Q_INVOKABLE void openAudienceWindow();
    Q_INVOKABLE void closeAudienceWindow();
    Q_INVOKABLE void toggleAudienceWindow();
    Q_INVOKABLE void toggleZoomBroadcast();

    Q_INVOKABLE QVariantList getSupportedLanguages() const;
    Q_INVOKABLE QVariantMap getLanguageMap() const;
    // --- Legacy Timer Invokables Removed ---

    // --- BGM ---
    Q_INVOKABLE void toggleBgmPlayback();
    Q_INVOKABLE void nextBgm();
    Q_INVOKABLE void backBgm();
    Q_INVOKABLE void stopBgm();
    Q_INVOKABLE void scanBgmFolder();

signals:
    void selectedSegmentIdChanged();
    void meetingTypeChanged();
    void currentLanguageCodeChanged();
    void currentLanguageChanged();
    void masterVolumeChanged();
    void mixerMutedChanged();
    void isMeetingLiveChanged();
    void vcamEnabledChanged();
    void scanStatusChanged();
    void hasSecondaryScreenChanged();
    void songNotFound(int songNumber);
    void songNotFoundInLanguage(int songNumber, const QString &languageName);
    void isProgramPausedChanged();
    void bgmChanged();
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
    QString languageName(const QString &languageCode) const;
    QString resolveSongToSegment(int songNumber, const QString &languageCode, const QString &targetSegmentId, bool warnOnMissing);
    void reResolveSongSegmentsForCurrentLanguage();

    QQmlApplicationEngine *m_engine;
    MediaLibraryModel *m_libraryModel = nullptr;
    MediaLibraryProxyModel *m_proxyModel = nullptr;
    StagedMediaProxyModel *m_filterProxy;
    MeetingScheduleModel *m_meetingModel = nullptr;
    CameraDeviceModel *m_cameraModel;
    BroadcastEngine *m_broadcastEngine;
    FfmpegUdpBridge *m_bridge;

    QString m_selectedSegmentId;
    QString m_meetingType = "midweek";
    QString m_languageCode = "E";
    int m_masterVolume = 100;
    bool m_mixerMuted = false;
    bool m_isMeetingLive = false;
    bool m_vcamEnabled = false;
    bool m_feedExtended = false;
    QString m_scanStatus;

    // BGM
    QMediaPlayer *m_bgmPlayer = nullptr;
    QAudioOutput *m_bgmAudio = nullptr;
    QStringList m_bgmPlaylist;
    int m_bgmIndex = 0;

    // Thumbnail extractor
    MediaThumbnailManager *m_thumbManager = nullptr;

    MediaExtractor *m_extractor = nullptr;
    QPointer<QQuickWindow> m_audienceWindow;
};
