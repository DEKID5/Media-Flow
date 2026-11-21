#include "BgmStore.h"

#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantAnimation>
#include <QDebug>
#include <algorithm>

namespace {

QString normalizedPath(const QString &rawPath)
{
    const QString localPath = rawPath.startsWith(QStringLiteral("file:"))
        ? QUrl(rawPath).toLocalFile()
        : rawPath;
    return QDir::cleanPath(QFileInfo(localPath).absoluteFilePath());
}

QString stableId(const QString &prefix, const QString &value)
{
    const QByteArray hash = QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256)
                                .toHex().left(24);
    return prefix + QString::fromLatin1(hash);
}

QStringList stringListFromVariant(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QStringList)
        return value.toStringList();
    if (value.canConvert<QStringList>())
        return value.toStringList();
    const QString text = value.toString().trimmed();
    if (text.isEmpty())
        return {};
    return text.split(QRegularExpression(QStringLiteral("\\s*[,;]\\s*")), Qt::SkipEmptyParts);
}

QString artistIdForName(const QString &name)
{
    return stableId(QStringLiteral("artist:"), name.trimmed().toCaseFolded());
}

QString albumIdForName(const QString &artistId, const QString &albumName)
{
    return stableId(QStringLiteral("album:"), artistId + QLatin1Char('|') + albumName.trimmed().toCaseFolded());
}

QString stablePlaylistIdForName(const QString &name)
{
    return stableId(QStringLiteral("playlist:"), name.trimmed().toCaseFolded());
}

} // namespace

BgmAudioEngine::BgmAudioEngine(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    m_audioOutput->setVolume(static_cast<float>(m_volume));
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 positionMs) {
        if (!m_isSeeking)
            emit positionChanged(positionMs);
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, &BgmAudioEngine::durationChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
        if (m_player->source() != m_loadingSource)
            return;

        const QMediaMetaData metadata = m_player->metaData();
        const QVariant dateValue = metadata.value(QMediaMetaData::Date);
        const QDate date = dateValue.canConvert<QDate>()
            ? dateValue.toDate()
            : QDate::fromString(dateValue.toString().left(10), Qt::ISODate);
        emit metadataChanged({
            {QStringLiteral("title"), metadata.value(QMediaMetaData::Title).toString().trimmed()},
            {QStringLiteral("albumTitle"), metadata.value(QMediaMetaData::AlbumTitle).toString().trimmed()},
            {QStringLiteral("albumArtist"), metadata.value(QMediaMetaData::AlbumArtist).toString().trimmed()},
            {QStringLiteral("artists"), stringListFromVariant(metadata.value(QMediaMetaData::ContributingArtist))},
            {QStringLiteral("leadPerformer"), metadata.value(QMediaMetaData::LeadPerformer).toString().trimmed()},
            {QStringLiteral("trackNumber"), metadata.value(QMediaMetaData::TrackNumber).toInt()},
            {QStringLiteral("year"), date.isValid() ? date.year() : 0}
        });
    });
}

void BgmAudioEngine::setAudioDevice(const QAudioDevice &device)
{
    if (m_audioOutput && m_audioOutput->device() != device)
        m_audioOutput->setDevice(device);
}

void BgmAudioEngine::setVolume(qreal volume)
{
    m_volume = qBound<qreal>(0.0, volume, 1.0);
    if (m_audioOutput)
        m_audioOutput->setVolume(static_cast<float>(m_volume));
}

qreal BgmAudioEngine::volume() const
{
    return m_volume;
}

void BgmAudioEngine::setMuted(bool muted)
{
    m_muted = muted;
    if (m_audioOutput)
        m_audioOutput->setMuted(m_muted);
}

bool BgmAudioEngine::isMuted() const
{
    return m_muted;
}

void BgmAudioEngine::load(const QUrl &source, quint64 loadToken, bool autoplay)
{
    m_loadToken = loadToken;
    m_reportedLoadToken = 0;
    m_loadingSource = source;
    m_autoplay = autoplay;
    m_loadReady = false;
    m_isSeeking = false;

    disconnect(m_statusConnection);
    disconnect(m_errorConnection);
    m_player->stop();

    m_statusConnection = connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
        [this, loadToken, source](QMediaPlayer::MediaStatus status) {
            if (loadToken != m_loadToken || m_player->source() != source)
                return;
            if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                && m_reportedLoadToken != loadToken) {
                m_reportedLoadToken = loadToken;
                m_loadReady = true;
                emit loadFinished(loadToken, true, {});
                if (m_autoplay) {
                    m_autoplay = false;
                    m_player->play();
                }
            } else if (status == QMediaPlayer::InvalidMedia) {
                reportLoadFailure(loadToken, source, m_player->errorString());
            } else if (status == QMediaPlayer::EndOfMedia) {
                emit mediaEnded(loadToken);
            }
        });

    m_errorConnection = connect(m_player, &QMediaPlayer::errorOccurred, this,
        [this, loadToken, source](QMediaPlayer::Error, const QString &message) {
            reportLoadFailure(loadToken, source, message);
        });
    m_player->setSource(source);
}

void BgmAudioEngine::reportLoadFailure(quint64 loadToken, const QUrl &source, const QString &error)
{
    if (loadToken != m_loadToken || source != m_loadingSource
        || m_player->source() != source || m_reportedLoadToken == loadToken) {
        return;
    }
    m_reportedLoadToken = loadToken;
    m_loadReady = false;
    m_autoplay = false;
    emit loadFinished(loadToken, false, error.isEmpty() ? QStringLiteral("Unable to load audio track") : error);
}

void BgmAudioEngine::play()
{
    if (!m_loadReady) {
        m_autoplay = true;
        return;
    }
    m_autoplay = false;
    m_player->play();
}

void BgmAudioEngine::pause()
{
    m_autoplay = false;
    m_player->pause();
}

void BgmAudioEngine::stop()
{
    m_autoplay = false;
    m_player->stop();
}

void BgmAudioEngine::clear()
{
    m_autoplay = false;
    m_loadReady = false;
    m_loadingSource = {};
    disconnect(m_statusConnection);
    disconnect(m_errorConnection);
    m_player->stop();
    m_player->setSource({});
}

void BgmAudioEngine::seek(qint64 positionMs)
{
    if (!m_player)
        return;
    m_player->setPosition(qBound<qint64>(0, positionMs, m_player->duration()));
}

void BgmAudioEngine::beginSeeking()
{
    m_isSeeking = true;
}

void BgmAudioEngine::endSeeking()
{
    m_isSeeking = false;
    emit positionChanged(m_player ? m_player->position() : 0);
}

qint64 BgmAudioEngine::positionMs() const
{
    return m_player ? m_player->position() : 0;
}

qint64 BgmAudioEngine::durationMs() const
{
    return m_player ? m_player->duration() : 0;
}

bool BgmAudioEngine::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

