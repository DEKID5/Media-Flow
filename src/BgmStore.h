#pragma once

#include <QAudioDevice>
#include <QAudioOutput>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QMediaPlayer>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class QVariantAnimation;

struct BgmTrack
{
    QString id;
    QString title;
    QStringList artistIds;
    QString albumId;
    int discNumber = 1;
    int trackNumber = 0;
    qint64 durationSec = 0;
    QUrl sourceUrl;
    QUrl artworkUrl;
    QString artistName;
    QString albumTitle;
    int year = 0;
};

struct BgmAlbum
{
    QString id;
    QString title;
    QString artistId;
    QString artistName;
    QString coverUrl;
    int year = 0;
    QStringList trackIds;
};

struct BgmPlaylist
{
    QString id;
    QString name;
    QStringList trackIds;
    QDateTime updatedAt;
};

class BgmAudioEngine final : public QObject
{
    Q_OBJECT

public:
    explicit BgmAudioEngine(QObject *parent = nullptr);

    void setAudioDevice(const QAudioDevice &device);
    void setVolume(qreal volume);
    qreal volume() const;
    void setMuted(bool muted);
    bool isMuted() const;
    void load(const QUrl &source, quint64 loadToken, bool autoplay);
    void play();
    void pause();
    void stop();
    void clear();
    void seek(qint64 positionMs);
    void beginSeeking();
    void endSeeking();
    qint64 positionMs() const;
    qint64 durationMs() const;
    bool isPlaying() const;

signals:
    void loadFinished(quint64 loadToken, bool success, const QString &error);
    void mediaEnded(quint64 loadToken);
    void playingChanged(bool playing);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void metadataChanged(const QVariantMap &metadata);

private:
    void reportLoadFailure(quint64 loadToken, const QUrl &source, const QString &error);

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QMetaObject::Connection m_statusConnection;
    QMetaObject::Connection m_errorConnection;
    QUrl m_loadingSource;
    quint64 m_loadToken = 0;
    quint64 m_reportedLoadToken = 0;
    qreal m_volume = 0.6;
    bool m_muted = false;
    bool m_autoplay = false;
    bool m_isSeeking = false;
    bool m_loadReady = false;
};

class BgmStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bgmPath READ bgmPath NOTIFY playbackStateChanged)
    Q_PROPERTY(QString bgmCoverArt READ bgmCoverArt NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isPlayingBgm READ isPlayingBgm NOTIFY playbackStateChanged)
    Q_PROPERTY(QString bgmTrackName READ bgmTrackName NOTIFY playbackStateChanged)
    Q_PROPERTY(int bgmCount READ bgmCount NOTIFY playbackStateChanged)
    Q_PROPERTY(bool bgmShuffle READ bgmShuffle NOTIFY playbackStateChanged)
    Q_PROPERTY(int bgmIndex READ bgmIndex NOTIFY playbackStateChanged)
    Q_PROPERTY(int bgmPositionMs READ bgmPositionMs NOTIFY positionChanged)
    Q_PROPERTY(int bgmDurationMs READ bgmDurationMs NOTIFY durationChanged)
    Q_PROPERTY(qreal bgmVolume READ bgmVolume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isMuted READ isMuted NOTIFY volumeChanged)
    Q_PROPERTY(int bgmRepeatMode READ bgmRepeatMode NOTIFY playbackStateChanged)
    Q_PROPERTY(QString status READ status NOTIFY playbackStateChanged)
    Q_PROPERTY(QVariantMap activeSource READ activeSource NOTIFY playbackStateChanged)
    Q_PROPERTY(QStringList originalQueueIds READ originalQueueIds NOTIFY playbackStateChanged)
    Q_PROPERTY(QStringList activeQueueIds READ activeQueueIds NOTIFY playbackStateChanged)
    Q_PROPERTY(QString activeTrackId READ activeTrackId NOTIFY playbackStateChanged)
    Q_PROPERTY(QVariantMap playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QVariantMap viewState READ viewState NOTIFY viewStateChanged)
    Q_PROPERTY(QString activeTab READ activeTab NOTIFY viewStateChanged)
    Q_PROPERTY(QString viewedAlbumId READ viewedAlbumId NOTIFY viewStateChanged)
    Q_PROPERTY(QString viewedPlaylistId READ viewedPlaylistId NOTIFY viewStateChanged)
    Q_PROPERTY(QVariantList bgmLibraryTracks READ bgmLibraryTracks NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList bgmPlaylistTracks READ bgmPlaylistTracks NOTIFY playlistTracksChanged)
    Q_PROPERTY(QVariantList bgmAlbumTracks READ bgmAlbumTracks NOTIFY viewStateChanged)
    Q_PROPERTY(QVariantList bgmAlbums READ bgmAlbums NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList bgmPlaylists READ bgmPlaylists NOTIFY playlistsChanged)
    Q_PROPERTY(QStringList bgmPlaylistNames READ bgmPlaylistNames NOTIFY playlistsChanged)
    Q_PROPERTY(QString currentBgmPlaylistName READ currentBgmPlaylistName NOTIFY viewStateChanged)

