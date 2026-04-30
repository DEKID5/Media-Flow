import time
import sys
import ctypes
import numpy as np
import pyvirtualcam
import win32gui
import win32ui
import win32con
import win32file
import argparse
from ctypes import wintypes

# Try to force DPI awareness to avoid scaling issues
try:
    ctypes.windll.user32.SetProcessDPIAware()
except AttributeError:
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
            return False # Stop enumeration
        return True
    
    try:
        win32gui.EnumWindows(enum_handler, None)
    except:
        pass
    return hwnd_found[0]

def capture_window(hwnd, width, height):
    # PW_RENDERFULLCONTENT = 2 (captures DirectComposition / hardware accelerated windows)
    PW_RENDERFULLCONTENT = 2
    
    # Get window dimensions to verify
    try:
        left, top, right, bottom = win32gui.GetWindowRect(hwnd)
        w = right - left
        h = bottom - top
        if w < 100 or h < 100:
            return None
    except Exception as e:
        return None

    # Create DC and bitmaps
    hwndDC = win32gui.GetWindowDC(hwnd)
    mfcDC  = win32ui.CreateDCFromHandle(hwndDC)
    saveDC = mfcDC.CreateCompatibleDC()

    saveBitMap = win32ui.CreateBitmap()
    saveBitMap.CreateCompatibleBitmap(mfcDC, width, height)

    saveDC.SelectObject(saveBitMap)

    # PrintWindow successfully captures off-screen / hidden windows if hardware acceleration is used
    result = ctypes.windll.user32.PrintWindow(hwnd, saveDC.GetSafeHdc(), PW_RENDERFULLCONTENT)
    
    bmpinfo = saveBitMap.GetInfo()
    bmpstr = saveBitMap.GetBitmapBits(True)
    
    # Clean up
    win32gui.DeleteObject(saveBitMap.GetHandle())
    saveDC.DeleteDC()
    mfcDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwndDC)

    if result == 1:
        img = np.frombuffer(bmpstr, dtype=np.uint8)
        img = img.reshape((height, width, 4))
        return img # Return BGRA
    else:
        return None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['obs', 'unity'], default='obs')
    args = parser.parse_args()
    
    print(f"Target: {args.target.upper()}")
    print(f"Resolution: {TARGET_WIDTH}x{TARGET_HEIGHT}")
    print(f"Searching for window containing '{WINDOW_TITLE_PART}'...")
    
    hwnd = 0
    while hwnd == 0:
        hwnd = get_window_hwnd(WINDOW_TITLE_PART)
        if hwnd == 0:
            # Fallback check for common electron title if the specific one hasn't set yet
            hwnd = get_window_hwnd("Media Flow Broadcasting")
            if hwnd != 0:
                 # Check if it's the right size or location to be the audience window
                 left, top, right, bottom = win32gui.GetWindowRect(hwnd)
                 if (right-left) != 1920: # Main window is usually 1200
                     hwnd = 0 # Not the one
            
        if hwnd == 0:
            time.sleep(1)

    print(f"Found window! HWND: {hwnd} | Title: {win32gui.GetWindowText(hwnd)}")
    
    # Ensure window is "shown"
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNA)

    try:
        cam = None
        pipe = None
        
        if args.target == 'obs':
            devices = ['OBS Virtual Camera', 'OBS-Camera', 'OBS Camera']
            for device_name in devices:
                try:
                    print(f"Attempting to open {device_name}...")
                    cam = pyvirtualcam.Camera(width=TARGET_WIDTH, height=TARGET_HEIGHT, fps=FPS, fmt=pyvirtualcam.PixelFormat.RGBA, device=device_name)
                    print(f"Successfully connected to {device_name}")
                    break
                except Exception as e:
                    print(f"Could not open {device_name}: {e}")
            
            if cam is None:
                 raise Exception("Could not find any 'OBS Virtual Camera' device. Is the driver installed and started?")
                 
        elif args.target == 'unity':
            pipe_path = r'\\.\pipe\UnityCapture'
            try:
                print(f"Attempting to open Unity Capture pipe: {pipe_path}...")
                pipe = win32file.CreateFile(
                    pipe_path,
                    win32file.GENERIC_WRITE,
                    0,
                    None,
                    win32file.OPEN_EXISTING,
                    0,
                    None
                )
                print("Unity Capture Pipe Opened Successfully.")
            except Exception as e:
                raise Exception(f"Could not open Unity Capture pipe. Is the UnityCapture driver installed and registered? Error: {e}")

        print(f"--- BRIDGE ACTIVE ---")
        
        while True:
            if not win32gui.IsWindow(hwnd):
                print("Target window lost. Exiting.")
                break

            bgra_frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)
            
            if bgra_frame is not None:
                if args.target == 'obs' and cam:
                    # Convert BGRA to RGBA
                    rgba = np.empty_like(bgra_frame)
                    rgba[:, :, 0] = bgra_frame[:, :, 2] # R
                    rgba[:, :, 1] = bgra_frame[:, :, 1] # G
                    rgba[:, :, 2] = bgra_frame[:, :, 0] # B
                    rgba[:, :, 3] = bgra_frame[:, :, 3] # A
                    cam.send(rgba)
                    cam.sleep_until_next_frame()
                elif args.target == 'unity' and pipe:
                    win32file.WriteFile(pipe, bgra_frame.tobytes())
                    time.sleep(1/FPS)
            else:
                time.sleep(0.01)
                    
    except Exception as e:
        print(f"Bridge Error: {e}")

if __name__ == '__main__':
    main()
