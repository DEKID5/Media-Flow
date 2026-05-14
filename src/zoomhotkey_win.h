#pragma once

#if defined(_WIN32)

namespace ZoomHotkey {

// Brings a Zoom main window to the foreground and synthesizes Alt+S (stop share).
void sendAltStopShare();

} // namespace ZoomHotkey

#endif // _WIN32
