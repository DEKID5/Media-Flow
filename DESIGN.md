# Design (Qt 6)

## Architecture

1. **QML** (`qml/`) — `ApplicationWindow` operator shell, `Window` audience view, controls and `VideoOutput` / `MediaPlayer` for playback.
2. **`MediaFlowController`** (`src/MediaFlowController.*`) — context property `MediaFlow`: `Q_PROPERTY` for UI binding, slots for actions, owns `MediaLibraryModel`.
3. **Workers on `QThread`** — `JwLibraryWorker` (SQLite discovery + read-only inspection), `MediaScanWorker` (filesystem scan). Invoked with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` so the GUI thread stays responsive.
4. **Dual display** — audience `QQuickWindow` is created dynamically, assigned to `QGuiApplication::screens().at(1)` when two screens exist, then `showFullScreen()`.

## Data paths (Windows)

Same layout as the former Electron main process: JW Library UWP packages under `%LOCALAPPDATA%\Packages\…\LocalState`, plus `Videos\JWLibrary`.

## Operator / audience sync

Both windows use the same `MediaFlow` context property: all `Q_PROPERTY` values (program, preview, timer, mixer, meeting, vcam, bridge status, etc.) update both UIs without a separate IPC channel.

## Virtual camera / OBS

The **ffmpeg UDP bridge** (`FfmpegUdpBridge`) streams the current **program** MP4/M4V to a configurable UDP URL (default `udp://127.0.0.1:6000?pkt_size=1316`) for an **OBS Media Source** or similar. Requires an `ffmpeg` binary on `PATH` or `MEDIAFLOW_FFMPEG` / the **ffmpeg** field in the UI. This replaces the old Python `pyvirtualcam` pipe while keeping the repo C++-only.

## Background music

`QMediaPlayer` + `QAudioOutput` in `MediaFlowController` plays BGM on the default audio device; volume follows **BGM** slider and **room** master/mute.

## Zoom integration

Windows-only code in `zoomhotkey_win.cpp` locates a Zoom process window and synthesizes **Alt+S** with `SendInput`.
