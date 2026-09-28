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

## Optional: Zoom virtual camera (OBS Virtual Camera)

**Broadcast to Zoom** feeds the program output directly into OBS Studio's own virtual-camera device, by implementing its shared-memory producer protocol directly (`src/ObsVirtualCamWriter.*`, ported field-for-field from `plugins/win-dshow/shared-memory-queue.c` in the real [obs-studio](https://github.com/obsproject/obs-studio) source). **OBS Studio itself never needs to run** — installing it once is enough, since that's what registers the "OBS Virtual Camera" DirectShow driver on the system (same as installing any webcam driver); MediaFlow acts as its own producer from then on, the same role OBS's own "Start Virtual Camera" button plays internally.

1. Install [OBS Studio](https://obsproject.com/) (free). No configuration needed — just installing it registers the driver.
2. Click **Broadcast to Zoom** in MediaFlow. It appears as **OBS Virtual Camera** in Zoom's (or any other app's) camera picker.

Only one producer can hold the shared memory at a time — if OBS Studio's own "Start Virtual Camera" is running at the same time, MediaFlow's will fail to start (surfaced as a warning, not a crash or silent failure). Don't run both at once.

Without the driver installed at all, **Broadcast to Zoom** shows a warning dialog and does nothing destructive.

## Optional: remove locked `resources/python`

If an old Electron-era embeddable Python folder remains and Windows reports “access denied” when deleting, close any process using those files, then delete `resources/python` manually. It is not used by the Qt build.
