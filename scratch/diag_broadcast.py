import win32gui
import win32con
import os

def list_windows():
    def enum_handler(hwnd, results):
        if win32gui.IsWindowVisible(hwnd):
            results.append((hwnd, win32gui.GetWindowText(hwnd)))
    
    results = []
    win32gui.EnumWindows(enum_handler, results)
    return results

print("--- Visible Windows ---")
for hwnd, text in list_windows():
    if text:
        print(f"HWND: {hwnd} | Title: {text}")

print("\n--- Checking Unity Capture Pipe ---")
if os.path.exists(r'\\.\pipe\UnityCapture'):
    print("Unity Capture Pipe FOUND")
else:
    print("Unity Capture Pipe NOT FOUND")

print("\n--- Checking OBS Virtual Camera (via pyvirtualcam simulation) ---")
try:
    import pyvirtualcam
    print("pyvirtualcam is installed.")
except ImportError:
    print("pyvirtualcam NOT installed.")