BgmStore::BgmStore(QObject *parent)
    : QObject(parent)
    , m_audioEngine(new BgmAudioEngine(this))
{
    connect(m_audioEngine, &BgmAudioEngine::loadFinished,
            this, &BgmStore::handleLoadFinished);
    connect(m_audioEngine, &BgmAudioEngine::mediaEnded,
            this, &BgmStore::handleMediaEnded);
    connect(m_audioEngine, &BgmAudioEngine::playingChanged, this, [this](bool playing) {
        if (playing)
            setStatus(PlaybackStatus::Playing);
        else if (m_status == PlaybackStatus::Playing)
            setStatus(PlaybackStatus::Paused);
    });
    connect(m_audioEngine, &BgmAudioEngine::positionChanged, this, [this](qint64 positionMs) {
        m_positionMs = qMax<qint64>(0, positionMs);
        emit positionChanged();
        emit playbackStateChanged();
    });
    connect(m_audioEngine, &BgmAudioEngine::durationChanged, this, [this](qint64 durationMs) {
        m_durationMs = qMax<qint64>(0, durationMs);
        if (!m_activeTrackId.isEmpty() && m_tracksById.contains(m_activeTrackId))
            m_tracksById[m_activeTrackId].durationSec = m_durationMs / 1000;
        emit durationChanged();
        emit playbackStateChanged();
    });
    connect(m_audioEngine, &BgmAudioEngine::metadataChanged,
            this, &BgmStore::handleMetadata);
}

void BgmStore::setAudioDevice(const QAudioDevice &device)
{
    m_audioEngine->setAudioDevice(device);
}

QString BgmStore::ensureTrack(const QString &rawPath, const QString &title, const QString &artworkUrl)
{
    const QString path = normalizedPath(rawPath);
    if (path.isEmpty() || !QFileInfo(path).isFile())
        return {};

    const QString key = path.toCaseFolded();
    const auto existing = m_trackIdBySource.constFind(key);
    if (existing != m_trackIdBySource.cend()) {
        BgmTrack &track = m_tracksById[existing.value()];
        if (!title.trimmed().isEmpty())
            track.title = title.trimmed();
        if (!artworkUrl.isEmpty())
            track.artworkUrl = QUrl(artworkUrl);
        return existing.value();
    }

    const QString id = stableId(QStringLiteral("track:"), key);
    BgmTrack track;
    track.id = id;
    track.title = title.trimmed().isEmpty() ? QFileInfo(path).completeBaseName() : title.trimmed();
    track.sourceUrl = QUrl::fromLocalFile(path);
    track.artworkUrl = QUrl(artworkUrl);
    track.albumTitle = QFileInfo(path).dir().dirName();
    if (track.albumTitle.isEmpty())
        track.albumTitle = QStringLiteral("Unknown Album");
    track.artistName = QStringLiteral("Unknown Artist");
    const QString artistId = artistIdForName(track.artistName);
    track.artistIds = {artistId};
    track.albumId = albumIdForName(artistId, track.albumTitle);
    m_trackIdBySource.insert(key, id);
    m_tracksById.insert(id, track);
    return id;
}

QString BgmStore::trackIdForPath(const QString &path) const
{
    return m_trackIdBySource.value(normalizedPath(path).toCaseFolded());
}

void BgmStore::setLocalLibraryPaths(const QStringList &paths)
{
    QStringList ids;
    QSet<QString> seen;
    for (const QString &path : paths) {
        const QString id = ensureTrack(path);
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            ids.append(id);
        }
    }
    if (ids == m_localLibraryTrackIds)
        return;
    m_localLibraryTrackIds = ids;
    rebuildAlbums();
    emit libraryChanged();
}

QStringList BgmStore::localLibraryPaths() const
{
    QStringList paths;
    paths.reserve(m_localLibraryTrackIds.size());
    for (const QString &id : m_localLibraryTrackIds) {
        const auto track = m_tracksById.constFind(id);
        if (track != m_tracksById.cend())
            paths.append(track->sourceUrl.toLocalFile());
    }
    return paths;
}

void BgmStore::setImportedLibraryTracks(const QVariantList &tracks)
{
    QStringList ids;
    QSet<QString> seen;
    bool metadataChanged = false;
    for (const QVariant &entry : tracks) {
        const QVariantMap map = entry.toMap();
        const QString path = map.value(QStringLiteral("path")).toString();
        const QString existingId = trackIdForPath(path);
        const BgmTrack previousTrack = m_tracksById.value(existingId);
        const QString id = ensureTrack(path, map.value(QStringLiteral("name")).toString(),
                                       map.value(QStringLiteral("thumbnailPath")).toString());
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            ids.append(id);
            const BgmTrack updatedTrack = m_tracksById.value(id);
            metadataChanged = metadataChanged
                || previousTrack.title != updatedTrack.title
                || previousTrack.artworkUrl != updatedTrack.artworkUrl;
        }
    }
    if (ids == m_importedLibraryTrackIds && !metadataChanged)
        return;
    const bool membershipChanged = ids != m_importedLibraryTrackIds;
    m_importedLibraryTrackIds = ids;
    if (membershipChanged || metadataChanged)
        rebuildAlbums();
    emit libraryChanged();
    if (metadataChanged)
        emit playlistTracksChanged();
}

void BgmStore::setPlaylistsFromPaths(const QMap<QString, QStringList> &playlists,
                                     const QString &viewedPlaylistName)
{
    m_playlistsById.clear();
    m_playlistIdByName.clear();
    for (auto it = playlists.cbegin(); it != playlists.cend(); ++it) {
        const QString name = it.key().trimmed();
        if (name.isEmpty())
            continue;
        BgmPlaylist saved;
        saved.id = stablePlaylistIdForName(name);
        saved.name = name;
        saved.updatedAt = QDateTime::currentDateTimeUtc();
        QSet<QString> seen;
        for (const QString &path : it.value()) {
            const QString trackId = ensureTrack(path);
            if (!trackId.isEmpty() && !seen.contains(trackId)) {
                seen.insert(trackId);
                saved.trackIds.append(trackId);
                if (!m_localLibraryTrackIds.contains(trackId))
                    m_localLibraryTrackIds.append(trackId);
            }
        }
        m_playlistIdByName.insert(name.toCaseFolded(), saved.id);
        m_playlistsById.insert(saved.id, saved);
    }
    if (m_playlistsById.isEmpty()) {
        BgmPlaylist saved;
        saved.id = playlistIdForName(QStringLiteral("My Playlist"));
        saved.name = QStringLiteral("My Playlist");
        saved.updatedAt = QDateTime::currentDateTimeUtc();
        m_playlistsById.insert(saved.id, saved);
        m_playlistIdByName.insert(saved.name.toCaseFolded(), saved.id);
    }
    QString selectedId = playlistIdForName(viewedPlaylistName);
    if (!m_playlistsById.contains(selectedId))
        selectedId = m_playlistsById.cbegin().key();
    m_viewedPlaylistId = selectedId;
    rebuildAlbums();
    emit playlistsChanged();
    emitPlaylistChanges();
    emit viewStateChanged();
}

