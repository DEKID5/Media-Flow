#include "BroadcastController.h"
#include "MediaExtractor.h"
#include "MediaThumbnailManager.h"
#include "MediaLibraryProxyModel.h"
#include "SongSearchUtils.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QQuickWindow>
#include <QScreen>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QDebug>
#include <QUuid>
#include <QBuffer>
#include <QCameraDevice>
#include <QTimer>
#include <QImageReader>
#include <QStandardPaths>
#include <QDirIterator>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>

namespace {

QString normalizedMediaPath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath().toLower();
}

QString songIndexKey(const QString &languageCode, int songNumber, const QString &type = {}, const QString &track = {})
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(SongSearchUtils::normalizeLanguageCode(languageCode),
             QString::number(songNumber),
             type.isEmpty() ? QStringLiteral("*") : type.toLower(),
             track.isEmpty() ? QStringLiteral("*") : track.toLower());
}

QVariantMap parseSongMetadata(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString fileName = fi.fileName();

    static const QRegularExpression jwSongRegex(
        QStringLiteral("^sjj([mc])_([A-Z]+)_(\\d{1,3})(?:_r(\\d+)P)?"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = jwSongRegex.match(fileName);

    QVariantMap metadata;
    if (match.hasMatch()) {
        metadata.insert(QStringLiteral("isSong"), true);
        metadata.insert(QStringLiteral("trackType"),
                        match.captured(1).compare(QStringLiteral("m"), Qt::CaseInsensitive) == 0
                            ? QStringLiteral("vocal")
                            : QStringLiteral("instrumental"));
        metadata.insert(QStringLiteral("languageCode"), SongSearchUtils::normalizeLanguageCode(match.captured(2)));
        metadata.insert(QStringLiteral("songNumber"), match.captured(3).toInt());
        if (!match.captured(4).isEmpty())
            metadata.insert(QStringLiteral("resolution"), match.captured(4).toInt());
        return metadata;
    }

    static const QRegularExpression altSongRegex(
        QStringLiteral("^(?:snnw|snn|sjj)_([A-Z]+)_(\\d{1,3})"),
        QRegularExpression::CaseInsensitiveOption);
    match = altSongRegex.match(fileName);
    if (match.hasMatch()) {
        metadata.insert(QStringLiteral("isSong"), true);
        metadata.insert(QStringLiteral("trackType"), QStringLiteral("vocal"));
        metadata.insert(QStringLiteral("languageCode"), SongSearchUtils::normalizeLanguageCode(match.captured(1)));
        metadata.insert(QStringLiteral("songNumber"), match.captured(2).toInt());
    }

    return metadata;
}

int mediaRank(const QVariantMap &asset)
{
    int rank = 0;
    if (asset.value(QStringLiteral("type")).toString() == QStringLiteral("video"))
        rank += 1000;
    else if (asset.value(QStringLiteral("type")).toString() == QStringLiteral("audio"))
        rank += 500;

    if (asset.value(QStringLiteral("trackType")).toString() == QStringLiteral("vocal"))
        rank += 100;

    rank += asset.value(QStringLiteral("resolution")).toInt();
    return rank;
}

} // namespace

// ──────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ──────────────────────────────────────────────────────────────────

BroadcastController::BroadcastController(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_libraryModel(new MediaLibraryModel(this))
    , m_proxyModel(new MediaLibraryProxyModel(this))
    , m_filterProxy(new StagedMediaProxyModel(this))
    , m_meetingModel(new MeetingScheduleModel(this))
    , m_cameraModel(new CameraDeviceModel(this))
    , m_broadcastEngine(new BroadcastEngine(this))
    , m_vcamManager(new VirtualCameraManager(this))
{
    m_programCameraDevice = QMediaDevices::defaultVideoInput();
    m_proxyModel->setSourceModel(m_libraryModel);
    m_filterProxy->setSourceModel(m_libraryModel);

    // ── Screen Monitoring ──
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &BroadcastController::updateScreenCount);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &BroadcastController::updateScreenCount);

    // ── Relay Engine signals ──
    connect(m_broadcastEngine, &BroadcastEngine::isProgramPausedChanged,
            this, &BroadcastController::isProgramPausedChanged);

    // ── Sync volume to engine ──
    connect(this, &BroadcastController::masterVolumeChanged, this, [this]() {
        m_broadcastEngine->setProgramVolume(m_masterVolume / 100.0);
    });
    connect(this, &BroadcastController::mixerMutedChanged, this, [this]() {
        m_broadcastEngine->setProgramMuted(m_mixerMuted);
    });

    registerCameras();

    // ── BGM Player ──
    m_bgmAudio = new QAudioOutput(this);
    m_bgmAudio->setVolume(0.6);
    m_bgmPlayer = new QMediaPlayer(this);
    m_bgmPlayer->setAudioOutput(m_bgmAudio);
    connect(m_bgmPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &BroadcastController::onBgmStatusChanged);

    // ── Media Thumbnail Manager (Async/Hash-cached) ──
    m_thumbManager = new MediaThumbnailManager(this);
    connect(m_thumbManager, &MediaThumbnailManager::thumbnailReady, this, [this](const QString &id, const QString &cachePath, const QString &title) {
        if (id == QStringLiteral("bgm-cover")) {
            m_bgmCoverArt = QUrl::fromLocalFile(cachePath).toString();
            emit bgmChanged();
            return;
        }
        m_libraryModel->updateThumbnail(id, QUrl::fromLocalFile(cachePath).toString());
        if (!title.isEmpty()) {
            m_libraryModel->updateName(id, title);
            saveState();
        }
        indexMediaAsset(m_libraryModel->getRowById(id));
    });

    // ── Initial Load ──
    loadState();
    
    // Startup prefetch: build the media/song indexes in the background without blocking launch.
    requestScanJwMedia();
}

