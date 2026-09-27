# Build (Qt 6 only)

This repository is a **C++/Qt 6** application. UI is **QML**; logic runs in **C++** on worker threads where appropriate.

## Prerequisites

- CMake 3.21+
- Qt 6.5+ (Core, Gui, Widgets, Qml, Quick, Sql, Multimedia, MultimediaQuick, Concurrent)

## Configure and build (Windows example)

```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build build --config Release
```

The executable is **`MediaFlow`** (target `mediaflow`). Run it from `build\Release\MediaFlow.exe` or your generator output directory.

Set **`CMAKE_PREFIX_PATH`** to your Qt installation’s kit root (the folder that contains `lib/cmake/Qt6`).

## Optional: ffmpeg (video thumbnails)

`MediaThumbnailManager` uses `ffmpeg` to extract video thumbnails when it's available (falling back to a native `QMediaPlayer`-based grab otherwise). Install [ffmpeg](https://ffmpeg.org) and ensure `ffmpeg.exe` is on `PATH`, or drop it next to `MediaFlow.exe`.

## Optional: Zoom virtual camera (UnityCapture)

**Broadcast to Zoom** feeds the program output into a virtual webcam using [UnityCapture](https://github.com/schellingb/UnityCapture) — a small, MIT-licensed DirectShow virtual camera driver built for exactly this ("feed frames from my app into a virtual webcam"). MediaFlow talks to it directly (`src/UnityCaptureWriter.*`); the driver itself is a separate one-time install, same as any other webcam driver:

1. Download the latest release from the [UnityCapture releases page](https://github.com/schellingb/UnityCapture/releases) into `drivers/unitycapture.zip` (git-ignored — this repo doesn't vendor the binary).
2. Extract it and run `Install.bat` as Administrator (registers the DirectShow filter via `regsvr32`).
3. It then appears as a normal webcam named **Unity Video Capture** in Zoom's camera picker, and to `BroadcastController::hasVirtualCameraDriver()`.

Without it installed, **Broadcast to Zoom** shows a warning dialog and does nothing destructive — no crash, no silent failure.

## Optional: remove locked `resources/python`

If an old Electron-era embeddable Python folder remains and Windows reports “access denied” when deleting, close any process using those files, then delete `resources/python` manually. It is not used by the Qt build.