QMap<QString, QStringList> BgmStore::playlistsAsPaths() const
{
    QMap<QString, QStringList> result;
    for (auto it = m_playlistsById.cbegin(); it != m_playlistsById.cend(); ++it) {
        QStringList paths;
        for (const QString &trackId : it->trackIds) {
            const auto track = m_tracksById.constFind(trackId);
            if (track != m_tracksById.cend())
                paths.append(track->sourceUrl.toLocalFile());
        }
        result.insert(it->name, paths);
    }
    return result;
}

QVariantList BgmStore::bgmPlaylists() const
{
    QVariantList result;
    result.reserve(m_playlistsById.size());
    for (const BgmPlaylist &item : m_playlistsById) {
        result.append(QVariantMap{
            {QStringLiteral("id"), item.id},
            {QStringLiteral("name"), item.name},
            {QStringLiteral("trackIds"), item.trackIds},
            {QStringLiteral("updatedAt"), item.updatedAt.toString(Qt::ISODateWithMs)}
        });
    }
    return result;
}

void BgmStore::setVolume(qreal volume)
{
    const qreal bounded = qBound<qreal>(0.0, volume, 1.0);
    if (qAbs(m_volume - bounded) < 0.001 && m_muted == (bounded <= 0.001))
        return;
    m_volume = bounded;
    if (m_volume > 0.001) {
        m_lastAudibleVolume = m_volume;
        m_muted = false;
    } else {
        m_muted = true;
    }
    m_audioEngine->setVolume(m_volume > 0.001 ? m_volume : m_lastAudibleVolume);
    m_audioEngine->setMuted(m_muted);
    emit volumeChanged();
    emit playbackStateChanged();
}

qreal BgmStore::volume() const
{
    return m_volume;
}

void BgmStore::setRepeatMode(int mode)
{
    const int bounded = qBound(0, mode, 2);
    if (m_repeatMode == bounded)
        return;
    m_repeatMode = bounded;
    emit repeatModeChanged();
    emit playbackStateChanged();
}

int BgmStore::repeatMode() const
{
    return m_repeatMode;
}

void BgmStore::setShuffle(bool enabled)
{
    if (m_shuffle == enabled)
        return;
    m_shuffle = enabled;
    if (m_shuffle && !m_activeQueueIds.isEmpty()) {
        QStringList remaining = m_activeQueueIds;
        const int current = remaining.indexOf(m_activeTrackId);
        if (current >= 0)
            remaining.removeAt(current);
        for (int i = remaining.size() - 1; i > 0; --i) {
            const int j = QRandomGenerator::global()->bounded(i + 1);
            remaining.swapItemsAt(i, j);
        }
        m_activeQueueIds.clear();
        if (!m_activeTrackId.isEmpty())
            m_activeQueueIds.append(m_activeTrackId);
        m_activeQueueIds.append(remaining);
        m_currentIndex = m_activeTrackId.isEmpty() ? -1 : 0;
    } else if (!m_shuffle && !m_originalQueueIds.isEmpty()) {
        m_activeQueueIds = m_originalQueueIds;
        m_currentIndex = m_activeQueueIds.indexOf(m_activeTrackId);
        if (m_currentIndex < 0 && !m_activeQueueIds.isEmpty())
            m_currentIndex = 0;
    }
    emit shuffleChanged();
    emit playbackStateChanged();
}

bool BgmStore::shuffle() const
{
    return m_shuffle;
}

void BgmStore::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    if (muted && m_volume > 0.001)
        m_lastAudibleVolume = m_volume;
    if (!muted && m_volume <= 0.001)
        m_volume = m_lastAudibleVolume > 0.001 ? m_lastAudibleVolume : 0.6;
    m_muted = muted;
    m_audioEngine->setVolume(m_muted ? m_lastAudibleVolume : m_volume);
    m_audioEngine->setMuted(m_muted);
    emit volumeChanged();
    emit playbackStateChanged();
}

bool BgmStore::muted() const
{
    return m_muted;
}

void BgmStore::setTrackArtwork(const QString &path, const QString &artworkUrl, const QString &title)
{
    const QString id = trackIdForPath(path);
    if (id.isEmpty())
        return;
    BgmTrack &track = m_tracksById[id];
    bool changed = false;
    if (!title.trimmed().isEmpty() && track.title != title.trimmed()) {
        track.title = title.trimmed();
        changed = true;
    }
    if (!artworkUrl.isEmpty() && track.artworkUrl.toString() != artworkUrl) {
        track.artworkUrl = QUrl(artworkUrl);
        changed = true;
    }
    if (!changed)
        return;
    rebuildAlbums();
    emit libraryChanged();
    emit playlistTracksChanged();
    emit trackMetadataUpdated(path, track.title, track.artworkUrl.toString());
    if (id == m_activeTrackId)
        emit playbackStateChanged();
}

QString BgmStore::artworkForPath(const QString &path) const
{
    const QString id = trackIdForPath(path);
    return id.isEmpty() ? QString() : m_tracksById.value(id).artworkUrl.toString();
}

void BgmStore::shutdown()
{
    stopBgm();
}

QString BgmStore::bgmPath() const
{
    return m_tracksById.value(m_activeTrackId).sourceUrl.toLocalFile();
}

QString BgmStore::bgmCoverArt() const
{
    return m_tracksById.value(m_activeTrackId).artworkUrl.toString();
}

bool BgmStore::isPlayingBgm() const
{
    return m_status == PlaybackStatus::Playing;
}

QString BgmStore::bgmTrackName() const
{
    const BgmTrack track = m_tracksById.value(m_activeTrackId);
    return track.id.isEmpty() ? QStringLiteral("NO TRACKS") : track.title;
}

int BgmStore::bgmCount() const
{
    return m_activeQueueIds.size();
}

bool BgmStore::bgmShuffle() const
{
    return m_shuffle;
}

int BgmStore::bgmIndex() const
{
    return qMax(0, m_currentIndex);
}

int BgmStore::bgmPositionMs() const
{
    return static_cast<int>(m_positionMs);
}

int BgmStore::bgmDurationMs() const
{
    return static_cast<int>(m_durationMs);
}

qreal BgmStore::bgmVolume() const
{
    return m_volume;
}

bool BgmStore::isMuted() const
{
    return m_muted;
}

int BgmStore::bgmRepeatMode() const
{
    return m_repeatMode;
}

QString BgmStore::status() const
{
    return statusName();
}

QVariantMap BgmStore::activeSource() const
{
    return {
        {QStringLiteral("type"), m_activeSourceType},
        {QStringLiteral("id"), m_activeSourceId.isEmpty() ? QVariant() : QVariant(m_activeSourceId)}
    };
}

