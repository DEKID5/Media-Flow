#include "BroadcastController.h"
#include "WorkbookManager.h"
#include "JwLibraryPaths.h"
#include "MediaExtractor.h"
#include "MediaThumbnailManager.h"
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
#include <QRandomGenerator>
#include <QSettings>

#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <wrl/client.h>

namespace {

// Windows ducks (lowers/mutes, per the user's "Communications" sound setting)
// every other app's audio session automatically whenever a call becomes
// active in an app it recognizes as a communications app -- Zoom included.
// The room's audio output is a separate physical device/purpose from the
// Zoom call feed and must stay at full quality regardless, so every audio
// session belonging to this process is explicitly opted out via WASAPI's
// per-session ducking preference (the officially supported way to exempt an
// app, rather than relying on the user's global system-wide setting, which
// may not retroactively apply to sessions that were already open).
void exemptSessionsOnDevice(IMMDevice *device, DWORD myPid, bool verbose)
{
    using Microsoft::WRL::ComPtr;

    ComPtr<IAudioSessionManager2> sessionManager;
    HRESULT hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void **>(sessionManager.GetAddressOf()));
    if (FAILED(hr)) {
        if (verbose) qWarning() << "DUCKDIAG: Activate(IAudioSessionManager2) failed hr=" << Qt::hex << hr;
        return;
    }

    ComPtr<IAudioSessionEnumerator> sessionEnumerator;
    if (FAILED(sessionManager->GetSessionEnumerator(&sessionEnumerator))) {
        if (verbose) qWarning() << "DUCKDIAG: GetSessionEnumerator failed";
        return;
    }

    int count = 0;
    sessionEnumerator->GetCount(&count);
    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> session;
        if (FAILED(sessionEnumerator->GetSession(i, &session)) || !session)
            continue;
        ComPtr<IAudioSessionControl2> session2;
        if (FAILED(session.As(&session2)) || !session2)
            continue;
        DWORD pid = 0;
        if (SUCCEEDED(session2->GetProcessId(&pid)) && pid == myPid) {
            HRESULT dhr = session2->SetDuckingPreference(TRUE); // TRUE = opt out of ducking
            if (verbose) {
                float sessionVolume = -1.0f;
                ComPtr<ISimpleAudioVolume> simpleVolume;
                if (SUCCEEDED(session.As(&simpleVolume)))
                    simpleVolume->GetMasterVolume(&sessionVolume);
                AudioSessionState state = AudioSessionStateInactive;
                session2->GetState(&state);
                qWarning() << "DUCKDIAG: session matched pid, SetDuckingPreference hr=" << Qt::hex << dhr
                           << "sessionVolume=" << sessionVolume << "state=" << (int)state;
            }
        }
    }
}

