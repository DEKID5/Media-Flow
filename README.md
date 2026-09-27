# Media Flow (Qt 6)

Desktop media operator and audience display for Kingdom Hall–style workflows. Built with **C++** and **Qt 6 QML** (no Node.js, Electron, or Python runtime in this tree).

## Build

See [BUILD.md](BUILD.md).

## Features (current Qt port)

- JW Library `userData.db` discovery under `%LOCALAPPDATA%`, inspection via **QSqlDatabase** on a worker thread (temp copy when the live DB is locked).
- Recursive media scan of JW Library package paths and `Videos\JWLibrary`, with optional language suffix filter; import files and scan custom folders into the library.
- **Broadcast state** on `MediaFlow`: program + **preview** buses, transport pause, **midweek/weekend meeting schedule**, **live** toggle, **vcam mode**, **countdown timer**, **room volume + mute**, **BGM** (C++ `QMediaPlayer`), **camera** list with program-to-camera.
- **ffmpeg UDP bridge** (optional `ffmpeg` on PATH or `MEDIAFLOW_FFMPEG`) to feed OBS or other receivers; see [DESIGN.md](DESIGN.md).
- Operator + **audience** window (fullscreen on second screen when present); **Zoom** **Alt+S** on Windows.