public:
    explicit BgmStore(QObject *parent = nullptr);

    void setAudioDevice(const QAudioDevice &device);
    void setLocalLibraryPaths(const QStringList &paths);
    QStringList localLibraryPaths() const;
    void setImportedLibraryTracks(const QVariantList &tracks);
    void setPlaylistsFromPaths(const QMap<QString, QStringList> &playlists,
                               const QString &viewedPlaylistName);
    QMap<QString, QStringList> playlistsAsPaths() const;
    void setVolume(qreal volume);
    qreal volume() const;
    void setRepeatMode(int mode);
    int repeatMode() const;
    void setShuffle(bool enabled);
    bool shuffle() const;
    void setMuted(bool muted);
    bool muted() const;
    void setTrackArtwork(const QString &path, const QString &artworkUrl,
                         const QString &title = {});
    QString artworkForPath(const QString &path) const;
    void shutdown();

    QString bgmPath() const;
    QString bgmCoverArt() const;
    bool isPlayingBgm() const;
    QString bgmTrackName() const;
    int bgmCount() const;
    bool bgmShuffle() const;
    int bgmIndex() const;
    int bgmPositionMs() const;
    int bgmDurationMs() const;
    qreal bgmVolume() const;
    bool isMuted() const;
    int bgmRepeatMode() const;
    QString status() const;
    QVariantMap activeSource() const;
    QStringList originalQueueIds() const;
    QStringList activeQueueIds() const;
    QString activeTrackId() const;
    QVariantMap playbackState() const;
    QVariantMap viewState() const;
    QString activeTab() const;
    QString viewedAlbumId() const;
    QString viewedPlaylistId() const;
    QVariantList bgmLibraryTracks() const;
    QVariantList bgmPlaylistTracks() const;
    QVariantList bgmAlbumTracks() const;
    QVariantList bgmAlbums() const;
    QVariantList bgmPlaylists() const;
    QStringList bgmPlaylistNames() const;
    QString currentBgmPlaylistName() const;

    Q_INVOKABLE void setActiveTab(const QString &tab);
    Q_INVOKABLE void viewAlbums();
    Q_INVOKABLE void viewPlaylist(const QString &playlistId);
    Q_INVOKABLE void viewAlbum(const QString &albumId);
    Q_INVOKABLE void selectBgmPlaylist(const QString &name);
    Q_INVOKABLE bool createBgmPlaylist(const QString &name);
    Q_INVOKABLE void addBgmTracksToPlaylist(const QStringList &paths);
    Q_INVOKABLE void removeBgmTrack(int index);
    Q_INVOKABLE void clearBgmPlaylist();
    Q_INVOKABLE void addTrackToPlaylist(const QString &playlistId, const QString &trackId);
    Q_INVOKABLE void removeTrackFromPlaylist(const QString &playlistId, int index);
    Q_INVOKABLE void reorderPlaylist(const QString &playlistId, int fromIndex, int toIndex);
    Q_INVOKABLE void addPathsToLibrary(const QStringList &paths, bool addToViewedPlaylist = false);
    Q_INVOKABLE void playFromSource(const QString &type, const QString &id,
                                    const QStringList &trackIds, int startIndex = 0);
    Q_INVOKABLE void playBgmTrack(int index);
    Q_INVOKABLE void playBgmAlbumTrack(int index);
    Q_INVOKABLE void playBgmFile(const QString &path);
    Q_INVOKABLE void toggleBgmPlayback();
    Q_INVOKABLE void togglePlayPause() { toggleBgmPlayback(); }
    Q_INVOKABLE void nextBgm();
    Q_INVOKABLE void nextTrack() { nextBgm(); }
    Q_INVOKABLE void backBgm();
    Q_INVOKABLE void previousTrack() { backBgm(); }
    Q_INVOKABLE void stopBgm();
    Q_INVOKABLE void fadeOutBgm(int durationMs = 2500);
    Q_INVOKABLE void toggleBgmShuffle();
    Q_INVOKABLE void toggleShuffle() { toggleBgmShuffle(); }
    Q_INVOKABLE void cycleBgmRepeatMode();
    Q_INVOKABLE void cycleRepeatMode() { cycleBgmRepeatMode(); }
    Q_INVOKABLE void seekBgm(int positionMs);
    Q_INVOKABLE void beginBgmSeek();
    Q_INVOKABLE void endBgmSeek();
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void addToQueue(const QString &trackId);
    Q_INVOKABLE void playNext(const QString &trackId);