// Every new media source (a Cut/Take to a different video, a fresh BGM
// track) can tear down and recreate the underlying WASAPI audio session, and
// a role mismatch (room audio may not land on the "eMultimedia" role
// endpoint specifically) can also leave a session unexempted -- so this
// enumerates every active render endpoint, not just the default one, and is
// called repeatedly (see m_duckingExemptionTimer) rather than once, so any
// newly created session is caught within a couple of seconds regardless of
// when/why it was (re)created.
void exemptProcessAudioFromDucking(bool verbose = false)
{
    using Microsoft::WRL::ComPtr;

    const bool comInitializedHere = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) {
        const DWORD myPid = GetCurrentProcessId();
        ComPtr<IMMDeviceCollection> devices;
        if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
            UINT deviceCount = 0;
            devices->GetCount(&deviceCount);
            if (verbose) qWarning() << "DUCKDIAG: active render devices=" << deviceCount;
            for (UINT i = 0; i < deviceCount; ++i) {
                ComPtr<IMMDevice> device;
                if (SUCCEEDED(devices->Item(i, &device)) && device) {
                    if (verbose) {
                        wchar_t *devId = nullptr;
                        if (SUCCEEDED(device->GetId(&devId))) {
                            qWarning() << "DUCKDIAG: device" << i << "id=" << QString::fromWCharArray(devId);
                            CoTaskMemFree(devId);
                        }
                        ComPtr<IAudioEndpointVolume> epVolume;
                        if (SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                                        reinterpret_cast<void **>(epVolume.GetAddressOf())))) {
                            float level = -1.0f; BOOL muted = FALSE;
                            epVolume->GetMasterVolumeLevelScalar(&level);
                            epVolume->GetMute(&muted);
                            qWarning() << "DUCKDIAG: device" << i << "masterVolume=" << level << "muted=" << (bool)muted;
                        }
                    }
                    exemptSessionsOnDevice(device.Get(), myPid, verbose);
                }
            }
        } else if (verbose) {
            qWarning() << "DUCKDIAG: EnumAudioEndpoints failed";
        }
    } else if (verbose) {
        qWarning() << "DUCKDIAG: CoCreateInstance(MMDeviceEnumerator) failed hr=" << Qt::hex << hr;
    }

    if (comInitializedHere)
        CoUninitialize();
}

} // namespace

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
    , m_filterProxy(new StagedMediaProxyModel(this))
    , m_meetingModel(new MeetingScheduleModel(this))
    , m_cameraModel(new CameraDeviceModel(this))
    , m_broadcastEngine(new BroadcastEngine(this))
    , m_vcamManager(new VirtualCameraManager(this))
    , m_pinnedFolders(new PinnedFolderModel(this))
{
    m_programCameraDevice = QMediaDevices::defaultVideoInput();
    m_roomAudioOutputDevice = QMediaDevices::defaultAudioOutput();
    m_filterProxy->setSourceModel(m_libraryModel);

    // ── Screen Monitoring ──
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &BroadcastController::updateScreenCount);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &BroadcastController::updateScreenCount);

    // ── Relay Engine signals ──
    connect(m_broadcastEngine, &BroadcastEngine::isProgramPausedChanged,
            this, &BroadcastController::isProgramPausedChanged);

    // masterVolume/mixerMuted are consumed directly by AudienceWindow.qml
    // (the single audio output) rather than routed through the engine.

    // ── Auto-pause BGM when a program (video/audio) goes live ──
    // Keeps audio to a single channel: opening/closing music plays only
    // when no program video/audio is live, matching real meeting AV practice.
    connect(m_broadcastEngine, &BroadcastEngine::programAssetChanged, this, [this]() {
        const MediaAsset &a = m_broadcastEngine->programAsset();
        if (!a.absolutePath.isEmpty() && (a.type == QStringLiteral("video") || a.type == QStringLiteral("audio"))) {
            if (isPlayingBgm())
                toggleBgmPlayback();
        }
    });

    registerCameras();

    // ── BGM Player ──
    m_bgmAudio = new QAudioOutput(this);
    // Same fixed device as room audio -- BGM shouldn't get dragged along by
    // Zoom changing Windows' default device role either.
    m_bgmAudio->setDevice(m_roomAudioOutputDevice);
    m_bgmAudio->setVolume(0.6);
    m_bgmPlayer = new QMediaPlayer(this);
    m_bgmPlayer->setAudioOutput(m_bgmAudio);
    connect(m_bgmPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &BroadcastController::onBgmStatusChanged);
    connect(m_bgmPlayer, &QMediaPlayer::positionChanged, this, &BroadcastController::bgmPositionChanged);
    connect(m_bgmPlayer, &QMediaPlayer::durationChanged, this, &BroadcastController::bgmDurationChanged);

    exemptProcessAudioFromDucking();
    m_duckingExemptionTimer = new QTimer(this);
    m_duckingExemptionTimer->setInterval(2000);
    connect(m_duckingExemptionTimer, &QTimer::timeout, this, [] { exemptProcessAudioFromDucking(); });

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

    // Auto-fetch the weekly workbook once local media is indexed (so the
    // song-number-to-local-file auto-link actually has something to find),
    // then keep checking periodically -- see WorkbookManager::startAutoRefresh.
    m_workbookManager = new WorkbookManager(this, this);
    connect(m_workbookManager, &WorkbookManager::statusChanged, this, &BroadcastController::workbookStatusChanged);
    connect(m_extractor, &MediaExtractor::scanFinished, m_workbookManager, [this]() {
        if (!m_workbookAutoRefreshStarted) {
            m_workbookAutoRefreshStarted = true;
            m_workbookManager->startAutoRefresh();
        }
    });
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