QStringList BgmStore::originalQueueIds() const
{
    return m_originalQueueIds;
}

QStringList BgmStore::activeQueueIds() const
{
    return m_activeQueueIds;
}

QString BgmStore::activeTrackId() const
{
    return m_activeTrackId;
}

QVariantMap BgmStore::playbackState() const
{
    return {
        {QStringLiteral("activeSource"), activeSource()},
        {QStringLiteral("originalQueueIds"), m_originalQueueIds},
        {QStringLiteral("activeQueueIds"), m_activeQueueIds},
        {QStringLiteral("currentIndex"), m_currentIndex},
        {QStringLiteral("activeTrackId"), m_activeTrackId},
        {QStringLiteral("status"), statusName()},
        {QStringLiteral("positionSec"), m_positionMs / 1000.0},
        {QStringLiteral("durationSec"), m_durationMs / 1000.0},
        {QStringLiteral("volume"), m_volume},
        {QStringLiteral("isMuted"), m_muted},
        {QStringLiteral("isShuffling"), m_shuffle},
        {QStringLiteral("repeatMode"), m_repeatMode == 0 ? QStringLiteral("OFF")
                                      : m_repeatMode == 1 ? QStringLiteral("ALL")
                                                          : QStringLiteral("ONE")}
    };
}

QVariantMap BgmStore::viewState() const
{
    return {
        {QStringLiteral("activeTab"), m_activeTab},
        {QStringLiteral("viewedAlbumId"), m_viewedAlbumId},
        {QStringLiteral("viewedPlaylistId"), m_viewedPlaylistId}
    };
}

QString BgmStore::activeTab() const
{
    return m_activeTab;
}

QString BgmStore::viewedAlbumId() const
{
    return m_viewedAlbumId;
}

QString BgmStore::viewedPlaylistId() const
{
    return m_viewedPlaylistId;
}

QStringList BgmStore::libraryTrackIds() const
{
    QStringList ids;
    QSet<QString> seen;
    const auto append = [&ids, &seen](const QStringList &source) {
        for (const QString &id : source) {
            if (!id.isEmpty() && !seen.contains(id)) {
                seen.insert(id);
                ids.append(id);
            }
        }
    };
    append(m_localLibraryTrackIds);
    append(m_importedLibraryTrackIds);
    return ids;
}

QVariantMap BgmStore::trackMap(const QString &trackId, int index, bool inPlaylist) const
{
    const BgmTrack track = m_tracksById.value(trackId);
    if (track.id.isEmpty())
        return {};
    const QString path = track.sourceUrl.toLocalFile();
    const QString searchText = (track.title + QLatin1Char(' ') + track.artistName
                                + QLatin1Char(' ') + track.albumTitle + QLatin1Char(' ')
                                + path).toLower();
    return {
        {QStringLiteral("trackId"), track.id},
        {QStringLiteral("id"), track.id},
        {QStringLiteral("index"), index},
        {QStringLiteral("name"), track.title},
        {QStringLiteral("title"), track.title},
        {QStringLiteral("path"), path},
        {QStringLiteral("sourceUrl"), track.sourceUrl.toString()},
        {QStringLiteral("thumbnailPath"), track.artworkUrl.toString()},
        {QStringLiteral("artworkUrl"), track.artworkUrl.toString()},
        {QStringLiteral("artistIds"), track.artistIds},
        {QStringLiteral("artist"), track.artistName},
        {QStringLiteral("albumId"), track.albumId},
        {QStringLiteral("album"), track.albumTitle},
        {QStringLiteral("discNumber"), track.discNumber},
        {QStringLiteral("trackNumber"), track.trackNumber},
        {QStringLiteral("durationSec"), track.durationSec},
        {QStringLiteral("inPlaylist"), inPlaylist},
        {QStringLiteral("searchText"), searchText},
        {QStringLiteral("normalizedPath"), path.toCaseFolded()}
    };
}

QVariantList BgmStore::trackList(const QStringList &trackIds, bool inPlaylist) const
{
    QVariantList result;
    result.reserve(trackIds.size());
    for (int i = 0; i < trackIds.size(); ++i)
        result.append(trackMap(trackIds.at(i), i, inPlaylist));
    return result;
}

QVariantList BgmStore::bgmLibraryTracks() const
{
    const QStringList ids = libraryTrackIds();
    const BgmPlaylist *current = playlist(m_viewedPlaylistId);
    const QStringList viewedTrackIds = current ? current->trackIds : QStringList();
    const QSet<QString> playlistTrackIds(viewedTrackIds.cbegin(), viewedTrackIds.cend());
    QVariantList result;
    result.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i)
        result.append(trackMap(ids.at(i), i, playlistTrackIds.contains(ids.at(i))));
    return result;
}

QVariantList BgmStore::bgmPlaylistTracks() const
{
    const BgmPlaylist *current = playlist(m_viewedPlaylistId);
    return current ? trackList(current->trackIds, true) : QVariantList();
}

QVariantList BgmStore::bgmAlbumTracks() const
{
    const auto album = m_albumsById.constFind(m_viewedAlbumId);
    return album == m_albumsById.cend() ? QVariantList() : trackList(album->trackIds, false);
}

QVariantList BgmStore::bgmAlbums() const
{
    QVariantList result;
    result.reserve(m_albumOrder.size());
    for (const QString &id : m_albumOrder) {
        const BgmAlbum album = m_albumsById.value(id);
        result.append(QVariantMap{
            {QStringLiteral("id"), album.id},
            {QStringLiteral("title"), album.title},
            {QStringLiteral("artistId"), album.artistId},
            {QStringLiteral("artist"), album.artistName},
            {QStringLiteral("coverUrl"), album.coverUrl},
            {QStringLiteral("year"), album.year},
            {QStringLiteral("trackIds"), album.trackIds}
        });
    }
    return result;
}

