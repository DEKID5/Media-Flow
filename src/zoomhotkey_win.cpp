#if defined(_WIN32)

#include "zoomhotkey_win.h"

#include <QString>
#include <QtGlobal>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

BOOL CALLBACK enumZoomProc(HWND hwnd, LPARAM lParam)
{
    auto *out = reinterpret_cast<HWND *>(lParam);
    if (!IsWindowVisible(hwnd))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return TRUE;

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc)
        return TRUE;

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameW(proc, 0, path, &size)) {
        CloseHandle(proc);
        return TRUE;
    }
    CloseHandle(proc);

    const QString fullPath = QString::fromWCharArray(path);
    if (!fullPath.contains(QStringLiteral("Zoom"), Qt::CaseInsensitive))
        return TRUE;

    wchar_t title[256]{};
    GetWindowTextW(hwnd, title, 256);
    const QString t = QString::fromWCharArray(title);
    if (t.isEmpty())
        return TRUE;

    *out = hwnd;
    return FALSE;
}

HWND findZoomMainWindow()
{
    HWND found = nullptr;
    EnumWindows(enumZoomProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

void sendKeyCombo(WORD vk, bool withAlt)
{
    INPUT inputs[4]{};
    int n = 0;
    if (withAlt) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_MENU;
        inputs[n].ki.dwFlags = 0;
        ++n;
    }
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = vk;
    inputs[n].ki.dwFlags = 0;
    ++n;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = vk;
    inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
    ++n;
    if (withAlt) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_MENU;
        inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
        ++n;
    }
    SendInput(n, inputs, sizeof(INPUT));
}

} // namespace

namespace ZoomHotkey {

void sendAltStopShare()
{
    HWND zoom = findZoomMainWindow();
    if (!zoom)
        return;

    const DWORD fgThread = GetCurrentThreadId();
    const DWORD zoomThread = GetWindowThreadProcessId(zoom, nullptr);
    if (zoomThread && zoomThread != fgThread) {
        AttachThreadInput(fgThread, zoomThread, TRUE);
        SetForegroundWindow(zoom);
        BringWindowToTop(zoom);
        AttachThreadInput(fgThread, zoomThread, FALSE);
    } else {
        SetForegroundWindow(zoom);
    }

    sendKeyCombo(0x53 /* VK_S */, true);
}

} // namespace ZoomHotkey

#endif // _WIN32
