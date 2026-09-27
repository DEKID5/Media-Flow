# Design (Qt 6)

## Architecture

1. **QML** (`qml/`) — `ApplicationWindow` operator shell (`Main.qml`/`OperatorDashboard.qml`), a dynamically-created audience `Window` (`AudienceWindow.qml`), and a hidden off-screen `Window` used as the Zoom virtual-camera render source (`VirtualCameraWindow.qml`).
2. **`BroadcastController`** (`src/BroadcastController.*`) — registered as the `MediaFlowBackend` singleton: `Q_PROPERTY`s for UI binding, `Q_INVOKABLE` slots for actions, owns `MediaLibraryModel`, `MeetingScheduleModel`, `CameraDeviceModel`, `BroadcastEngine`, and `VirtualCameraManager`.
3. **Workers** — `MediaExtractor` (recursive JW Library filesystem scan + `QFileSystemWatcher`, background thread via `QtConcurrent`), `MediaThumbnailManager` (thumbnail generation, `QThreadPool` + a dedicated `QMediaPlayer`-based native fallback when `ffmpeg` isn't on `PATH`).
4. **Dual display** — the audience `QQuickWindow` is created dynamically, assigned to `QGuiApplication::screens().at(1)` when a second screen exists, then `showFullScreen()`; falls back to a normal window on a single-screen setup.

## Data paths (Windows)

JW Library UWP packages under `%LOCALAPPDATA%\Packages\…\LocalState`, plus `Videos\JWLibrary`.

## Operator / audience sync

All three windows (operator, audience, Zoom output) bind to the same `MediaFlowBackend` singleton and its `BroadcastEngine`: `Q_PROPERTY` values (program/preview asset, pause state, meeting schedule, vcam state, etc.) update every UI without a separate IPC channel.

## Audio routing

Exactly one player is ever audible at a time: the operator's PREVIEW/LIVE monitors (`MonitorView.qml`) are always muted (video confidence only); the **audience window** is the sole audio output, with its volume bound directly to the room **master volume**/**mute** controls. Background music auto-pauses whenever a program (video/audio) asset goes live, and stays paused until the operator resumes it, so it never plays under program audio.

## Zoom virtual camera

`VirtualCameraManager` captures `VirtualCameraWindow.qml`'s rendered frames (`QQuickWindow::afterRendering` → async PBO readback) and feeds them to Zoom (or any app) via `UnityCaptureWriter`, an implementation of the [UnityCapture](https://github.com/schellingb/UnityCapture) shared-memory protocol — a small MIT-licensed DirectShow virtual camera made for exactly this. See [BUILD.md](BUILD.md) for installing the driver. `VirtualCameraWindow.qml` shows the live camera feed when nothing (or a camera "input" asset) is the program, and switches to the program video/image otherwise — matching what the audience actually sees.

## Zoom hotkey

Windows-only code in `zoomhotkey_win.cpp` locates a Zoom process window and synthesizes **Alt+S** with `SendInput`.