void BroadcastController::reorderSegmentMedia(const QString &fromMediaId, const QString &toMediaId)
{
    if (m_selectedSegmentId.isEmpty() || fromMediaId == toMediaId) return;
    int row = m_meetingModel->rowOfId(m_selectedSegmentId);
    if (row == -1) return;
    QStringList ids = m_meetingModel->data(m_meetingModel->index(row, 0), MeetingScheduleModel::AssociatedMediaIdsRole).toStringList();
    const int fromIndex = ids.indexOf(fromMediaId);
    const int toIndex = ids.indexOf(toMediaId);
    if (fromIndex == -1 || toIndex == -1) return;
    m_meetingModel->moveLinkedMedia(row, fromIndex, toIndex);
    selectSegment(m_selectedSegmentId);
    saveState();
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
    QStringList bgmDirs;
    if (m_bgmUseCustomFolder && !m_bgmCustomFolder.isEmpty()) {
        // Custom folder replaces the defaults entirely -- the setting is
        // "use a separate folder", not "also include the defaults", so BGM
        // stays scoped to exactly what the user pointed at.
        bgmDirs << m_bgmCustomFolder;
    } else {
        QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        bgmDirs << home + "/Music/MediaFlow/BGM";
        bgmDirs << home + "/Music";
        bgmDirs << home + "/Videos/JWLibrary";
    }

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

void BroadcastController::addFilesToBgm(const QStringList &paths)
{
    static const QStringList audioExt = {"mp3", "m4a", "wav"};
    const bool wasEmpty = m_bgmPlaylist.isEmpty();

    for (const QString &raw : paths) {
        // QML drag-and-drop delivers file:// URLs (drop.urls[i].toString());
        // OS files/folders dropped directly, plain local paths otherwise.
        const QString localPath = raw.startsWith("file:") ? QUrl(raw).toLocalFile() : raw;
        QFileInfo info(localPath);
        if (!info.exists()) continue;

        if (info.isDir()) {
            QDirIterator it(localPath, {"*.mp3", "*.m4a", "*.wav"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString found = it.next();
                if (!m_bgmPlaylist.contains(found)) m_bgmPlaylist.append(found);
            }
        } else if (audioExt.contains(info.suffix().toLower())) {
            const QString abs = info.absoluteFilePath();
            if (!m_bgmPlaylist.contains(abs)) m_bgmPlaylist.append(abs);
        }
    }

    if (wasEmpty && !m_bgmPlaylist.isEmpty()) loadBgmTrack(0);
    emit bgmChanged();
}

void BroadcastController::setBgmUseCustomFolder(bool enabled)
{
    if (m_bgmUseCustomFolder == enabled) return;
    m_bgmUseCustomFolder = enabled;
    emit bgmSettingsChanged();
    scanBgmFolder();
    saveState();
}

void BroadcastController::browseBgmFolder()
{
    const QString startDir = m_bgmCustomFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
        : m_bgmCustomFolder;
    const QString dir = QFileDialog::getExistingDirectory(nullptr, tr("Choose Background Music Folder"), startDir);
    if (dir.isEmpty()) return;
    m_bgmCustomFolder = dir;
    m_bgmUseCustomFolder = true;
    emit bgmSettingsChanged();
    scanBgmFolder();
    saveState();
}

void BroadcastController::browseExtendedFeedBackground()
{
    const QString filter = tr("Images and Videos (*.jpg *.jpeg *.png *.webp *.mp4 *.m4v *.mov *.mkv)");
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString file = QFileDialog::getOpenFileName(nullptr, tr("Choose Extended Feed Background"), startDir, filter);
    if (file.isEmpty()) return;

    static const QStringList imageExt = {"jpg", "jpeg", "png", "webp"};
    const QString ext = QFileInfo(file).suffix().toLower();
    m_extendedFeedBackgroundPath = file;
    m_extendedFeedBackgroundType = imageExt.contains(ext) ? QStringLiteral("image") : QStringLiteral("video");
    emit extendedFeedBackgroundChanged();
    saveState();
}

QString BroadcastController::workbookStatus() const
{
    return m_workbookManager ? m_workbookManager->status() : QString();
}

void BroadcastController::refreshWorkbook()
{
    if (m_workbookManager) m_workbookManager->refreshNow();
}

QVariantList BroadcastController::availableWorkbookWeeks() const
{
    return m_workbookManager ? m_workbookManager->availableWeeks() : QVariantList();
}

void BroadcastController::selectWorkbookWeek(const QString &isoDate)
{
    if (!m_workbookManager) return;
    const QDate date = isoDate.isEmpty() ? QDate() : QDate::fromString(isoDate, Qt::ISODate);
    m_workbookManager->selectWeek(date);
}

void BroadcastController::clearExtendedFeedBackground()
{
    if (m_extendedFeedBackgroundPath.isEmpty()) return;
    m_extendedFeedBackgroundPath.clear();
    m_extendedFeedBackgroundType.clear();
    emit extendedFeedBackgroundChanged();
    saveState();
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

    if (m_bgmShuffle && m_bgmPlaylist.count() > 1) {
        m_bgmShuffleHistory.append(m_bgmIndex);
        int next = m_bgmIndex;
        while (next == m_bgmIndex)
            next = QRandomGenerator::global()->bounded(m_bgmPlaylist.count());
        loadBgmTrack(next);
    } else {
        int next = (m_bgmIndex + 1) % m_bgmPlaylist.count();
        loadBgmTrack(next);
    }
    m_bgmPlayer->play();
}

void BroadcastController::backBgm()
{
    if (m_bgmPlaylist.isEmpty()) return;

    if (m_bgmShuffle && !m_bgmShuffleHistory.isEmpty()) {
        int prev = m_bgmShuffleHistory.takeLast();
        loadBgmTrack(prev);
    } else if (m_bgmShuffle && m_bgmPlaylist.count() > 1) {
        int prev = m_bgmIndex;
        while (prev == m_bgmIndex)
            prev = QRandomGenerator::global()->bounded(m_bgmPlaylist.count());
        loadBgmTrack(prev);
    } else {
        int prev = (m_bgmIndex - 1 + m_bgmPlaylist.count()) % m_bgmPlaylist.count();
        loadBgmTrack(prev);
    }
    m_bgmPlayer->play();
}

void BroadcastController::stopBgm() { m_bgmPlayer->stop(); emit bgmChanged(); }

void BroadcastController::setBgmShuffle(bool enabled)
{
    if (m_bgmShuffle == enabled) return;
    m_bgmShuffle = enabled;
    m_bgmShuffleHistory.clear();
    emit bgmShuffleChanged();
}

int BroadcastController::bgmPositionMs() const
{
    return m_bgmPlayer ? static_cast<int>(m_bgmPlayer->position()) : 0;
}

int BroadcastController::bgmDurationMs() const
{
    return m_bgmPlayer ? static_cast<int>(m_bgmPlayer->duration()) : 0;
}

void BroadcastController::seekBgm(int ms)
{
    if (!m_bgmPlayer || m_bgmPlaylist.isEmpty()) return;
    m_bgmPlayer->setPosition(qBound(0, ms, static_cast<int>(m_bgmPlayer->duration())));
}

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

void BroadcastController::playPinnedFolderInPreview(const QString &folderId)
{
    if (!m_pinnedFolders) return;

    QStringList playable;
    for (const QString &id : m_pinnedFolders->mediaIdsForFolder(folderId)) {
        const QVariantMap row = m_libraryModel->getRowById(id);
        const QString type = row.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("video") || type == QStringLiteral("image"))
            playable << id;
    }
    if (playable.isEmpty()) return;

    m_previewPlaylistIds = playable;
    m_previewPlaylistIndex = 0;
    m_previewPlaylistFolderName = m_pinnedFolders->nameForFolder(folderId);
    emit previewPlaylistChanged();
    stageMedia(m_previewPlaylistIds.first());
}

void BroadcastController::stopPreviewPlaylist()
{
    if (m_previewPlaylistIds.isEmpty()) return;
    m_previewPlaylistIds.clear();
    m_previewPlaylistIndex = -1;
    m_previewPlaylistFolderName.clear();
    emit previewPlaylistChanged();
}

void BroadcastController::advancePreviewPlaylist()
{
    if (m_previewPlaylistIds.isEmpty()) return;
    m_previewPlaylistIndex = (m_previewPlaylistIndex + 1) % m_previewPlaylistIds.size();
    stageMedia(m_previewPlaylistIds.at(m_previewPlaylistIndex));
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

void BroadcastController::setExtendedFeedScreenIndex(int index)
{
    if (m_extendedFeedScreenIndex == index) return;
    m_extendedFeedScreenIndex = index;
    emit extendedFeedScreenIndexChanged();
    saveState();
    if (m_feedExtended) openAudienceWindow();
}

QVariantList BroadcastController::availableScreens() const
{
    QVariantList result;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QVariantMap m;
        m.insert("index", i);
        QScreen *s = screens.at(i);
        QString label = s->name();
        if (label.isEmpty()) label = tr("Display %1").arg(i + 1);
        m.insert("name", QStringLiteral("%1 (%2×%3)").arg(label).arg(s->geometry().width()).arg(s->geometry().height()));
        result << m;
    }
    return result;
}

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
    QScreen *targetScreen = nullptr;
    if (m_extendedFeedScreenIndex >= 0 && m_extendedFeedScreenIndex < screens.size())
        targetScreen = screens.at(m_extendedFeedScreenIndex);
    if (!targetScreen)
        targetScreen = screens.size() > 1 ? screens.at(1) : QGuiApplication::primaryScreen();

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

void BroadcastController::setTimerScreenIndex(int index)
{
    if (m_timerScreenIndex == index) return;
    m_timerScreenIndex = index;
    emit timerScreenIndexChanged();
    saveState();
    // Re-place the window immediately if it's already up, rather than
    // waiting for the next toggle, so picking a screen while the timer is
    // already showing takes effect right away.
    if (m_timerWindow && m_timerWindow->isVisible())
        setTimerFullScreenActive(true);
}

void BroadcastController::setTimerFullScreenActive(bool active)
{
    if (!active) {
        if (m_timerWindow) m_timerWindow->hide();
        return;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *targetScreen = nullptr;
    if (m_timerScreenIndex >= 0 && m_timerScreenIndex < screens.size())
        targetScreen = screens.at(m_timerScreenIndex);

    if (!targetScreen) {
        // No dedicated screen chosen (or the previously chosen one is no
        // longer connected) -- this used to silently guess screens.at(1),
        // which could land the countdown on the operator's own display in
        // front of everyone. Require an explicit pick in Settings instead;
        // TimerPanel's confirmation dialog is what normally prevents
        // reaching this path at all.
        qWarning() << "Full-screen timer: no dedicated screen configured (Settings > Displays > Timer).";
        return;
    }

    if (!m_timerWindow) {
        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/MediaFlow/qml/TimerWindow.qml")));
        if (component.status() != QQmlComponent::Ready) {
            qWarning() << "TimerWindow component failed:" << component.errors();
            return;
        }
        QObject *created = component.create();
        m_timerWindow = qobject_cast<QQuickWindow *>(created);
        if (!m_timerWindow) {
            qWarning() << "TimerWindow did not create a QQuickWindow:" << created;
            if (created) created->deleteLater();
            return;
        }
    }

    m_timerWindow->hide();
    m_timerWindow->setScreen(targetScreen);
    m_timerWindow->setGeometry(targetScreen->geometry());
    m_timerWindow->showFullScreen();
}

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

    // Must be positioned on an actual monitor -- Qt Quick never fires
    // afterRendering for a window entirely outside every screen's bounds
    // (confirmed live: the previous (-10000,-10000) placement left
    // VirtualCameraManager::onAfterRendering never firing at all, so no
    // frame was ever captured, regardless of the shared-memory protocol
    // underneath). WindowStaysOnBottomHint (set in VirtualCameraWindow.qml)
    // keeps it out of the operator's way while still rendering.
    m_zoomWindow->setGeometry(0, 0, 1920, 1080);
    m_zoomWindow->show();
    m_zoomWindow->lower();
    return true;
}

bool BroadcastController::hasVirtualCameraDriver() const
{
    // MediaFlow feeds VirtualCameraManager's output directly into OBS
    // Studio's own virtual-camera shared-memory protocol (see
    // ObsVirtualCamWriter) -- installing OBS Studio once registers its
    // DirectShow filter ("OBS Virtual Camera") system-wide; OBS Studio
    // itself never needs to run. Qt6's QMediaDevices enumerates cameras via
    // Windows Media Foundation, which doesn't reliably surface classic
    // DirectShow-only capture filters (confirmed with the previous
    // UnityCapture-based driver: correctly registered, verified in the
    // registry, yet never appeared in QMediaDevices::videoInputs()). So
    // check what the question is actually asking -- "is the driver
    // installed" -- directly against the registry's DirectShow video
    // capture sources category, which is authoritative regardless of what
    // any particular app's camera picker shows.
    QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance"),
                  QSettings::NativeFormat);
    const QStringList instances = reg.childGroups();
    for (const QString &instance : instances) {
        const QString friendlyName = reg.value(instance + QStringLiteral("/FriendlyName")).toString();
        if (friendlyName.contains(QStringLiteral("OBS Virtual Camera"), Qt::CaseInsensitive))
            return true;
    }

    return false;
}

