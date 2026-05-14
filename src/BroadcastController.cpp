#include "BroadcastController.h"
#include "MediaExtractor.h"
#include "MediaThumbnailManager.h"
#include "MediaLibraryProxyModel.h"
#include "SongSearchUtils.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDebug>
#include <QUuid>
#include <QBuffer>
#include <QMediaDevices>
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
    , m_bridge(new FfmpegUdpBridge(this))
{
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
        m_libraryModel->updateThumbnail(id, QUrl::fromLocalFile(cachePath).toString());
        if (!title.isEmpty()) {
            m_libraryModel->updateName(id, title);
            saveState();
        }
    });

    // ── Initial Load ──
    loadState();
    
    // Auto scan JW media on startup if nothing loaded?
    if (m_libraryModel->rowCount() <= QMediaDevices::videoInputs().size()) {
        requestScanJwMedia();
    }
}

BroadcastController::~BroadcastController()
{
    saveState();
}

void BroadcastController::resetApp()
{
    m_libraryModel->clear();
    m_meetingModel->clearAllMedia();
    m_filterProxy->setStagedIds({});
    
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(appData + "/app_state.json");
    
    QDir cacheDir(appData + "/MediaThumbnails");
    if (cacheDir.exists()) cacheDir.removeRecursively();

    registerCameras();
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
    setCurrentLanguageCode(language.value(QStringLiteral("code")).toString());
}

void BroadcastController::setCurrentLanguageCode(const QString &lang)
{
    const QString normalized = SongSearchUtils::normalizeLanguageCode(lang);
    if (m_languageCode != normalized) {
        m_languageCode = normalized;
        if (m_proxyModel) m_proxyModel->setLanguageCode(normalized);
        emit currentLanguageCodeChanged();
        emit currentLanguageChanged();
        reResolveSongSegmentsForCurrentLanguage();
        saveState();
    }
}

void BroadcastController::setMasterVolume(int v)
{
    v = qBound(0, v, 100);
    if (m_masterVolume != v) {
        m_masterVolume = v;
        emit masterVolumeChanged();
    }
}

void BroadcastController::setMixerMuted(bool muted)
{
    if (m_mixerMuted != muted) {
        m_mixerMuted = muted;
        emit mixerMutedChanged();
    }
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
    m_bgmPlayer->setSource(QUrl::fromLocalFile(m_bgmPlaylist.at(index)));
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
    Q_UNUSED(prefVideo);
    Q_UNUSED(track);
    QVariantMap res = SongSearchUtils::findSongFile(num, lang, m_libraryModel->toVariantList());
    if (res.value(QStringLiteral("found")).toBool() && !res.contains(QStringLiteral("path")))
        res.insert(QStringLiteral("path"), res.value(QStringLiteral("absolutePath")));
    return res;
}


void BroadcastController::openAudienceWindow()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *targetScreen = screens.size() > 1 ? screens.at(1) : QGuiApplication::primaryScreen();

    if (!m_audienceWindow) {
        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/MediaFlow/qml/AudienceWindow.qml")));
        if (component.status() != QQmlComponent::Ready) return;
        m_audienceWindow = qobject_cast<QQuickWindow *>(component.create());
    }

    if (m_audienceWindow) {
        m_audienceWindow->hide();
        m_audienceWindow->setScreen(targetScreen);
        if (screens.size() > 1) {
            m_audienceWindow->setGeometry(targetScreen->geometry());
            m_audienceWindow->showFullScreen();
        } else {
            m_audienceWindow->resize(1280, 720);
            m_audienceWindow->show();
        }
        m_feedExtended = true;
        emit feedExtendedChanged();
    }
}

void BroadcastController::closeAudienceWindow() { if (m_audienceWindow) m_audienceWindow->hide(); m_feedExtended = false; emit feedExtendedChanged(); }
void BroadcastController::toggleAudienceWindow() { if (m_feedExtended) closeAudienceWindow(); else openAudienceWindow(); }
void BroadcastController::toggleZoomBroadcast() { if (m_bridge->running()) m_bridge->stop(); else m_bridge->startWithCamera("Integrated Camera"); m_vcamEnabled = m_bridge->running(); emit vcamEnabledChanged(); }

void BroadcastController::onMediaFound(MediaType type, const QString &name, const QString &absolutePath, const QImage &thumbnail, const QDateTime &creationDate)
{
    Q_UNUSED(creationDate); Q_UNUSED(thumbnail);
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
    m_libraryModel->appendFromVariantList({m});
    if (m_thumbManager) m_thumbManager->enqueue(id, absolutePath, typeStr);
}

void BroadcastController::onScanFinished() { m_scanStatus = tr("Scan complete."); emit scanStatusChanged(); }

void BroadcastController::saveState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    QFile file(path + "/app_state.json");
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
    root["currentLanguageCode"] = m_languageCode;
    file.write(QJsonDocument(root).toJson());
}

void BroadcastController::loadState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app_state.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_libraryModel->appendFromVariantList(root["importedMedia"].toArray().toVariantList());
    m_meetingModel->setFullState("midweek", root["midweek"].toArray().toVariantList());
    m_meetingModel->setFullState("weekend", root["weekend"].toArray().toVariantList());
    m_meetingType = root["meetingType"].toString("midweek");
    m_languageCode = root["currentLanguageCode"].toString("E");
    
    // Sync models with loaded state
    if (m_meetingModel) m_meetingModel->loadMeeting(m_meetingType);
    if (m_proxyModel) m_proxyModel->setLanguageCode(m_languageCode);

    emit meetingTypeChanged();
    emit currentLanguageCodeChanged();
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

QString BroadcastController::languageName(const QString &languageCode) const
{
    return SongSearchUtils::languageNameForCode(languageCode);
}

QString BroadcastController::resolveSongToSegment(int songNumber, const QString &languageCode, const QString &targetSegmentId, bool warnOnMissing)
{
    const QString code = SongSearchUtils::normalizeLanguageCode(languageCode);
    const QVariantMap result = SongSearchUtils::findSongFile(songNumber, code, m_libraryModel->toVariantList());
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