BroadcastController::~BroadcastController()
{
    saveState();
}

void BroadcastController::registerCameras()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    QVariantList inputAssets;
    for (const QCameraDevice &cam : cameras) {
        QVariantMap m;
        m.insert("id", QString::fromUtf8(cam.id()));
        m.insert("name", cam.description());
        m.insert("type", "input");
        m.insert("absolutePath", QString::fromUtf8(cam.id()));
        m.insert("thumbnailPath", "qrc:/MediaFlow/qml/assets/cam_placeholder.png");
        m.insert("category", "Cameras");
        m.insert("isStaged", true);
        inputAssets.append(m);
    }
    m_libraryModel->appendFromVariantList(inputAssets);
}

void BroadcastController::selectSegment(const QString &id)
{
    m_selectedSegmentId = id;
    emit selectedSegmentIdChanged();

    if (id.isEmpty()) {
        m_filterProxy->setStagedIds({});
        return;
    }

    int row = m_meetingModel->rowOfId(id);
    if (row != -1) {
        QStringList ids = m_meetingModel->data(m_meetingModel->index(row, 0), MeetingScheduleModel::AssociatedMediaIdsRole).toStringList();
        m_filterProxy->setStagedIds(ids);
        m_filterProxy->setSelectedSegmentId(m_selectedSegmentId);
    }
}

void BroadcastController::bindMediaToSequence(const QString &mediaId)
{
    if (m_selectedSegmentId.isEmpty()) return;
    int row = m_meetingModel->rowOfId(m_selectedSegmentId);
    if (row != -1) {
        m_meetingModel->addLinkedMedia(row, mediaId);
        selectSegment(m_selectedSegmentId);
        saveState();
    }
}

void BroadcastController::removeMediaFromSequence(const QString &seqId, const QString &mediaId)
{
    int row = m_meetingModel->rowOfId(seqId);
    if (row != -1) {
        m_meetingModel->removeLinkedMedia(row, mediaId);
        if (seqId == m_selectedSegmentId) selectSegment(seqId);
        saveState();
    }
}

void BroadcastController::setMeetingTypeStr(const QString &type)
{
    if (m_meetingType != type) {
        m_meetingType = type;
        if (m_meetingModel) m_meetingModel->loadMeeting(type);
        m_selectedSegmentId = "";
        emit selectedSegmentIdChanged();
        emit meetingTypeChanged();
        saveState();
    }
}

QVariantMap BroadcastController::currentLanguage() const
{
    const QString code = SongSearchUtils::normalizeLanguageCode(m_languageCode);
    return QVariantMap{{QStringLiteral("name"), SongSearchUtils::languageNameForCode(code)},
                       {QStringLiteral("code"), code}};
}

