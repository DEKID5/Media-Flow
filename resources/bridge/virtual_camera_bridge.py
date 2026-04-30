import time
import sys
import ctypes
import numpy as np
import pyvirtualcam
import win32gui
import win32ui
import win32con
from ctypes import wintypes

# Try to force DPI awareness to avoid scaling issues
try:
    ctypes.windll.user32.SetProcessDPIAware()
except AttributeError:
    pass

WINDOW_TITLE = 'MEDIAFLOW_NATIVE_BRIDGE_TARGET'
TARGET_WIDTH = 1280
TARGET_HEIGHT = 720
FPS = 30

def get_window_hwnd(title):
    hwnd = win32gui.FindWindow(None, title)
    return hwnd

def capture_window(hwnd, width, height):
    # PW_RENDERFULLCONTENT = 2 (captures DirectComposition / hardware accelerated windows)
    PW_RENDERFULLCONTENT = 2
    
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
        # Convert raw BGRA bytes to a numpy array, then to RGBA format for pyvirtualcam
        img = np.frombuffer(bmpstr, dtype=np.uint8)
        img = img.reshape((height, width, 4))
        # Convert BGRA to RGBA
        rgba = np.empty_like(img)
        rgba[:, :, 0] = img[:, :, 2] # R
        rgba[:, :, 1] = img[:, :, 1] # G
        rgba[:, :, 2] = img[:, :, 0] # B
        rgba[:, :, 3] = img[:, :, 3] # A
        return rgba
    else:
        return None

def main():
    print(f"Waiting for window '{WINDOW_TITLE}' to appear...")
    hwnd = 0
    while hwnd == 0:
        hwnd = get_window_hwnd(WINDOW_TITLE)
        if hwnd == 0:
            time.sleep(1)

    print(f"Found window! HWND: {hwnd}. Starting Virtual Camera...")

    # Wait a moment for window to fully render
    time.sleep(2)

    try:
        # Request standard RGBA pixel format
        # By not specifying a device, it will pick the first available one (usually Unity Video Capture)
        with pyvirtualcam.Camera(width=TARGET_WIDTH, height=TARGET_HEIGHT, fps=FPS, fmt=pyvirtualcam.PixelFormat.RGBA) as cam:
            print(f"DEVICE_ACTIVE: {cam.device}")
            print(f"--- VIRTUAL CAMERA ACTIVE ---")
            print(f"DEVICE: {cam.device}")
            print(f"Please select '{cam.device}' in Zoom.")
            print(f"-----------------------------")
            
            while True:
                start_time = time.time()
                
                # If window closed, exit
                if not win32gui.IsWindow(hwnd):
                    print("Window closed. Exiting.")
                    break

                frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)
                if frame is not None:
                    cam.send(frame)
                    cam.sleep_until_next_frame()
                else:
                    # If capture fails (e.g. window resizing), just sleep
                    time.sleep(1/FPS)
                    
    except Exception as e:
        print(f"Error initializing virtual camera: {e}")
        print("Make sure you have OBS Virtual Camera installed.")

if __name__ == '__main__':
    main()
