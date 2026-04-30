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

try:
    ctypes.windll.winmm.timeBeginPeriod(1)
except Exception:
    pass

try:
    ABOVE_NORMAL_PRIORITY_CLASS = 0x00008000
    ctypes.windll.kernel32.SetPriorityClass(
        ctypes.windll.kernel32.GetCurrentProcess(),
        ABOVE_NORMAL_PRIORITY_CLASS
    )
except Exception:
    pass

def fit_frame_to_target(frame, target_width, target_height):
    src_height, src_width = frame.shape[:2]
    if src_width == target_width and src_height == target_height:
        return frame

    src_aspect = src_width / src_height
    target_aspect = target_width / target_height

    if src_aspect > target_aspect:
        out_width = target_width
        out_height = max(1, round(target_width / src_aspect))
    else:
        out_height = target_height
        out_width = max(1, round(target_height * src_aspect))

    x_idx = np.linspace(0, src_width - 1, out_width).astype(np.int32)
    y_idx = np.linspace(0, src_height - 1, out_height).astype(np.int32)
    resized = frame[y_idx][:, x_idx]

    output = np.zeros((target_height, target_width, frame.shape[2]), dtype=frame.dtype)
    x_offset = (target_width - out_width) // 2
    y_offset = (target_height - out_height) // 2
    output[y_offset:y_offset + out_height, x_offset:x_offset + out_width] = resized
    return output

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

def capture_window(hwnd, target_width, target_height):
    PW_RENDERFULLCONTENT = 2
    try:
        left, top, right, bottom = win32gui.GetWindowRect(hwnd)
        cap_width = right - left
        cap_height = bottom - top
        if cap_width < 100 or cap_height < 100: return None
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
        return fit_frame_to_target(img, target_width, target_height)
    return None

def open_virtual_camera():
    attempts = [
        ('Auto virtual camera', None),
        ('OBS Virtual Camera', 'OBS Virtual Camera'),
    ]
    fmt_priority = [pyvirtualcam.PixelFormat.BGR, pyvirtualcam.PixelFormat.RGB]
    errors = []

    for label, device_name in attempts:
        for fmt in fmt_priority:
            try:
                kwargs = {
                    'width': TARGET_WIDTH,
                    'height': TARGET_HEIGHT,
                    'fps': FPS,
                    'fmt': fmt,
                }
                if device_name:
                    kwargs['device'] = device_name

                cam = pyvirtualcam.Camera(**kwargs)
                print(f"Connected to {label}")
                return cam
            except Exception as e:
                errors.append(f"{label}: {e}")
                print(f"DEVICE_SKIPPED:{label}:{e}")

    print("FATAL_ERROR: Virtual camera output could not be started. Stop any running OBS Virtual Camera output, close apps currently using the virtual camera, then start MediaFlow Broadcast to Zoom before selecting the camera in Zoom.")
    if errors:
        print("FATAL_DETAILS: " + " | ".join(errors))
    return None

def main():
    print(f"Starting High-Res OBS Broadcast Bridge ({TARGET_WIDTH}x{TARGET_HEIGHT})")
    
    hwnd = 0
    while hwnd == 0:
        hwnd = get_window_hwnd(WINDOW_TITLE_PART)
        if hwnd == 0: time.sleep(1)

    print(f"Connected to Broadcast Window: {win32gui.GetWindowText(hwnd)}")
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNA)

    cam = open_virtual_camera()

    if not cam:
        sys.exit(1)

    print(f"DEVICE_ACTIVE:{cam.device}")

    last_good_frame = np.zeros((TARGET_HEIGHT, TARGET_WIDTH, 3), dtype=np.uint8)
    missed_frames = 0

    try:
        while True:
            if not win32gui.IsWindow(hwnd):
                hwnd = get_window_hwnd(WINDOW_TITLE_PART)
                if hwnd == 0:
                    cam.send(last_good_frame)
                    cam.sleep_until_next_frame()
                    continue

            frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)

            if frame is not None:
                missed_frames = 0
                if cam.fmt == pyvirtualcam.PixelFormat.BGR:
                    out = frame[:, :, :3]
                else:
                    out = frame[:, :, :3][:, :, ::-1]

                # Send the window exactly as rendered. Zoom may mirror the local
                # self-preview, but the virtual camera frame itself must stay
                # unflipped so media text and slides remain readable.
                last_good_frame = np.ascontiguousarray(out)
            else:
                missed_frames += 1
                if missed_frames % (FPS * 2) == 0:
                    hwnd = get_window_hwnd(WINDOW_TITLE_PART)

            cam.send(last_good_frame)
            cam.sleep_until_next_frame()
    except KeyboardInterrupt: pass
    except Exception as e: print(f"FATAL_ERROR: {str(e)}")
    finally:
        try:
            ctypes.windll.winmm.timeEndPeriod(1)
        except Exception:
            pass
        if cam: cam.__exit__(None, None, None)

if __name__ == '__main__':
    main()
