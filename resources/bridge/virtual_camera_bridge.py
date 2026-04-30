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
    
    # Get window dimensions to verify
    try:
        left, top, right, bottom = win32gui.GetWindowRect(hwnd)
        w = right - left
        h = bottom - top
        if w < 10 or h < 10:
            return None
    except Exception as e:
        print(f"DEBUG: GetWindowRect failed: {e}")
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
        # Convert raw BGRA bytes to a numpy array, then to RGBA format for pyvirtualcam
        img = np.frombuffer(bmpstr, dtype=np.uint8)
        img = img.reshape((height, width, 4))
        
        # Check if the frame is all zeros (black)
        # We only check a few pixels for performance
        if not np.any(img[::100, ::100, :3]):
             # Frame is likely black. This happens if PrintWindow fails to capture content.
             pass 

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
    
    # Ensure window is "shown" even if off-screen
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNA)

    # Wait a moment for window to fully render
    time.sleep(2)

    try:
        # The user requested to use ONLY OBS Virtual Camera.
        # Try different possible names for the OBS Virtual Camera device.
        devices = ['OBS Virtual Camera', 'OBS-Camera']
        cam = None
        
        for device_name in devices:
            try:
                print(f"Attempting to open {device_name}...")
                cam = pyvirtualcam.Camera(width=TARGET_WIDTH, height=TARGET_HEIGHT, fps=FPS, fmt=pyvirtualcam.PixelFormat.RGBA, device=device_name)
                break
            except Exception as e:
                print(f"Could not open {device_name}: {e}")
        
        if cam is None:
             raise Exception("Could not find 'OBS Virtual Camera' or 'OBS-Camera'. Please ensure OBS Virtual Camera is installed and started.")
        
        with cam:
            print(f"DEVICE_ACTIVE: {cam.device}")
            print(f"--- VIRTUAL CAMERA ACTIVE ---")
            print(f"DEVICE: {cam.device}")
            print(f"Please select '{cam.device}' in Zoom.")
            print(f"-----------------------------")
            
            consecutive_failures = 0
            while True:
                # If window closed, exit
                if not win32gui.IsWindow(hwnd):
                    print("Window closed. Exiting.")
                    break

                frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)
                if frame is not None:
                    cam.send(frame)
                    cam.sleep_until_next_frame()
                    consecutive_failures = 0
                else:
                    consecutive_failures += 1
                    # Send a black frame if capture fails to avoid gray screen in Zoom
                    black_frame = np.zeros((TARGET_HEIGHT, TARGET_WIDTH, 4), dtype=np.uint8)
                    cam.send(black_frame)
                    cam.sleep_until_next_frame()
                    
                    if consecutive_failures % 30 == 0:
                        print(f"DEBUG: Capture failing for {consecutive_failures} frames. Check window visibility.")
                        # Try to refresh window handle just in case
                        new_hwnd = get_window_hwnd(WINDOW_TITLE)
                        if new_hwnd != 0:
                            hwnd = new_hwnd
                    
                    time.sleep(0.01)
                    
    except Exception as e:
        print(f"Error initializing virtual camera: {e}")
        print("Make sure you have OBS Virtual Camera installed.")

if __name__ == '__main__':
    main()
