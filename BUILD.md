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

## Optional: ffmpeg (UDP bridge / OBS)

The in-app **Start bridge** action runs `ffmpeg` as a separate process. Install [ffmpeg](https://ffmpeg.org) and ensure it is on `PATH`, or set the environment variable **`MEDIAFLOW_FFMPEG`** to the full path of `ffmpeg.exe`, or fill in the **ffmpeg** field in the operator UI.

## Optional: remove locked `resources/python`

If an old Electron-era embeddable Python folder remains and Windows reports “access denied” when deleting, close any process using those files, then delete `resources/python` manually. It is not used by the Qt build.
