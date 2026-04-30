# MediaFlow Broadcast Suite

A cross-platform desktop broadcast management suite built with React, Electron, and Vite.

## Development

### Web Browser
To run the application in a standard web browser:
```bash
npm run dev
```

### Desktop App (Electron)
To run the application as a desktop app:
```bash
npm run dev:desktop
```

### Windows Installer
To build the Windows installer with the Zoom/OBS bridge runtime bundled:
```bash
npm run build:desktop
```

The desktop build prepares an embedded Python runtime automatically, so installed users do not need to install Python manually. OBS Studio / OBS Virtual Camera must still be available on the machine.

## Architecture

MediaFlow uses a two-window architecture:
1. **Operator Dashboard**: The main control center for managing media, timers, and mixers.
2. **Audience View**: A secondary window (usually on an external display) that shows the program output.

State is synchronized in real-time between windows using the browser's `BroadcastChannel` API.

## Preload API Reference (`window.mediaflow`)

When running in Electron, the following methods are available via the `mediaflow` bridge:

- `getSystemSnapshot()`: Returns system info (memory, displays, platform).
- `findJwDatabase()`: Attempts to locate the JW Library user database.
- `openExternalDisplay()`: Opens or focuses the audience view window.
- `isDesktop`: Boolean indicating if the app is running in Electron.

## Persistence
Settings and library state are persisted in `localStorage`:
- `mf_assets`: Indexed media assets.
- `mf_state`: Last known operator state.
