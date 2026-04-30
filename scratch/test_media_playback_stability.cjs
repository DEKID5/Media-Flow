const { app, BrowserWindow } = require('electron');
const path = require('node:path');
const fs = require('node:fs');

const TEST_SECONDS = Number(process.env.MEDIAFLOW_STABILITY_SECONDS || 20);
const SAMPLE_RATE = 48000;

function makeWavDataUrl(durationSeconds) {
  const samples = Math.floor(SAMPLE_RATE * durationSeconds);
  const bytesPerSample = 2;
  const blockAlign = bytesPerSample;
  const dataSize = samples * bytesPerSample;
  const buffer = Buffer.alloc(44 + dataSize);

  buffer.write('RIFF', 0);
  buffer.writeUInt32LE(36 + dataSize, 4);
  buffer.write('WAVE', 8);
  buffer.write('fmt ', 12);
  buffer.writeUInt32LE(16, 16);
  buffer.writeUInt16LE(1, 20);
  buffer.writeUInt16LE(1, 22);
  buffer.writeUInt32LE(SAMPLE_RATE, 24);
  buffer.writeUInt32LE(SAMPLE_RATE * blockAlign, 28);
  buffer.writeUInt16LE(blockAlign, 32);
  buffer.writeUInt16LE(16, 34);
  buffer.write('data', 36);
  buffer.writeUInt32LE(dataSize, 40);

  for (let i = 0; i < samples; i += 1) {
    const t = i / SAMPLE_RATE;
    const envelope = Math.min(1, t / 0.05, (durationSeconds - t) / 0.05);
    const sample = Math.round(Math.sin(2 * Math.PI * 440 * t) * 0.35 * envelope * 32767);
    buffer.writeInt16LE(sample, 44 + i * bytesPerSample);
  }

  return `data:audio/wav;base64,${buffer.toString('base64')}`;
}

function makeState(audioUrl) {
  return {
    previewAsset: null,
    programAsset: {
      id: 'stability-audio',
      name: 'Stability Test Tone',
      type: 'audio',
      url: audioUrl,
      duration: TEST_SECONDS + 5,
    },
    bgmAsset: null,
    isPlayingBgm: false,
    timer: { seconds: 0, isRunning: false, duration: 0 },
    previewTimer: false,
    mixer: { masterVolume: 70, isMuted: false },
    displaySettings: { mainDisplay: 'audience', secondaryDisplay: 'audience', showTimerOnAudience: false },
    meetingType: 'midweek',
    language: 'E',
    videoPaths: [],
    imagePaths: [],
    audioPaths: [],
    bgmFolderPath: null,
    isPermissionGranted: true,
    selectedCameraId: '',
    isMeetingLive: true,
    vcamMode: 'media',
    isProgramPaused: false,
  };
}

async function main() {
  await app.whenReady();

  const win = new BrowserWindow({
    width: 1280,
    height: 720,
    show: false,
    webPreferences: {
      contextIsolation: false,
      nodeIntegration: false,
      autoplayPolicy: 'no-user-gesture-required',
      preload: path.resolve(__dirname, 'preload_media_stability.cjs'),
    },
  });

  const consoleMessages = [];
  win.webContents.on('console-message', (_event, level, message) => {
    consoleMessages.push({ level, message });
  });

  const distIndex = path.resolve(__dirname, '..', 'dist', 'index.html');
  await win.loadURL(`file:///${distIndex.replace(/\\/g, '/')}?view=audience`);

  const audioUrl = makeWavDataUrl(TEST_SECONDS + 5);
  const state = makeState(audioUrl);

  const result = await win.webContents.executeJavaScript(`
    (async () => {
      const state = ${JSON.stringify(state)};
      const syncInterval = setInterval(() => {
        window.__pushMediaflowSync({ type: 'SYNC_STATE', state });
      }, 100);
      window.__pushMediaflowSync({ type: 'SYNC_STATE', state });

      const startedAt = performance.now();
      const samples = [];
      const errors = [];
      let audio = null;
      let mixerStep = 0;
      document.body.click();

      while (performance.now() - startedAt < 5000) {
        document.body.click();
        audio = document.querySelector('audio');
        if (audio) break;
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      clearInterval(syncInterval);

      if (!audio) return { ok: false, reason: 'audio element was not mounted', samples, errors };

      audio.volume = 0.7;
      await audio.play().catch(error => errors.push('play failed: ' + error.message));

      let lastWall = performance.now();
      let lastMedia = audio.currentTime;
      let maxGapMs = 0;
      let maxDriftMs = 0;
      let stalledEvents = 0;
      let waitingEvents = 0;
      let timeupdateEvents = 0;

      audio.addEventListener('stalled', () => { stalledEvents += 1; });
      audio.addEventListener('waiting', () => { waitingEvents += 1; });
      audio.addEventListener('timeupdate', () => { timeupdateEvents += 1; });
      audio.addEventListener('error', () => {
        errors.push('media error: ' + (audio.error ? audio.error.message : 'unknown'));
      });

      const until = performance.now() + ${TEST_SECONDS * 1000};
      while (performance.now() < until && !audio.ended) {
        await new Promise(resolve => setTimeout(resolve, 250));
        mixerStep += 1;
        state.mixer.masterVolume = 25 + ((mixerStep * 13) % 70);
        state.mixer.isMuted = mixerStep % 37 === 0;
        window.__pushMediaflowSync({ type: 'SYNC_STATE', state });
        const now = performance.now();
        const mediaNow = audio.currentTime;
        const wallDelta = (now - lastWall) / 1000;
        const mediaDelta = mediaNow - lastMedia;
        const driftMs = Math.abs(mediaDelta - wallDelta) * 1000;

        maxGapMs = Math.max(maxGapMs, now - lastWall);
        maxDriftMs = Math.max(maxDriftMs, driftMs);
        samples.push({
          wallMs: Math.round(now - startedAt),
          currentTime: Number(mediaNow.toFixed(3)),
          driftMs: Math.round(driftMs),
          readyState: audio.readyState,
          paused: audio.paused,
        });

        lastWall = now;
        lastMedia = mediaNow;
      }

      return {
        ok: errors.length === 0 && stalledEvents === 0 && waitingEvents === 0 && !audio.paused && audio.currentTime >= ${Math.max(3, TEST_SECONDS - 2)},
        currentTime: Number(audio.currentTime.toFixed(3)),
        duration: Number(audio.duration.toFixed(3)),
        maxGapMs: Math.round(maxGapMs),
        maxDriftMs: Math.round(maxDriftMs),
        stalledEvents,
        waitingEvents,
        timeupdateEvents,
        errors,
        samples: samples.slice(-12),
      };
    })();
  `);

  const badConsole = consoleMessages.filter(entry => {
    const text = entry.message.toLowerCase();
    return entry.level >= 2 || text.includes('failed') || text.includes('error') || text.includes('stalled');
  });

  const output = { result, badConsole };
  const outputPath = path.resolve(__dirname, 'media_playback_stability_result.json');
  fs.writeFileSync(outputPath, JSON.stringify(output, null, 2));
  console.log(JSON.stringify(output, null, 2));
  await win.close();
  app.quit();
}

main().catch(error => {
  console.error(error);
  app.quit();
  process.exitCode = 1;
});