void BroadcastController::setCurrentLanguage(const QVariantMap &language)
{
    const QString code = language.value(QStringLiteral("code")).toString();
    setCurrentLanguageCode(code.isEmpty() ? language.value(QStringLiteral("name")).toString() : code);
}

void BroadcastController::setCurrentLanguageCode(const QString &lang)
{
    applyLanguageCode(lang, true, true);
}

void BroadcastController::setMasterVolume(int v)
{
    v = qBound(0, v, 100);
    if (m_masterVolume != v) {
        m_masterVolume = v;
        emit masterVolumeChanged();
    }
}

void BroadcastController::setMixerMuted(bool muted) {
    if (m_mixerMuted == muted) return;
    m_mixerMuted = muted;
    m_broadcastEngine->setProgramMuted(muted);
    emit mixerMutedChanged();
}

void BroadcastController::setProgramCameraDevice(const QCameraDevice &device) {
    if (m_programCameraDevice == device) return;
    m_programCameraDevice = device;
    emit programCameraDeviceChanged();
}

void BroadcastController::setMeetingLive(bool live)
{
    if (m_isMeetingLive != live) {
        m_isMeetingLive = live;
        emit isMeetingLiveChanged();
    }
}

// BGM
void BroadcastController::scanBgmFolder()
{
    m_bgmPlaylist.clear();
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QStringList bgmDirs;
    bgmDirs << home + "/Music/MediaFlow/BGM";
    bgmDirs << home + "/Music";
    bgmDirs << home + "/Videos/JWLibrary";

    QStringList audioFilters = {"*.mp3", "*.m4a", "*.wav"};

    for (const QString &dir : bgmDirs) {
        if (!QDir(dir).exists()) continue;
        QDirIterator it(dir, audioFilters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            m_bgmPlaylist.append(it.next());
        }
    }

    if (!m_bgmPlaylist.isEmpty()) loadBgmTrack(0);
    emit bgmChanged();
}

void BroadcastController::loadBgmTrack(int index)
{
    if (index < 0 || index >= m_bgmPlaylist.count()) return;
    m_bgmIndex = index;
    m_bgmCoverArt.clear();
    m_bgmPlayer->setSource(QUrl::fromLocalFile(m_bgmPlaylist.at(index)));
    if (m_thumbManager)
        m_thumbManager->enqueue(QStringLiteral("bgm-cover"), m_bgmPlaylist.at(index), QStringLiteral("audio"));
    emit bgmChanged();
}

void BroadcastController::toggleBgmPlayback()
{
    if (m_bgmPlaylist.isEmpty()) { scanBgmFolder(); return; }
    if (m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState) m_bgmPlayer->pause();
    else m_bgmPlayer->play();
    emit bgmChanged();
}

void BroadcastController::nextBgm()
{
    if (m_bgmPlaylist.isEmpty()) return;
    int next = (m_bgmIndex + 1) % m_bgmPlaylist.count();
    loadBgmTrack(next);
    m_bgmPlayer->play();
}

void BroadcastController::backBgm()
{
    if (m_bgmPlaylist.isEmpty()) return;
    int prev = (m_bgmIndex - 1 + m_bgmPlaylist.count()) % m_bgmPlaylist.count();
    loadBgmTrack(prev);
    m_bgmPlayer->play();
}

void BroadcastController::stopBgm() { m_bgmPlayer->stop(); emit bgmChanged(); }

QString BroadcastController::bgmPath() const {
    if (m_bgmPlaylist.isEmpty() || m_bgmIndex >= m_bgmPlaylist.count()) return "";
    return m_bgmPlaylist.at(m_bgmIndex);
}

bool BroadcastController::isPlayingBgm() const {
    return m_bgmPlayer && m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState;
}

QString BroadcastController::bgmTrackName() const {
    if (m_bgmPlaylist.isEmpty() || m_bgmIndex >= m_bgmPlaylist.count()) return "NO TRACKS";
    return QFileInfo(m_bgmPlaylist.at(m_bgmIndex)).baseName();
}

void BroadcastController::onBgmStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) nextBgm();
}

void BroadcastController::requestScanJwMedia()
{
    ensureExtractor();
    m_scanStatus = tr("Scanning JW Library assets...");
    emit scanStatusChanged();
    m_extractor->startScan();
}

