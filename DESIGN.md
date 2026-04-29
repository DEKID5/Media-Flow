# MediaFlow Design Document

## Data Flow
1. **Discovery**: Electron main process scans specified paths and JW Library database.
2. **Index**: Discovered metadata is transformed into `MediaAsset` objects.
3. **UI**: Operator Dashboard displays the library and allows queuing media.
4. **Broadcast**: State changes (program asset, timer) are sent via `BroadcastChannel`.
5. **Audience**: Audience view listens for messages and updates its display reactively.

## File Access
- **JW Library**: Reads from standard OS-specific paths.
- **Custom Scans**: Users can select folders (supported via `dialog.showOpenDialog` in future iterations).
- **User Data**: Electron `userData` folder is used for caching and logs.

## Failure Modes
- **No JW DB**: App displays a warning but allows manual file imports.
- **Disconnected**: If BroadcastChannel fails, windows will fall out of sync (rare in single-process Electron).
- **Unsupported Media**: Uses standard HTML5 video/audio support; fallback to icons if rendering fails.