signals:
    void playbackStateChanged();
    void viewStateChanged();
    void libraryChanged();
    void playlistsChanged();
    void playlistTracksChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void trackMetadataUpdated(const QString &path, const QString &title,
                              const QString &thumbnailUrl);
    void playbackError(const QString &message);

private:
    enum class PlaybackStatus { Idle, Loading, Ready, Playing, Paused, Ended, Error };

    QString ensureTrack(const QString &path, const QString &title = {},
                        const QString &artworkUrl = {});
    QString trackIdForPath(const QString &path) const;
    QString playlistIdForName(const QString &name) const;
    QStringList libraryTrackIds() const;
    QStringList validTrackIds(const QStringList &trackIds) const;
    QVariantMap trackMap(const QString &trackId, int index, bool inPlaylist) const;
    QVariantList trackList(const QStringList &trackIds, bool inPlaylist) const;
    BgmPlaylist *playlist(const QString &playlistId);
    const BgmPlaylist *playlist(const QString &playlistId) const;
    void rebuildAlbums();
    void emitPlaylistChanges();
    void setStatus(PlaybackStatus status);
    QString statusName() const;
    void loadCurrentTrack();
    void handleLoadFinished(quint64 loadToken, bool success, const QString &error);
    void handleMediaEnded(quint64 loadToken);
    void handleMetadata(const QVariantMap &metadata);
    void advanceToIndex(int index, bool autoplay);
    void skipFailedTrack(const QString &failedTrackId);
    void updatePlaylistTimestamp(BgmPlaylist &playlist);

    BgmAudioEngine *m_audioEngine = nullptr;
    QHash<QString, BgmTrack> m_tracksById;
    QHash<QString, QString> m_trackIdBySource;
    QHash<QString, BgmAlbum> m_albumsById;
    QHash<QString, BgmPlaylist> m_playlistsById;
    QHash<QString, QString> m_playlistIdByName;
    QStringList m_localLibraryTrackIds;
    QStringList m_importedLibraryTrackIds;
    QStringList m_albumOrder;
    QStringList m_originalQueueIds;
    QStringList m_activeQueueIds;
    QString m_activeSourceType = QStringLiteral("LIBRARY");
    QString m_activeSourceId;
    QString m_activeTrackId;
    QString m_activeTab = QStringLiteral("LIBRARY");
    QString m_viewedAlbumId;
    QString m_viewedPlaylistId;
    PlaybackStatus m_status = PlaybackStatus::Idle;
    int m_currentIndex = -1;
    int m_repeatMode = 0;
    qreal m_volume = 0.6;
    qreal m_lastAudibleVolume = 0.6;
    bool m_shuffle = false;
    bool m_muted = false;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    quint64 m_loadToken = 0;
    QVariantAnimation *m_fadeAnimation = nullptr;
};