void BroadcastController::requestScanCustomFolder()
{
    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("Select Media Folder"), "");
    if (dir.isEmpty()) return;
    ensureExtractor();
    m_scanStatus = tr("Scanning custom folder...");
    emit scanStatusChanged();
    m_extractor->scanDirectory(dir);
}

void BroadcastController::stageMedia(const QString &assetId)
{
    for (int i = 0; i < m_libraryModel->rowCount(); ++i) {
        QModelIndex idx = m_libraryModel->index(i);
        if (m_libraryModel->data(idx, MediaLibraryModel::IdRole).toString() == assetId) {
            MediaAsset asset;
            asset.id = assetId;
            asset.absolutePath = m_libraryModel->data(idx, MediaLibraryModel::PathRole).toString();
            asset.type = m_libraryModel->data(idx, MediaLibraryModel::TypeRole).toString();
            asset.name = m_libraryModel->data(idx, MediaLibraryModel::NameRole).toString();
            asset.thumbnailPath = m_libraryModel->data(idx, MediaLibraryModel::ThumbnailRole).toString();
            m_broadcastEngine->setPreviewAsset(asset);
            break;
        }
    }
}

void BroadcastController::findAndStageSong(int songNumber, const QString &languageCode, const QString &targetSegmentId)
{
    const QString lang = languageCode.isEmpty() ? m_languageCode : languageCode;
    resolveSongToSegment(songNumber, lang, targetSegmentId, true);
}

void BroadcastController::renameCategory(const QString &oldName, const QString &newName) { m_libraryModel->renameCategory(oldName, newName); saveState(); }
void BroadcastController::removeMedia(const QString &id) { m_libraryModel->removeMedia(id); saveState(); }
bool BroadcastController::hasSecondaryScreen() const { return QGuiApplication::screens().size() > 1; }
void BroadcastController::updateScreenCount() { emit hasSecondaryScreenChanged(); if (m_feedExtended) openAudienceWindow(); }

QVariantMap BroadcastController::findSong(int num, const QString &lang, bool prefVideo, const QString &track)
{
    const QString type = prefVideo ? QStringLiteral("video") : QStringLiteral("audio");
    QVariantMap result = m_songIndex.value(songIndexKey(lang, num, type, track));
    if (result.isEmpty())
        result = m_songIndex.value(songIndexKey(lang, num, type));
    if (result.isEmpty())
        result = getSong(num, lang);

    if (!result.value(QStringLiteral("found")).toBool())
        return result;

    result.insert(QStringLiteral("preferredType"), type);
    result.insert(QStringLiteral("preferredTrack"), track);
    return result;
}

QVariantMap BroadcastController::getSong(int number, const QString &langCode) const
{
    QVariantMap result = m_songIndex.value(songIndexKey(langCode, number));
    if (result.isEmpty()) {
        result.insert(QStringLiteral("found"), false);
        result.insert(QStringLiteral("code"), SongSearchUtils::normalizeLanguageCode(langCode));
        result.insert(QStringLiteral("songNumber"), number);
        result.insert(QStringLiteral("languageName"), SongSearchUtils::languageNameForCode(langCode));
        return result;
    }

    result.insert(QStringLiteral("found"), true);
    if (!result.contains(QStringLiteral("path")))
        result.insert(QStringLiteral("path"), result.value(QStringLiteral("absolutePath")));
    return result;
}


void BroadcastController::openAudienceWindow()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *targetScreen = screens.size() > 1 ? screens.at(1) : QGuiApplication::primaryScreen();

    if (!m_audienceWindow) {
        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/MediaFlow/qml/AudienceWindow.qml")));
        if (component.status() != QQmlComponent::Ready) {
            qWarning() << "AudienceWindow component failed:" << component.errors();
            m_feedExtended = false;
            emit feedExtendedChanged();
            return;
        }
        QObject *created = component.create();
        m_audienceWindow = qobject_cast<QQuickWindow *>(created);
        if (!m_audienceWindow) {
            qWarning() << "AudienceWindow did not create a QQuickWindow:" << created;
            if (created)
                created->deleteLater();
            m_feedExtended = false;
            emit feedExtendedChanged();
            return;
        }
    }

    if (m_audienceWindow) {
        m_audienceWindow->hide();
        if (targetScreen)
            m_audienceWindow->setScreen(targetScreen);
        if (screens.size() > 1) {
            m_audienceWindow->setGeometry(targetScreen->geometry());
            m_audienceWindow->showFullScreen();
        } else {
            m_audienceWindow->resize(1280, 720);
            m_audienceWindow->show();
            m_audienceWindow->raise();
            m_audienceWindow->requestActivate();
        }
        m_feedExtended = true;
        emit feedExtendedChanged();
    }
}