QStringList BgmStore::bgmPlaylistNames() const
{
    QStringList names;
    names.reserve(m_playlistsById.size());
    for (const BgmPlaylist &item : m_playlistsById)
        names.append(item.name);
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString BgmStore::currentBgmPlaylistName() const
{
    const BgmPlaylist *current = playlist(m_viewedPlaylistId);
    return current ? current->name : QString();
}

void BgmStore::setActiveTab(const QString &tab)
{
    const QString normalized = tab.trimmed().toUpper();
    if (normalized != QStringLiteral("LIBRARY")
        && normalized != QStringLiteral("ALBUMS")
        && normalized != QStringLiteral("PLAYLISTS")) {
        qWarning() << "BgmStore: ignoring unknown BGM tab" << tab;
        return;
    }
    if (m_activeTab == normalized)
        return;
    m_activeTab = normalized;
    emit viewStateChanged();
}

void BgmStore::viewAlbums()
{
    const bool changed = m_activeTab != QStringLiteral("ALBUMS")
        || !m_viewedAlbumId.isEmpty();
    m_activeTab = QStringLiteral("ALBUMS");
    m_viewedAlbumId.clear();
    if (changed)
        emit viewStateChanged();
}

BgmPlaylist *BgmStore::playlist(const QString &playlistId)
{
    auto it = m_playlistsById.find(playlistId);
    return it == m_playlistsById.end() ? nullptr : &it.value();
}

const BgmPlaylist *BgmStore::playlist(const QString &playlistId) const
{
    const auto it = m_playlistsById.constFind(playlistId);
    return it == m_playlistsById.cend() ? nullptr : &it.value();
}

QString BgmStore::playlistIdForName(const QString &name) const
{
    return m_playlistIdByName.value(name.trimmed().toCaseFolded());
}

void BgmStore::viewPlaylist(const QString &playlistIdOrName)
{
    QString id = m_playlistsById.contains(playlistIdOrName)
        ? playlistIdOrName
        : playlistIdForName(playlistIdOrName);
    if (id.isEmpty()) {
        qWarning() << "BgmStore: cannot view unknown playlist" << playlistIdOrName;
        return;
    }
    const bool changed = m_activeTab != QStringLiteral("PLAYLISTS")
        || m_viewedPlaylistId != id;
    m_activeTab = QStringLiteral("PLAYLISTS");
    m_viewedPlaylistId = id;
    if (changed) {
        emit viewStateChanged();
        emit playlistTracksChanged();
        emit libraryChanged();
    }
}

void BgmStore::viewAlbum(const QString &albumId)
{
    if (!m_albumsById.contains(albumId)) {
        qWarning() << "BgmStore: cannot view unknown album" << albumId;
        return;
    }
    const bool changed = m_activeTab != QStringLiteral("ALBUMS")
        || m_viewedAlbumId != albumId;
    m_activeTab = QStringLiteral("ALBUMS");
    m_viewedAlbumId = albumId;
    if (changed)
        emit viewStateChanged();
}

void BgmStore::selectBgmPlaylist(const QString &name)
{
    viewPlaylist(name);
}

bool BgmStore::createBgmPlaylist(const QString &name)
{
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        qWarning() << "BgmStore: refusing to create an empty playlist name";
        return false;
    }
    if (!playlistIdForName(normalized).isEmpty()) {
        qWarning() << "BgmStore: playlist already exists" << normalized;
        return false;
    }
    BgmPlaylist saved;
    saved.id = stablePlaylistIdForName(normalized);
    saved.name = normalized;
    saved.updatedAt = QDateTime::currentDateTimeUtc();
    m_playlistIdByName.insert(normalized.toCaseFolded(), saved.id);
    m_playlistsById.insert(saved.id, saved);
    viewPlaylist(saved.id);
    emit playlistsChanged();
    emitPlaylistChanges();
    return true;
}