void BroadcastController::toggleZoomBroadcast() { 
    qDebug() << "toggleZoomBroadcast() clicked!";
    if (m_vcamManager->isBroadcasting()) {
        m_vcamManager->stop();
        if (m_zoomWindow)
            m_zoomWindow->hide();
        m_duckingExemptionTimer->stop();
    } else {
        if (!hasVirtualCameraDriver()) {
            qWarning() << "OBS Virtual Camera is not installed or not detected.";
        } else if (openZoomWindow() && m_zoomWindow) {
            m_vcamManager->start(m_zoomWindow.data());
            // Re-apply the ducking exemption -- Zoom joining/activating its
            // call session is exactly the trigger Windows watches for, and
            // it can (re-)apply ducking to sessions right around that point.
            // Kept running every 2s for as long as broadcasting is on (not
            // just once) because a Cut/Take to a new video or a fresh BGM
            // track can tear down and recreate the underlying WASAPI audio
            // session, which comes back in the default (duckable) state.
            exemptProcessAudioFromDucking();
            m_duckingExemptionTimer->start();
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

void BroadcastController::onScanFinished()
{
    m_scanStatus = tr("Scan complete.");
    emit scanStatusChanged();
    if (!m_initialScanComplete) {
        m_initialScanComplete = true;
        emit initialScanCompleteChanged();
    }
}

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
    root["pinnedFolders"] = QJsonArray::fromVariantList(m_pinnedFolders->getFullState());
    root["meetingType"] = m_meetingType;
    const QString code = SongSearchUtils::normalizeLanguageCode(m_languageCode);
    root["currentLanguageCode"] = code;
    root["currentLanguageName"] = SongSearchUtils::languageNameForCode(code);
    root["extendedFeedScreenIndex"] = m_extendedFeedScreenIndex;
    root["timerScreenIndex"] = m_timerScreenIndex;
    root["bgmUseCustomFolder"] = m_bgmUseCustomFolder;
    root["bgmCustomFolder"] = m_bgmCustomFolder;
    root["extendedFeedBackgroundPath"] = m_extendedFeedBackgroundPath;
    root["extendedFeedBackgroundType"] = m_extendedFeedBackgroundType;
    root["customLanguages"] = QJsonArray::fromVariantList(m_customLanguages);
    file.write(QJsonDocument(root).toJson());
    file.commit();
}

void BroadcastController::loadState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app_state.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // No saved state yet (first run). Still populate the default meeting
        // schedule so the operator doesn't see an empty SEQUENCE panel.
        if (m_meetingModel) m_meetingModel->loadMeeting(m_meetingType);
        applyLanguageCode(m_languageCode, false, false);
        return;
    }
    QJsonParseError parseError;
    QJsonObject root = QJsonDocument::fromJson(file.readAll(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        if (m_meetingModel) m_meetingModel->loadMeeting(m_meetingType);
        applyLanguageCode(m_languageCode, false, false);
        return;
    }
    const QVariantList importedMedia = root["importedMedia"].toArray().toVariantList();
    m_libraryModel->appendFromVariantList(importedMedia);
    for (const QVariant &item : importedMedia)
        indexMediaAsset(item.toMap());
    m_pinnedFolders->setFullState(root["pinnedFolders"].toArray().toVariantList());
    m_meetingModel->setFullState("midweek", root["midweek"].toArray().toVariantList());
    m_meetingModel->setFullState("weekend", root["weekend"].toArray().toVariantList());
    m_meetingType = root["meetingType"].toString("midweek");
    const QString loadedLanguage = root["currentLanguageCode"].toString(root["languageCode"].toString("E"));
    m_extendedFeedScreenIndex = root["extendedFeedScreenIndex"].toInt(-1);
    m_timerScreenIndex = root["timerScreenIndex"].toInt(-1);
    m_bgmUseCustomFolder = root["bgmUseCustomFolder"].toBool(false);
    m_bgmCustomFolder = root["bgmCustomFolder"].toString();
    m_extendedFeedBackgroundPath = root["extendedFeedBackgroundPath"].toString();
    m_extendedFeedBackgroundType = root["extendedFeedBackgroundType"].toString();
    m_customLanguages = root["customLanguages"].toArray().toVariantList();

    // Sync models with loaded state
    if (m_meetingModel) m_meetingModel->loadMeeting(m_meetingType);
    applyLanguageCode(loadedLanguage, false, false);

    emit meetingTypeChanged();
    if (!m_selectedSegmentId.isEmpty()) selectSegment(m_selectedSegmentId);
}

void BroadcastController::ensureExtractor() { if (!m_extractor) { m_extractor = new MediaExtractor(this); connect(m_extractor, &MediaExtractor::mediaFound, this, &BroadcastController::onMediaFound); connect(m_extractor, &MediaExtractor::scanFinished, this, &BroadcastController::onScanFinished); } }

// Legacy Timer System Removed in favor of TimerController


// Media Actions
QString BroadcastController::importOneFile(const QString &absolutePath, const QString &category)
{
    const QFileInfo fi(absolutePath);
    const QString ext = fi.suffix().toLower();
    QString type;
    if (ext == "mp3" || ext == "m4a" || ext == "wav") type = QStringLiteral("audio");
    else if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp") type = QStringLiteral("image");
    else type = QStringLiteral("video");

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap m;
    m.insert("id", id);
    m.insert("name", fi.completeBaseName());
    m.insert("type", type);
    m.insert("absolutePath", absolutePath);
    m.insert("category", category);
    m.insert("isImported", true);
    m.insert("thumbnailPath", (type == QStringLiteral("image")) ? QUrl::fromLocalFile(absolutePath).toString() : "");

    m_libraryModel->appendFromVariantList({m});
    indexMediaAsset(m);
    if (m_thumbManager) m_thumbManager->enqueue(id, absolutePath, type);
    return id;
}

void BroadcastController::browseAndAddMedia(const QString &seqId, const QString &mediaType) {
    QString filter = (mediaType == "video") ? "Videos (*.mp4 *.m4v *.mov *.mkv)" : "Images (*.jpg *.png *.jpeg *.webp)";
    // Multi-select: an operator adding B-roll or a set of slides typically wants
    // several files in one pass rather than repeating this dialog per file.
    QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Select Media"), "", filter);
    if (files.isEmpty()) return;

    for (const QString &file : files) {
        const QString id = importOneFile(file, QStringLiteral("Imported"));
        if (!seqId.isEmpty()) bindMediaToSequence(id);
    }
    saveState();
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

    QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Import Media"), startDir, "Media Files (*.mp4 *.m4v *.mov *.mkv *.jpg *.png *.jpeg *.webp *.mp3 *.m4a)");
    if (files.isEmpty()) return;

    for (const QString &file : files)
        importOneFile(file, category);
    saveState();
}

QVariantMap BroadcastController::addMediaToSegment(const QString &segmentId, const QString &mediaType)
{
    QString startDir;
    QString filter;

    // User-requested paths
    QString jwVideos = QDir::homePath() + "/Videos/JWLibrary";
    const QStringList jwPublicationsDirs = JwLibraryPaths::publicationsDirs();
    QString jwImages = jwPublicationsDirs.isEmpty() ? QString() : jwPublicationsDirs.first();

    if (mediaType == "image") {
        // Prefer this week's own matched publication folder (set by
        // WorkbookManager once it's resolved the current mwb/Watchtower
        // issue locally) so the operator lands directly among that week's
        // article images instead of the whole, unsorted Publications root.
        QString weekFolder;
        if (segmentId == QStringLiteral("m9"))
            weekFolder = m_cbsPublicationFolder;
        else if (segmentId.startsWith(QStringLiteral("m")))
            weekFolder = m_mwbPublicationFolder;
        else if (segmentId == QStringLiteral("w4") || segmentId == QStringLiteral("w5"))
            weekFolder = m_watchtowerPublicationFolder;

        if (!weekFolder.isEmpty() && QDir(weekFolder).exists())
            startDir = weekFolder;
        else if (!jwImages.isEmpty() && QDir(jwImages).exists())
            startDir = jwImages;
        else
            startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        filter = "Images (*.jpg *.jpeg *.png *.webp)";
    } else {
        startDir = QDir(jwVideos).exists() ? jwVideos : QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        filter = "Videos (*.mp4 *.m4v *.mov *.avi *.mkv)";
    }

    // Multi-select: linking several videos/images to one segment in one go.
    QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Select Media"), startDir, filter);
    if (files.isEmpty()) return {};

    const int row = m_meetingModel->rowOfId(segmentId);
    QVariantMap firstResult;
    for (const QString &file : files) {
        const QString id = importOneFile(file, QStringLiteral("Meeting"));
        if (row != -1)
            m_meetingModel->addLinkedMedia(row, id);
        if (firstResult.isEmpty())
            firstResult = m_libraryModel->getRowById(id);
    }
    if (row != -1) {
        saveState();
        // addLinkedMedia() above only updates MeetingScheduleModel -- it does
        // not refresh StagedMediaProxyModel's stagedIds, which is what
        // Active Media actually filters against. Without this, newly added
        // media shows up in the Sequence chips (which read linkedMediaIds
        // directly) but never appears in Active Media itself, since the
        // proxy's stagedIds is stale until something re-selects the segment.
        if (segmentId == m_selectedSegmentId)
            selectSegment(segmentId);
    }

    return firstResult;
}

QVariantList BroadcastController::getSupportedLanguages() const {
    QVariantList result = SongSearchUtils::supportedLanguages();
    result.append(m_customLanguages);
    return result;
}

void BroadcastController::addCustomLanguage(const QString &name, const QString &code)
{
    const QString trimmedName = name.trimmed();
    const QString upperCode = code.trimmed().toUpper();
    if (trimmedName.isEmpty() || upperCode.isEmpty() || upperCode.length() > 3)
        return;

    // Reject duplicates against both built-ins and existing customs -- the
    // filename marker filter matches by code, so two languages sharing one
    // would be indistinguishable in Active Media.
    for (const auto &v : getSupportedLanguages()) {
        if (v.toMap().value("code").toString().compare(upperCode, Qt::CaseInsensitive) == 0)
            return;
    }

    QVariantMap lang;
    lang.insert("name", trimmedName);
    lang.insert("code", upperCode);
    m_customLanguages.append(lang);
    emit languagesChanged();
    saveState();
}

void BroadcastController::removeCustomLanguage(const QString &code)
{
    for (int i = 0; i < m_customLanguages.size(); ++i) {
        if (m_customLanguages.at(i).toMap().value("code").toString().compare(code, Qt::CaseInsensitive) == 0) {
            m_customLanguages.removeAt(i);
            emit languagesChanged();
            saveState();
            return;
        }
    }
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
    if (m_filterProxy)
        m_filterProxy->setLanguageCode(normalized);

    if (changed) {
        emit currentLanguageCodeChanged();
        emit currentLanguageChanged();
        if (reResolveSongs) {
            reResolveSongSegmentsForCurrentLanguage();
            // Song numbers alone aren't language-specific, but the Watchtower
            // title/images and the "week folder" addMediaToSegment defaults
            // into are matched from a specific language's own local
            // publication folder -- re-run the whole workbook lookup so all
            // of that also moves over to the newly selected language.
            if (m_workbookManager) m_workbookManager->refreshNow();
        }
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

    // Uses the *ForId MeetingScheduleModel methods (search both midweek and
    // weekend lists directly by id) rather than rowOfId()+setSongNumber(row,
    // ...)/setLinkedMedia(row, ...), which only ever touch activeRows() --
    // the currently-selected tab. Without this, resolving a song for a
    // segment on the tab the operator doesn't currently have open would
    // silently do nothing (rowOfId returns -1 there). Fixes this for both
    // the manual song-search popup and the automated workbook fetch, which
    // both go through this one function.
    if (!result.value(QStringLiteral("found")).toBool()) {
        m_meetingModel->setSongNumberForId(targetSegmentId, songNumber);
        // Don't leave the previous language's file silently linked —
        // it would otherwise still be eligible to go live.
        m_meetingModel->setLinkedMediaForId(targetSegmentId, {});
        if (targetSegmentId == m_selectedSegmentId)
            selectSegment(targetSegmentId);
        saveState();
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

    // Replaces (not appends) -- a song segment holds exactly one video, the
    // current best match for this song number/language. Auto-discovered
    // (non-imported) library entries get a fresh random id every scan (see
    // BroadcastController::onMediaFound), so appending here would leave
    // every previous restart's now-stale id for the *same* underlying file
    // sitting in the list forever, since its id never matches the new one
    // for the dedup check in addLinkedMediaForId to catch -- confirmed
    // live: repeated restarts piled up half a dozen broken-thumbnail
    // entries on Opening Song.
    m_meetingModel->setLinkedMediaForId(targetSegmentId, {id});
    m_meetingModel->setSongNumberForId(targetSegmentId, songNumber);
    if (targetSegmentId == m_selectedSegmentId)
        selectSegment(targetSegmentId);
    saveState();

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

// ──────────────────────────────────────────────────────────────────
//  Pinned Folders
// ──────────────────────────────────────────────────────────────────

QString BroadcastController::normalizeDroppedPath(const QString &pathOrUrl)
{
    // OS drag-and-drop delivers file:// URLs; a plain local path is also
    // accepted so QML can pass either without caring which.
    const QUrl url(pathOrUrl);
    if (url.isLocalFile())
        return url.toLocalFile();
    return pathOrUrl;
}

QString BroadcastController::createPinnedFolder(const QString &name)
{
    const QString id = m_pinnedFolders->createFolder(name);
    saveState();
    return id;
}

void BroadcastController::renamePinnedFolder(const QString &folderId, const QString &newName)
{
    m_pinnedFolders->renameFolder(folderId, newName);
    saveState();
}

void BroadcastController::deletePinnedFolder(const QString &folderId)
{
    m_pinnedFolders->deleteFolder(folderId);
    saveState();
}

void BroadcastController::pinMediaToFolder(const QString &folderId, const QString &mediaId)
{
    m_pinnedFolders->addMedia(folderId, mediaId);
    saveState();
}

void BroadcastController::unpinMediaFromFolder(const QString &folderId, const QString &mediaId)
{
    m_pinnedFolders->removeMedia(folderId, mediaId);
    saveState();
}

void BroadcastController::browseAndAddFilesToPinnedFolder(const QString &folderId)
{
    QString jwVideos = QDir::homePath() + "/Videos/JWLibrary";
    QString startDir = QDir(jwVideos).exists() ? jwVideos : QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Add Files to Pin"), startDir,
        "Media Files (*.mp4 *.m4v *.mov *.mkv *.jpg *.png *.jpeg *.webp *.mp3 *.m4a)");
    if (files.isEmpty()) return;

    const QString folderName = m_pinnedFolders->nameForFolder(folderId);
    for (const QString &file : files) {
        const QString id = importOneFile(file, folderName.isEmpty() ? QStringLiteral("Pinned") : folderName);
        m_pinnedFolders->addMedia(folderId, id);
    }
    saveState();
}

void BroadcastController::importFilesToPinnedFolder(const QString &folderId, const QStringList &pathsOrUrls)
{
    if (pathsOrUrls.isEmpty()) return;
    const QString folderName = m_pinnedFolders->nameForFolder(folderId);

    for (const QString &raw : pathsOrUrls) {
        const QString path = normalizeDroppedPath(raw);
        if (path.isEmpty() || !QFileInfo::exists(path) || QFileInfo(path).isDir())
            continue;

        // Dropping a file MediaFlow already knows about just pins the existing
        // asset instead of duplicating it in the library.
        const QString existingId = m_libraryModel->idOfPath(path);
        const QString id = existingId.isEmpty()
            ? importOneFile(path, folderName.isEmpty() ? QStringLiteral("Pinned") : folderName)
            : existingId;
        m_pinnedFolders->addMedia(folderId, id);
    }
    saveState();
}
