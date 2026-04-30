import win32gui

def callback(hwnd, extra):
    title = win32gui.GetWindowText(hwnd)
    if title:
        rect = win32gui.GetWindowRect(hwnd)
        w = rect[2] - rect[0]
        h = rect[3] - rect[1]
        print(f"HWND: {hwnd} | Size: {w}x{h} | Title: {title}")

win32gui.EnumWindows(callback, None)
