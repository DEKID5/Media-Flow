import time
import sys
import ctypes
import numpy as np
import pyvirtualcam
import win32gui
import win32ui
import win32con
import argparse
from ctypes import wintypes

# Force DPI awareness
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

WINDOW_TITLE_PART = 'MEDIAFLOW_NATIVE_BRIDGE_TARGET'
TARGET_WIDTH = 1920
TARGET_HEIGHT = 1080
FPS = 30

def get_window_hwnd(title_part):
    hwnd_found = [0]
    def enum_handler(hwnd, lparam):
        if title_part in win32gui.GetWindowText(hwnd):
            hwnd_found[0] = hwnd
            return False
        return True
    try:
        win32gui.EnumWindows(enum_handler, None)
    except:
        pass
    return hwnd_found[0]

def capture_window(hwnd, cap_width, cap_height):
    PW_RENDERFULLCONTENT = 2
    try:
        left, top, right, bottom = win32gui.GetWindowRect(hwnd)
        if (right - left) < 100: return None
    except:
        return None

    hwndDC = win32gui.GetWindowDC(hwnd)
    mfcDC  = win32ui.CreateDCFromHandle(hwndDC)
    saveDC = mfcDC.CreateCompatibleDC()

    saveBitMap = win32ui.CreateBitmap()
    saveBitMap.CreateCompatibleBitmap(mfcDC, cap_width, cap_height)
    saveDC.SelectObject(saveBitMap)

    result = ctypes.windll.user32.PrintWindow(hwnd, saveDC.GetSafeHdc(), PW_RENDERFULLCONTENT)
    bmpstr = saveBitMap.GetBitmapBits(True)

    win32gui.DeleteObject(saveBitMap.GetHandle())
    saveDC.DeleteDC()
    mfcDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwndDC)

    if result == 1:
        img = np.frombuffer(bmpstr, dtype=np.uint8)
        img = img.reshape((cap_height, cap_width, 4))
        return img
    return None

def main():
    print(f"Starting High-Res OBS Broadcast Bridge ({TARGET_WIDTH}x{TARGET_HEIGHT})")
    
    hwnd = 0
    while hwnd == 0:
        hwnd = get_window_hwnd(WINDOW_TITLE_PART)
        if hwnd == 0: time.sleep(1)

    print(f"Connected to Broadcast Window: {win32gui.GetWindowText(hwnd)}")
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNA)

    device_candidates = ['OBS Virtual Camera']
    fmt_priority = [pyvirtualcam.PixelFormat.BGR, pyvirtualcam.PixelFormat.RGB]
    
    cam = None
    for device_name in device_candidates:
        for fmt in fmt_priority:
            try:
                cam = pyvirtualcam.Camera(width=TARGET_WIDTH, height=TARGET_HEIGHT, fps=FPS, fmt=fmt, device=device_name)
                print(f"Connected to {device_name}")
                break
            except Exception as e:
                print(f"DEVICE_SKIPPED:{device_name}:{e}")
                continue
        if cam: break

    if not cam:
        print("FATAL_ERROR: OBS Virtual Camera could not be started. Close Zoom and stop OBS Virtual Camera in OBS, start MediaFlow Broadcast to Zoom first, then select OBS Virtual Camera in Zoom.")
        sys.exit(1)

    print(f"DEVICE_ACTIVE:{cam.device}")

    try:
        while True:
            if not win32gui.IsWindow(hwnd):
                break

            frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)

            if frame is not None:
                if cam.fmt == pyvirtualcam.PixelFormat.BGR:
                    out = frame[:, :, :3]
                else:
                    out = frame[:, :, :3][:, :, ::-1]

                # FLIP HORIZONTALLY to match user expectation for virtual cameras
                out = np.ascontiguousarray(np.flip(out, axis=1))

                cam.send(out)
                cam.sleep_until_next_frame()
            else:
                time.sleep(0.05)
    except KeyboardInterrupt: pass
    except Exception as e: print(f"FATAL_ERROR: {str(e)}")
    finally:
        if cam: cam.__exit__(None, None, None)

if __name__ == '__main__':
    main()