void BgmStore::addPathsToLibrary(const QStringList &paths, bool addToViewedPlaylist)
{
    static const QStringList audioExtensions = {
        QStringLiteral("mp3"), QStringLiteral("m4a"), QStringLiteral("wav"),
        QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("aac"),
        QStringLiteral("wma")
    };
    QStringList addedIds;
    QStringList foundIds;
    const QStringList existingLibraryIds = libraryTrackIds();
    QSet<QString> seen(existingLibraryIds.cbegin(), existingLibraryIds.cend());
    QSet<QString> found;
    const auto appendFile = [this, &addedIds, &foundIds, &seen, &found](const QString &path) {
        const QFileInfo info(path);
        if (!info.isFile() || !audioExtensions.contains(info.suffix().toLower()))
            return;
        const QString id = ensureTrack(info.absoluteFilePath());
        if (!id.isEmpty()) {
            if (!found.contains(id)) {
                found.insert(id);
                foundIds.append(id);
            }
            if (!seen.contains(id)) {
                seen.insert(id);
                addedIds.append(id);
            }
        }
    };

    for (const QString &rawPath : paths) {
        const QString localPath = rawPath.startsWith(QStringLiteral("file:"))
            ? QUrl(rawPath).toLocalFile()
            : rawPath;
        const QFileInfo info(localPath);
        if (info.isDir()) {
            QDirIterator it(localPath, {"*.mp3", "*.m4a", "*.wav", "*.flac", "*.ogg", "*.aac", "*.wma"},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
                appendFile(it.next());
        } else {
            appendFile(localPath);
        }
    }

    if (!addedIds.isEmpty())
        m_localLibraryTrackIds.append(addedIds);
    bool playlistChanged = false;
    BgmPlaylist *current = playlist(m_viewedPlaylistId);
    if (addToViewedPlaylist && current) {
        for (const QString &id : foundIds) {
            if (!current->trackIds.contains(id)) {
                current->trackIds.append(id);
                playlistChanged = true;
            }
        }
        if (playlistChanged)
            updatePlaylistTimestamp(*current);
    }
    if (addedIds.isEmpty())
        if (!playlistChanged)
            return;
    if (!addedIds.isEmpty()) {
        rebuildAlbums();
        emit libraryChanged();
    }
    if (playlistChanged)
        emitPlaylistChanges();
}

void BgmStore::addBgmTracksToPlaylist(const QStringList &paths)
{
    addPathsToLibrary(paths, true);
}

void BgmStore::removeBgmTrack(int index)
{
    removeTrackFromPlaylist(m_viewedPlaylistId, index);
}

void BgmStore::clearBgmPlaylist()
{
    BgmPlaylist *current = playlist(m_viewedPlaylistId);
    if (!current || current->trackIds.isEmpty())
        return;
    current->trackIds.clear();
    updatePlaylistTimestamp(*current);
    emitPlaylistChanges();
    emit libraryChanged();
}

void BgmStore::addTrackToPlaylist(const QString &playlistId, const QString &trackId)
{
    BgmPlaylist *target = playlist(playlistId);
    if (!target || !m_tracksById.contains(trackId)) {
        qWarning() << "BgmStore: cannot add unknown track or playlist" << trackId << playlistId;
        return;
    }
    target->trackIds.append(trackId);
    updatePlaylistTimestamp(*target);
    emitPlaylistChanges();
}

void BgmStore::removeTrackFromPlaylist(const QString &playlistId, int index)
{
    BgmPlaylist *target = playlist(playlistId);
    if (!target || index < 0 || index >= target->trackIds.size())
        return;
    target->trackIds.removeAt(index);
    updatePlaylistTimestamp(*target);
    emitPlaylistChanges();
    emit libraryChanged();
}

void BgmStore::reorderPlaylist(const QString &playlistId, int fromIndex, int toIndex)
{
    BgmPlaylist *target = playlist(playlistId.isEmpty() ? m_viewedPlaylistId : playlistId);
    if (!target || fromIndex < 0 || fromIndex >= target->trackIds.size()
        || toIndex < 0 || toIndex >= target->trackIds.size() || fromIndex == toIndex) {
        return;
    }
    target->trackIds.move(fromIndex, toIndex);
    updatePlaylistTimestamp(*target);
    emitPlaylistChanges();
}

QStringList BgmStore::validTrackIds(const QStringList &trackIds) const
{
    QStringList valid;
    for (const QString &id : trackIds) {
        const auto track = m_tracksById.constFind(id);
        if (track == m_tracksById.cend())
            continue;
        if (track->sourceUrl.isLocalFile() && !QFileInfo(track->sourceUrl.toLocalFile()).isFile()) {
            qWarning() << "BgmStore: skipping missing track" << track->sourceUrl.toLocalFile();
            continue;
        }
        valid.append(id);
    }
    return valid;
}

void BgmStore::playFromSource(const QString &type, const QString &id,
                              const QStringList &trackIds, int startIndex)
{
    const QString sourceType = type.trimmed().toUpper();
    if (sourceType != QStringLiteral("LIBRARY")
        && sourceType != QStringLiteral("ALBUM")
        && sourceType != QStringLiteral("PLAYLIST")) {
        qWarning() << "BgmStore: invalid playback source" << type;
        return;
    }
    m_activeSourceType = sourceType;
    m_activeSourceId = id;
    m_originalQueueIds = validTrackIds(trackIds);
    m_activeQueueIds = m_originalQueueIds;
    const int validBeforeStart = startIndex > 0 && startIndex < trackIds.size()
        ? validTrackIds(trackIds.mid(0, startIndex)).size()
        : 0;
    m_currentIndex = startIndex >= 0 && startIndex < trackIds.size()
        && validBeforeStart < m_activeQueueIds.size()
        && m_activeQueueIds.at(validBeforeStart) == trackIds.at(startIndex)
        ? validBeforeStart
        : -1;
    if (m_currentIndex < 0 && !m_activeQueueIds.isEmpty())
        m_currentIndex = 0;
    if (m_shuffle && m_currentIndex >= 0) {
        const QString currentId = m_activeQueueIds.at(m_currentIndex);
        QStringList remaining = m_activeQueueIds;
        remaining.removeAt(m_currentIndex);
        for (int i = remaining.size() - 1; i > 0; --i) {
            const int j = QRandomGenerator::global()->bounded(i + 1);
            remaining.swapItemsAt(i, j);
        }
        m_activeQueueIds = {currentId};
        m_activeQueueIds.append(remaining);
        m_currentIndex = 0;
    }
    if (m_activeQueueIds.isEmpty()) {
        if (!trackIds.isEmpty())
            emit playbackError(QStringLiteral("No available tracks remain in the selected source."));
        ++m_loadToken;
        m_audioEngine->clear();
        m_activeTrackId.clear();
        m_currentIndex = -1;
        m_positionMs = 0;
        m_durationMs = 0;
        setStatus(PlaybackStatus::Idle);
        emit positionChanged();
        emit durationChanged();
        emit playbackStateChanged();
        return;
    }
    m_activeTrackId = m_activeQueueIds.at(m_currentIndex);
    loadCurrentTrack();
    emit playbackStateChanged();
}

void BgmStore::playBgmTrack(int index)
{
    const BgmPlaylist *current = playlist(m_viewedPlaylistId);
    if (!current || index < 0 || index >= current->trackIds.size())
        return;
    playFromSource(QStringLiteral("PLAYLIST"), current->id, current->trackIds, index);
}

void BgmStore::playBgmAlbumTrack(int index)
{
    const auto current = m_albumsById.constFind(m_viewedAlbumId);
    if (current == m_albumsById.cend() || index < 0 || index >= current->trackIds.size())
        return;
    playFromSource(QStringLiteral("ALBUM"), current->id, current->trackIds, index);
}

void BgmStore::playBgmFile(const QString &path)
{
    QString id = trackIdForPath(path);
    if (id.isEmpty()) {
        id = ensureTrack(path);
        if (!id.isEmpty())
            m_localLibraryTrackIds.append(id);
    }
    if (id.isEmpty()) {
        qWarning() << "BgmStore: cannot play missing or unsupported file" << path;
        return;
    }
    QStringList queue = libraryTrackIds();
    int startIndex = queue.indexOf(id);
    if (startIndex < 0) {
        queue.append(id);
        startIndex = queue.size() - 1;
    }
    playFromSource(QStringLiteral("LIBRARY"), {}, queue, startIndex);
}

void BgmStore::toggleBgmPlayback()
{
    if (m_status == PlaybackStatus::Playing) {
        m_audioEngine->pause();
        setStatus(PlaybackStatus::Paused);
        return;
    }
    if (m_status == PlaybackStatus::Loading) {
        m_audioEngine->pause();
        setStatus(PlaybackStatus::Paused);
        return;
    }
    if (!m_activeQueueIds.isEmpty() && !m_activeTrackId.isEmpty()) {
        if (m_status == PlaybackStatus::Idle || m_status == PlaybackStatus::Ended
            || m_status == PlaybackStatus::Error) {
            m_currentIndex = qMax(0, m_activeQueueIds.indexOf(m_activeTrackId));
            loadCurrentTrack();
        } else {
            m_audioEngine->play();
        }
        return;
    }
    if (!m_activeQueueIds.isEmpty()) {
        m_currentIndex = 0;
        m_activeTrackId = m_activeQueueIds.first();
        loadCurrentTrack();
        return;
    }

    const BgmPlaylist *current = playlist(m_viewedPlaylistId);
    if (current && !current->trackIds.isEmpty()) {
        playFromSource(QStringLiteral("PLAYLIST"), current->id, current->trackIds, 0);
        return;
    }
    const QStringList ids = validTrackIds(libraryTrackIds());
    if (!ids.isEmpty()) {
        playFromSource(QStringLiteral("LIBRARY"), {}, ids, 0);
        return;
    }
    setStatus(PlaybackStatus::Idle);
}

void BgmStore::nextBgm()
{
    if (m_activeQueueIds.isEmpty()) {
        toggleBgmPlayback();
        return;
    }
    int next = m_currentIndex < 0 ? 0 : m_currentIndex + 1;
    if (next >= m_activeQueueIds.size()) {
        if (m_repeatMode != 1) {
            setStatus(PlaybackStatus::Ended);
            return;
        }
        next = 0;
    }
    advanceToIndex(next, true);
}

void BgmStore::backBgm()
{
    if (m_activeQueueIds.isEmpty())
        return;
    if (m_positionMs > 3000) {
        if (m_status == PlaybackStatus::Ended || m_status == PlaybackStatus::Error
            || m_status == PlaybackStatus::Idle) {
            loadCurrentTrack();
            return;
        }
        m_audioEngine->seek(0);
        m_positionMs = 0;
        emit positionChanged();
        emit playbackStateChanged();
        return;
    }
    int previous = m_currentIndex - 1;
    if (previous < 0) {
        if (m_repeatMode == 1)
            previous = m_activeQueueIds.size() - 1;
        else
            previous = m_currentIndex;
    }
    advanceToIndex(previous, true);
}

void BgmStore::stopBgm()
{
    ++m_loadToken;
    if (m_fadeAnimation) {
        m_fadeAnimation->stop();
        m_fadeAnimation->deleteLater();
        m_fadeAnimation = nullptr;
    }
    m_audioEngine->stop();
    m_positionMs = 0;
    setStatus(PlaybackStatus::Idle);
    emit positionChanged();
    emit playbackStateChanged();
}

void BgmStore::fadeOutBgm(int durationMs)
{
    if (!m_audioEngine->isPlaying()) {
        stopBgm();
        return;
    }
    if (m_fadeAnimation) {
        m_fadeAnimation->stop();
        m_fadeAnimation->deleteLater();
    }
    const qreal startVolume = m_volume;
    m_fadeAnimation = new QVariantAnimation(this);
    m_fadeAnimation->setStartValue(startVolume);
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->setDuration(qMax(300, durationMs));
    connect(m_fadeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_audioEngine->setVolume(value.toReal());
    });
    connect(m_fadeAnimation, &QVariantAnimation::finished, this, [this, startVolume]() {
        stopBgm();
        m_audioEngine->setVolume(m_muted ? m_lastAudibleVolume : startVolume);
        m_fadeAnimation = nullptr;
    });
    m_fadeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void BgmStore::toggleBgmShuffle()
{
    setShuffle(!m_shuffle);
}

void BgmStore::cycleBgmRepeatMode()
{
    setRepeatMode((m_repeatMode + 1) % 3);
}

void BgmStore::seekBgm(int positionMs)
{
    if (m_activeTrackId.isEmpty())
        return;
    m_positionMs = qBound<qint64>(0, static_cast<qint64>(positionMs), m_durationMs);
    m_audioEngine->seek(m_positionMs);
    emit positionChanged();
    emit playbackStateChanged();
}

void BgmStore::beginBgmSeek()
{
    m_audioEngine->beginSeeking();
}

void BgmStore::endBgmSeek()
{
    m_audioEngine->endSeeking();
}

void BgmStore::toggleMute()
{
    setMuted(!m_muted);
}

void BgmStore::addToQueue(const QString &trackId)
{
    if (!m_tracksById.contains(trackId)) {
        qWarning() << "BgmStore: cannot queue unknown track" << trackId;
        return;
    }
    m_activeQueueIds.append(trackId);
    if (m_currentIndex < 0)
        m_currentIndex = 0;
    emit playbackStateChanged();
}

void BgmStore::playNext(const QString &trackId)
{
    if (!m_tracksById.contains(trackId)) {
        qWarning() << "BgmStore: cannot play next unknown track" << trackId;
        return;
    }
    const int insertAt = qBound(0, m_currentIndex + 1, m_activeQueueIds.size());
    m_activeQueueIds.insert(insertAt, trackId);
    if (m_currentIndex < 0)
        m_currentIndex = 0;
    emit playbackStateChanged();
}

void BgmStore::rebuildAlbums()
{
    QHash<QString, BgmAlbum> rebuilt;
    for (const QString &trackId : libraryTrackIds()) {
        auto track = m_tracksById.find(trackId);
        if (track == m_tracksById.end())
            continue;
        if (track->albumId.isEmpty()) {
            const QString artistName = track->artistName.isEmpty()
                ? QStringLiteral("Unknown Artist")
                : track->artistName;
            const QString artistId = artistIdForName(artistName);
            if (track->albumTitle.isEmpty())
                track->albumTitle = QStringLiteral("Unknown Album");
            track->artistIds = {artistId};
            track->albumId = albumIdForName(artistId, track->albumTitle);
        }
        BgmAlbum &album = rebuilt[track->albumId];
        album.id = track->albumId;
        album.title = track->albumTitle.isEmpty() ? QStringLiteral("Unknown Album") : track->albumTitle;
        album.artistName = track->artistName.isEmpty() ? QStringLiteral("Unknown Artist") : track->artistName;
        album.artistId = track->artistIds.isEmpty() ? artistIdForName(album.artistName) : track->artistIds.first();
        if (album.coverUrl.isEmpty() && !track->artworkUrl.isEmpty())
            album.coverUrl = track->artworkUrl.toString();
        if (album.year == 0)
            album.year = track->year;
        album.trackIds.append(trackId);
    }

    for (auto album = rebuilt.begin(); album != rebuilt.end(); ++album) {
        std::sort(album->trackIds.begin(), album->trackIds.end(), [this](const QString &leftId, const QString &rightId) {
            const BgmTrack left = m_tracksById.value(leftId);
            const BgmTrack right = m_tracksById.value(rightId);
            if (left.discNumber != right.discNumber)
                return left.discNumber < right.discNumber;
            if (left.trackNumber != right.trackNumber)
                return left.trackNumber < right.trackNumber;
            return left.title.compare(right.title, Qt::CaseInsensitive) < 0;
        });
    }

    m_albumsById = rebuilt;
    m_albumOrder = m_albumsById.keys();
    std::sort(m_albumOrder.begin(), m_albumOrder.end(), [this](const QString &leftId, const QString &rightId) {
        const BgmAlbum left = m_albumsById.value(leftId);
        const BgmAlbum right = m_albumsById.value(rightId);
        const int titleOrder = left.title.compare(right.title, Qt::CaseInsensitive);
        return titleOrder == 0
            ? left.artistName.compare(right.artistName, Qt::CaseInsensitive) < 0
            : titleOrder < 0;
    });
    if (!m_viewedAlbumId.isEmpty() && !m_albumsById.contains(m_viewedAlbumId))
        m_viewedAlbumId.clear();
}

void BgmStore::emitPlaylistChanges()
{
    emit playlistsChanged();
    emit playlistTracksChanged();
    emit viewStateChanged();
}

void BgmStore::setStatus(PlaybackStatus status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit playbackStateChanged();
}

QString BgmStore::statusName() const
{
    switch (m_status) {
    case PlaybackStatus::Idle: return QStringLiteral("IDLE");
    case PlaybackStatus::Loading: return QStringLiteral("LOADING");
    case PlaybackStatus::Ready: return QStringLiteral("READY");
    case PlaybackStatus::Playing: return QStringLiteral("PLAYING");
    case PlaybackStatus::Paused: return QStringLiteral("PAUSED");
    case PlaybackStatus::Ended: return QStringLiteral("ENDED");
    case PlaybackStatus::Error: return QStringLiteral("ERROR");
    }
    return QStringLiteral("ERROR");
}

void BgmStore::loadCurrentTrack()
{
    int attempts = m_activeQueueIds.size();
    while (attempts-- > 0) {
        if (m_currentIndex < 0 || m_currentIndex >= m_activeQueueIds.size())
            m_currentIndex = 0;
        const QString trackId = m_activeQueueIds.value(m_currentIndex);
        const auto track = m_tracksById.constFind(trackId);
        if (track != m_tracksById.cend()
            && (!track->sourceUrl.isLocalFile() || QFileInfo(track->sourceUrl.toLocalFile()).isFile())) {
            m_activeTrackId = trackId;
            m_positionMs = 0;
            m_durationMs = track->durationSec * 1000;
            ++m_loadToken;
            setStatus(PlaybackStatus::Loading);
            emit positionChanged();
            emit durationChanged();
            m_audioEngine->load(track->sourceUrl, m_loadToken, true);
            emit playbackStateChanged();
            return;
        }
        qWarning() << "BgmStore: removing unavailable track from active queue" << trackId;
        m_activeQueueIds.removeAt(m_currentIndex);
        m_originalQueueIds.removeAll(trackId);
        if (m_activeQueueIds.isEmpty())
            break;
        if (m_currentIndex >= m_activeQueueIds.size()) {
            if (m_repeatMode == 1)
                m_currentIndex = 0;
            else
                break;
        }
    }
    m_activeTrackId.clear();
    m_currentIndex = -1;
    m_positionMs = 0;
    m_durationMs = 0;
    m_audioEngine->clear();
    setStatus(PlaybackStatus::Error);
    emit positionChanged();
    emit durationChanged();
    emit playbackError(QStringLiteral("No available tracks remain in the playback queue."));
}

void BgmStore::handleLoadFinished(quint64 loadToken, bool success, const QString &error)
{
    if (loadToken != m_loadToken)
        return;
    if (!success) {
        qWarning() << "BgmStore: track load failed" << bgmPath() << error;
        emit playbackError(error);
        skipFailedTrack(m_activeTrackId);
        return;
    }
    m_durationMs = m_audioEngine->durationMs();
    if (m_status != PlaybackStatus::Paused)
        setStatus(PlaybackStatus::Ready);
    emit durationChanged();
    emit playbackStateChanged();
}

void BgmStore::handleMediaEnded(quint64 loadToken)
{
    if (loadToken != m_loadToken || m_activeQueueIds.isEmpty())
        return;
    if (m_repeatMode == 2) {
        m_audioEngine->seek(0);
        m_positionMs = 0;
        m_audioEngine->play();
        emit positionChanged();
        return;
    }
    const int next = m_currentIndex + 1;
    if (next < m_activeQueueIds.size()) {
        advanceToIndex(next, true);
    } else if (m_repeatMode == 1) {
        advanceToIndex(0, true);
    } else {
        m_positionMs = m_durationMs;
        setStatus(PlaybackStatus::Ended);
        emit positionChanged();
    }
}

void BgmStore::handleMetadata(const QVariantMap &metadata)
{
    auto track = m_tracksById.find(m_activeTrackId);
    if (track == m_tracksById.end())
        return;

    bool changed = false;
    const QString title = metadata.value(QStringLiteral("title")).toString().trimmed();
    const QString albumName = metadata.value(QStringLiteral("albumTitle")).toString().trimmed();
    const QString albumArtist = metadata.value(QStringLiteral("albumArtist")).toString().trimmed();
    QStringList artistNames = stringListFromVariant(metadata.value(QStringLiteral("artists")));
    if (artistNames.isEmpty())
        artistNames = stringListFromVariant(metadata.value(QStringLiteral("leadPerformer")));
    QStringList artistIds;
    for (const QString &rawArtistName : artistNames) {
        const QString artist = rawArtistName.trimmed();
        if (!artist.isEmpty()) {
            const QString artistId = artistIdForName(artist);
            if (!artistIds.contains(artistId))
                artistIds.append(artistId);
        }
    }
    const QString artistName = albumArtist.isEmpty()
        ? artistNames.value(0).trimmed()
        : albumArtist;
    if (!albumArtist.isEmpty()) {
        const QString albumArtistId = artistIdForName(albumArtist);
        if (!artistIds.contains(albumArtistId))
            artistIds.prepend(albumArtistId);
    }
    const int trackNumber = metadata.value(QStringLiteral("trackNumber")).toInt();
    const int year = metadata.value(QStringLiteral("year")).toInt();
    if (!title.isEmpty() && track->title != title) {
        track->title = title;
        changed = true;
    }
    if (!artistName.isEmpty() && track->artistName != artistName) {
        track->artistName = artistName;
        changed = true;
    }
    if (!artistIds.isEmpty() && track->artistIds != artistIds) {
        track->artistIds = artistIds;
        changed = true;
    }
    if (!albumName.isEmpty() && track->albumTitle != albumName) {
        track->albumTitle = albumName;
        changed = true;
    }
    if (trackNumber > 0 && track->trackNumber != trackNumber) {
        track->trackNumber = trackNumber;
        changed = true;
    }
    if (year > 0 && track->year != year) {
        track->year = year;
        changed = true;
    }
    if (changed) {
        track->albumId = albumIdForName(
            artistIdForName(track->artistName), track->albumTitle);
        rebuildAlbums();
        emit libraryChanged();
        emit playlistTracksChanged();
        emit trackMetadataUpdated(track->sourceUrl.toLocalFile(), track->title,
                                   track->artworkUrl.toString());
        emit playbackStateChanged();
    }
}

void BgmStore::advanceToIndex(int index, bool autoplay)
{
    if (m_activeQueueIds.isEmpty() || index < 0 || index >= m_activeQueueIds.size())
        return;
    m_currentIndex = index;
    m_activeTrackId = m_activeQueueIds.at(index);
    if (!autoplay) {
        setStatus(PlaybackStatus::Ready);
        emit playbackStateChanged();
        return;
    }
    loadCurrentTrack();
}

void BgmStore::skipFailedTrack(const QString &failedTrackId)
{
    int nextIndex = qMax(0, m_currentIndex);
    if (!failedTrackId.isEmpty()) {
        m_activeQueueIds.removeAll(failedTrackId);
        m_originalQueueIds.removeAll(failedTrackId);
    }
    if (m_activeQueueIds.isEmpty()) {
        m_activeTrackId.clear();
        m_currentIndex = -1;
        m_positionMs = 0;
        m_durationMs = 0;
        setStatus(PlaybackStatus::Error);
        emit positionChanged();
        emit durationChanged();
        return;
    }
    if (nextIndex >= m_activeQueueIds.size()) {
        if (m_repeatMode == 1)
            nextIndex = 0;
        else {
            m_activeTrackId.clear();
            m_currentIndex = -1;
            m_positionMs = 0;
            setStatus(PlaybackStatus::Error);
            emit positionChanged();
            emit durationChanged();
            emit playbackStateChanged();
            return;
        }
    }
    m_currentIndex = nextIndex;
    m_activeTrackId = m_activeQueueIds.at(m_currentIndex);
    loadCurrentTrack();
}

void BgmStore::updatePlaylistTimestamp(BgmPlaylist &target)
{
    target.updatedAt = QDateTime::currentDateTimeUtc();
}