void BroadcastController::closeAudienceWindow() { if (m_audienceWindow) m_audienceWindow->hide(); m_feedExtended = false; emit feedExtendedChanged(); }
void BroadcastController::toggleAudienceWindow() { if (m_feedExtended) closeAudienceWindow(); else openAudienceWindow(); }

bool BroadcastController::openZoomWindow()
{
    if (!m_zoomWindow) {
        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/MediaFlow/qml/VirtualCameraWindow.qml")));
        if (component.status() != QQmlComponent::Ready) {
            qWarning() << "VirtualCameraWindow component failed:" << component.errors();
            return false;
        }

        QObject *created = component.create();
        m_zoomWindow = qobject_cast<QQuickWindow *>(created);
        if (!m_zoomWindow) {
            qWarning() << "VirtualCameraWindow did not create a QQuickWindow:" << created;
            if (created)
                created->deleteLater();
            return false;
        }
    }

    m_zoomWindow->setGeometry(-10000, -10000, 1920, 1080);
    m_zoomWindow->show();
    return true;
}

bool BroadcastController::hasObsVirtualCamera() const
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &camera : cameras) {
        const QString description = camera.description();
        const QString id = QString::fromUtf8(camera.id());
        if (description.contains(QStringLiteral("OBS Virtual Camera"), Qt::CaseInsensitive)
            || description.contains(QStringLiteral("OBS-Camera"), Qt::CaseInsensitive)
            || description.contains(QStringLiteral("OBS Camera"), Qt::CaseInsensitive)
            || id.contains(QStringLiteral("OBS Virtual Camera"), Qt::CaseInsensitive)
            || id.contains(QStringLiteral("OBS-Camera"), Qt::CaseInsensitive)
            || id.contains(QStringLiteral("OBS Camera"), Qt::CaseInsensitive)
            || id.contains(QStringLiteral("obs"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

void BroadcastController::toggleZoomBroadcast() { 
    qDebug() << "toggleZoomBroadcast() clicked!";
    if (m_vcamManager->isBroadcasting()) {
        m_vcamManager->stop(); 
        if (m_zoomWindow)
            m_zoomWindow->hide();
    } else {
        if (!hasObsVirtualCamera()) {
            qWarning() << "OBS Virtual Camera is not installed or not detected.";
        } else if (openZoomWindow() && m_zoomWindow) {
            m_vcamManager->start(m_zoomWindow.data());
        }
    }
    m_vcamEnabled = m_vcamManager->isBroadcasting(); 
    emit vcamEnabledChanged(); 
}

void BroadcastController::clearMediaIndexes()
{
    m_mediaIndexByPath.clear();
    m_songIndex.clear();
}

void BroadcastController::indexMediaAsset(const QVariantMap &asset)
{
    const QString path = asset.value(QStringLiteral("absolutePath")).toString();
    if (path.isEmpty())
        return;

    QVariantMap indexed = asset;
    const QVariantMap songMetadata = parseSongMetadata(path);
    for (auto it = songMetadata.cbegin(); it != songMetadata.cend(); ++it)
        indexed.insert(it.key(), it.value());

    if (!indexed.contains(QStringLiteral("path")))
        indexed.insert(QStringLiteral("path"), path);
    if (!indexed.contains(QStringLiteral("name")) || indexed.value(QStringLiteral("name")).toString().isEmpty())
        indexed.insert(QStringLiteral("name"), QFileInfo(path).fileName());

    m_mediaIndexByPath.insert(normalizedMediaPath(path), indexed);

    if (!indexed.value(QStringLiteral("isSong")).toBool())
        return;

    const QString languageCode = indexed.value(QStringLiteral("languageCode")).toString();
    const int songNumber = indexed.value(QStringLiteral("songNumber")).toInt();
    const QString type = indexed.value(QStringLiteral("type")).toString();
    const QString track = indexed.value(QStringLiteral("trackType")).toString();
    if (languageCode.isEmpty() || songNumber <= 0)
        return;

    indexed.insert(QStringLiteral("found"), true);
    indexed.insert(QStringLiteral("code"), languageCode);
    indexed.insert(QStringLiteral("languageName"), SongSearchUtils::languageNameForCode(languageCode));

    const QStringList keys{
        songIndexKey(languageCode, songNumber),
        songIndexKey(languageCode, songNumber, type),
        songIndexKey(languageCode, songNumber, type, track),
        songIndexKey(languageCode, songNumber, QString(), track)
    };

    for (const QString &key : keys) {
        const QVariantMap existing = m_songIndex.value(key);
        if (existing.isEmpty() || mediaRank(indexed) > mediaRank(existing))
            m_songIndex.insert(key, indexed);
    }
}

void BroadcastController::onMediaFound(MediaType type, const QString &name, const QString &absolutePath, const QImage &thumbnail, const QDateTime &creationDate)
{
    Q_UNUSED(creationDate); Q_UNUSED(thumbnail);
    if (m_libraryModel->containsPath(absolutePath)) {
        indexMediaAsset(m_libraryModel->getRowById(m_libraryModel->idOfPath(absolutePath)));
        return;
    }

    QVariantMap m;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.insert("id", id);

    QString cleanName = name;
    
    // Pattern: sjjm_E_012 -> Song 12
    QRegularExpression songRegex("sjjm_[^_]+_(\\d+)", QRegularExpression::CaseInsensitiveOption);
    auto match = songRegex.match(cleanName);
    if (match.hasMatch()) {
        cleanName = "Song " + QString::number(match.captured(1).toInt());
    } else {
        // Strip resolution and common suffixes
        cleanName.remove(QRegularExpression("_r?\\d+P$", QRegularExpression::CaseInsensitiveOption));
        cleanName.remove(QRegularExpression("_univ$", QRegularExpression::CaseInsensitiveOption));
        cleanName.remove(QRegularExpression("^\\d+_"));
        cleanName.replace('_', ' ');
        cleanName = cleanName.trimmed();
    }
    m.insert("name", cleanName);

    QString typeStr;
    switch (type) {
        case MediaType::Video: typeStr = "video"; break;
        case MediaType::Image: typeStr = "image"; break;
        case MediaType::Audio: typeStr = "audio"; break;
        case MediaType::Input: typeStr = "input"; break;
    }
    m.insert("type", typeStr);
    m.insert("absolutePath", absolutePath);
    m.insert("category", "General");
    m.insert("thumbnailPath", (type == MediaType::Image) ? QUrl::fromLocalFile(absolutePath).toString() : "");
    m.insert("isStaged", false);
    const QVariantMap songMetadata = parseSongMetadata(absolutePath);
    for (auto it = songMetadata.cbegin(); it != songMetadata.cend(); ++it)
        m.insert(it.key(), it.value());

    m_libraryModel->appendFromVariantList({m});
    indexMediaAsset(m);
    if (m_thumbManager) m_thumbManager->enqueue(id, absolutePath, typeStr);
}

void BroadcastController::onScanFinished() { m_scanStatus = tr("Scan complete."); emit scanStatusChanged(); }

void BroadcastController::saveState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    QSaveFile file(path + "/app_state.json");
    if (!file.open(QIODevice::WriteOnly)) return;
    QJsonObject root;
    root["midweek"] = QJsonArray::fromVariantList(m_meetingModel->getFullState("midweek"));
    root["weekend"] = QJsonArray::fromVariantList(m_meetingModel->getFullState("weekend"));
    QVariantList importedItems;
    for (int i = 0; i < m_libraryModel->rowCount(); ++i) {
        QVariantMap m = m_libraryModel->getRowById(m_libraryModel->data(m_libraryModel->index(i), MediaLibraryModel::IdRole).toString());
        if (m["isImported"].toBool()) importedItems << m;
    }
    root["importedMedia"] = QJsonArray::fromVariantList(importedItems);
    root["meetingType"] = m_meetingType;
    const QString code = SongSearchUtils::normalizeLanguageCode(m_languageCode);
    root["currentLanguageCode"] = code;
    root["currentLanguageName"] = SongSearchUtils::languageNameForCode(code);
    file.write(QJsonDocument(root).toJson());
    file.commit();
}

void BroadcastController::loadState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app_state.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        applyLanguageCode(m_languageCode, false, false);
        return;
    }
    QJsonParseError parseError;
    QJsonObject root = QJsonDocument::fromJson(file.readAll(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        applyLanguageCode(m_languageCode, false, false);
        return;
    }
    const QVariantList importedMedia = root["importedMedia"].toArray().toVariantList();
    m_libraryModel->appendFromVariantList(importedMedia);
    for (const QVariant &item : importedMedia)
        indexMediaAsset(item.toMap());
    m_meetingModel->setFullState("midweek", root["midweek"].toArray().toVariantList());
    m_meetingModel->setFullState("weekend", root["weekend"].toArray().toVariantList());
    m_meetingType = root["meetingType"].toString("midweek");
    const QString loadedLanguage = root["currentLanguageCode"].toString(root["languageCode"].toString("E"));
    
    // Sync models with loaded state
    if (m_meetingModel) m_meetingModel->loadMeeting(m_meetingType);
    applyLanguageCode(loadedLanguage, false, false);

    emit meetingTypeChanged();
    if (!m_selectedSegmentId.isEmpty()) selectSegment(m_selectedSegmentId);
}

void BroadcastController::ensureExtractor() { if (!m_extractor) { m_extractor = new MediaExtractor(this); connect(m_extractor, &MediaExtractor::mediaFound, this, &BroadcastController::onMediaFound); connect(m_extractor, &MediaExtractor::scanFinished, this, &BroadcastController::onScanFinished); } }

// Legacy Timer System Removed in favor of TimerController


// Media Actions
void BroadcastController::browseAndAddMedia(const QString &seqId, const QString &mediaType) {
    QString filter = (mediaType == "video") ? "Videos (*.mp4 *.m4v *.mov *.mkv)" : "Images (*.jpg *.png *.jpeg *.webp)";
    QString file = QFileDialog::getOpenFileName(nullptr, tr("Select Media"), "", filter);
    if (file.isEmpty()) return;
    
    QVariantMap m;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.insert("id", id);
    m.insert("name", QFileInfo(file).completeBaseName());
    m.insert("type", mediaType);
    m.insert("absolutePath", file);
    m.insert("category", "Imported");
    m.insert("isImported", true);
    m.insert("thumbnailPath", (mediaType == "image") ? QUrl::fromLocalFile(file).toString() : "");
    m_libraryModel->appendFromVariantList({m});
    indexMediaAsset(m);
    
    if (m_thumbManager) m_thumbManager->enqueue(id, file, mediaType);
    
    if (!seqId.isEmpty()) bindMediaToSequence(id);
}

void BroadcastController::previewMediaByPath(const QString &path) {
    MediaAsset asset;
    asset.absolutePath = path;
    asset.name = QFileInfo(path).fileName();
    asset.type = path.endsWith(".mp4", Qt::CaseInsensitive) ? "video" : "image";
    m_broadcastEngine->setPreviewAsset(asset);
}

void BroadcastController::importMediaToFileSystem(const QString &category) {
    QString jwVideos = QDir::homePath() + "/Videos/JWLibrary";
    QString startDir = QDir(jwVideos).exists() ? jwVideos : QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    QString file = QFileDialog::getOpenFileName(nullptr, tr("Import Media"), startDir, "Media Files (*.mp4 *.m4v *.mov *.mkv *.jpg *.png *.jpeg *.webp *.mp3 *.m4a)");
    if (file.isEmpty()) return;
    
    QVariantMap m;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.insert("id", id);
    m.insert("name", QFileInfo(file).completeBaseName());
    m.insert("type", file.endsWith(".mp3") || file.endsWith(".m4a") ? "audio" : (file.endsWith(".jpg") || file.endsWith(".png") ? "image" : "video"));
    m.insert("absolutePath", file);
    m.insert("category", category);
    m.insert("isImported", true);
    m_libraryModel->appendFromVariantList({m});
    indexMediaAsset(m);
    if (m_thumbManager) m_thumbManager->enqueue(id, file, m["type"].toString());
    saveState();
}

QVariantMap BroadcastController::addMediaToSegment(const QString &segmentId, const QString &mediaType)
{
    QString startDir;
    QString filter;
    
    // User-requested paths
    QString jwVideos = QDir::homePath() + "/Videos/JWLibrary";
    QString jwImages = QDir::homePath() + "/AppData/Local/Packages/WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e/LocalState/Publications";

    if (mediaType == "image") {
        startDir = QDir(jwImages).exists() ? jwImages : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        filter = "Images (*.jpg *.jpeg *.png *.webp)";
    } else {
        startDir = QDir(jwVideos).exists() ? jwVideos : QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        filter = "Videos (*.mp4 *.m4v *.mov *.avi *.mkv)";
    }

    QString file = QFileDialog::getOpenFileName(nullptr, tr("Select Media"), startDir, filter);
    if (file.isEmpty()) return {};

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = QFileInfo(file).completeBaseName();

    QVariantMap result;
    result.insert("id", id);
    result.insert("name", name);
    result.insert("type", mediaType);
    result.insert("absolutePath", file);
    result.insert("isImported", true);
    result.insert("category", "Meeting");

    m_libraryModel->appendFromVariantList({result});
    indexMediaAsset(result);
    
    // Thumbnail generation
    if (m_thumbManager) m_thumbManager->enqueue(id, file, mediaType);

    // Bind to segment
    int row = m_meetingModel->rowOfId(segmentId);
    if (row != -1) {
        m_meetingModel->addLinkedMedia(row, id);
        saveState();
    }

    return result;
}

QVariantList BroadcastController::getSupportedLanguages() const {
    return SongSearchUtils::supportedLanguages();
}

QVariantMap BroadcastController::getLanguageMap() const {
    QVariantMap map;
    for (const auto &v : getSupportedLanguages()) {
        QVariantMap m = v.toMap();
        map.insert(m["code"].toString(), m["name"].toString());
    }
    return map;
}

void BroadcastController::applyLanguageCode(const QString &languageCode, bool persist, bool reResolveSongs)
{
    const QString normalized = SongSearchUtils::normalizeLanguageCode(languageCode);
    const bool changed = m_languageCode != normalized;

    m_languageCode = normalized;
    if (m_proxyModel)
        m_proxyModel->setLanguageCode(normalized);
    if (m_filterProxy)
        m_filterProxy->setLanguageCode(normalized);

    if (changed) {
        emit currentLanguageCodeChanged();
        emit currentLanguageChanged();
        if (reResolveSongs)
            reResolveSongSegmentsForCurrentLanguage();
    }

    if (persist)
        saveState();
}

QString BroadcastController::languageName(const QString &languageCode) const
{
    return SongSearchUtils::languageNameForCode(languageCode);
}

QString BroadcastController::resolveSongToSegment(int songNumber, const QString &languageCode, const QString &targetSegmentId, bool warnOnMissing)
{
    const QString code = SongSearchUtils::normalizeLanguageCode(languageCode);
    const QVariantMap result = getSong(songNumber, code);
    const int row = m_meetingModel->rowOfId(targetSegmentId);

    if (!result.value(QStringLiteral("found")).toBool()) {
        if (row != -1)
            m_meetingModel->setSongNumber(row, songNumber);
        if (warnOnMissing) {
            emit songNotFoundInLanguage(songNumber, SongSearchUtils::languageNameForCode(code));
        }
        return {};
    }

    QString id = result.value(QStringLiteral("id")).toString();
    const QString path = result.value(QStringLiteral("absolutePath")).toString();
    if (id.isEmpty() && !path.isEmpty())
        id = m_libraryModel->idOfPath(path);
    if (id.isEmpty())
        return {};

    if (row != -1) {
        m_meetingModel->setLinkedMedia(row, QStringList{id});
        m_meetingModel->setSongNumber(row, songNumber);
        if (targetSegmentId == m_selectedSegmentId)
            selectSegment(targetSegmentId);
        saveState();
    }

    return id;
}

void BroadcastController::reResolveSongSegmentsForCurrentLanguage()
{
    if (!m_meetingModel)
        return;

    for (int row = 0; row < m_meetingModel->rowCount(); ++row) {
        const QModelIndex idx = m_meetingModel->index(row, 0);
        if (m_meetingModel->data(idx, MeetingScheduleModel::TypeRole).toString() != QStringLiteral("song"))
            continue;

        const int songNumber = m_meetingModel->data(idx, MeetingScheduleModel::SongNumberRole).toInt();
        if (songNumber <= 0)
            continue;

        const QString segmentId = m_meetingModel->data(idx, MeetingScheduleModel::IdRole).toString();
        resolveSongToSegment(songNumber, m_languageCode, segmentId, true);
    }
}
