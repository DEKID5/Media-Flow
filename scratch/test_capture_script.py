import time
import sys
import ctypes
import numpy as np
import win32gui
import win32ui
import win32con
from PIL import Image

def get_window_hwnd(title_part):
    hwnd_found = [0]
    def enum_handler(hwnd, lparam):
        if title_part in win32gui.GetWindowText(hwnd):
            hwnd_found[0] = hwnd
            return False
        return True
    win32gui.EnumWindows(enum_handler, None)
    return hwnd_found[0]

def capture_window(hwnd, width, height):
    PW_RENDERFULLCONTENT = 2
    hwndDC = win32gui.GetWindowDC(hwnd)
    mfcDC  = win32ui.CreateDCFromHandle(hwndDC)
    saveDC = mfcDC.CreateCompatibleDC()
    saveBitMap = win32ui.CreateBitmap()
    saveBitMap.CreateCompatibleBitmap(mfcDC, width, height)
    saveDC.SelectObject(saveBitMap)
    result = ctypes.windll.user32.PrintWindow(hwnd, saveDC.GetSafeHdc(), PW_RENDERFULLCONTENT)
    bmpstr = saveBitMap.GetBitmapBits(True)
    
    # Clean up
    win32gui.DeleteObject(saveBitMap.GetHandle())
    saveDC.DeleteDC()
    mfcDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwndDC)

    if result == 1:
        img = np.frombuffer(bmpstr, dtype=np.uint8)
        img = img.reshape((height, width, 4))
        return img # BGRA
    return None

def main():
    title = "Media Flow" # Try to find something
    hwnd = get_window_hwnd(title)
    if not hwnd:
        print(f"Could not find window with title {title}")
        return
    
    print(f"Capturing window: {win32gui.GetWindowText(hwnd)}")
    frame = capture_window(hwnd, 1920, 1080)
    if frame is not None:
        # Convert BGRA to RGBA for PIL
        rgba = frame.copy()
        rgba[:, :, 0] = frame[:, :, 2]
        rgba[:, :, 2] = frame[:, :, 0]
        img = Image.fromarray(rgba, 'RGBA')
        img.save('scratch/test_capture.png')
        print("Saved capture to scratch/test_capture.png")
    else:
        print("Capture failed")

if __name__ == '__main__':
    main()
