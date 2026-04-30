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

WINDOW_TITLE = 'MEDIAFLOW_NATIVE_BRIDGE_TARGET'
TARGET_WIDTH = 1920
TARGET_HEIGHT = 1080
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
        if w < 100 or h < 100:
            return None
    except Exception as e:
        # print(f"DEBUG: GetWindowRect failed: {e}")
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
        # if not np.any(img[::100, ::100, :3]):
        #      pass 

        # Return the raw BGRA image (useful for Unity Capture) and the processed RGBA image (for pyvirtualcam)
        # Actually, let's just return the processed one and convert as needed.
        # But wait, pyvirtualcam likes RGBA. Unity Capture likes BGRA.
        return img # Return BGRA (raw from PrintWindow)
    else:
        return None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=['obs', 'unity'], default='obs')
    args = parser.parse_args()
    
    print(f"Target: {args.target.upper()}")
    print(f"Resolution: {TARGET_WIDTH}x{TARGET_HEIGHT}")
    print(f"Waiting for window '{WINDOW_TITLE}' to appear...")
    
    hwnd = 0
    while hwnd == 0:
        hwnd = get_window_hwnd(WINDOW_TITLE)
        if hwnd == 0:
            time.sleep(1)

    print(f"Found window! HWND: {hwnd}. Starting Bridge...")
    
    # Ensure window is "shown" even if off-screen
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNA)

    # Wait a moment for window to fully render
    time.sleep(2)

    try:
        cam = None
        pipe = None
        
        if args.target == 'obs':
            devices = ['OBS Virtual Camera', 'OBS-Camera']
            for device_name in devices:
                try:
                    print(f"Attempting to open {device_name}...")
                    cam = pyvirtualcam.Camera(width=TARGET_WIDTH, height=TARGET_HEIGHT, fps=FPS, fmt=pyvirtualcam.PixelFormat.RGBA, device=device_name)
                    break
                except Exception as e:
                    print(f"Could not open {device_name}: {e}")
            
            if cam is None:
                 raise Exception("Could not find 'OBS Virtual Camera'. Ensure OBS is installed.")
                 
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
                raise Exception(f"Could not open Unity Capture pipe. Is the driver installed? Error: {e}")

        print(f"--- BRIDGE ACTIVE ---")
        print(f"TARGET: {args.target.upper()}")
        print(f"----------------------")
        
        while True:
            # If window closed, exit
            if not win32gui.IsWindow(hwnd):
                print("Window closed. Exiting.")
                break

            bgra_frame = capture_window(hwnd, TARGET_WIDTH, TARGET_HEIGHT)
            
            if bgra_frame is not None:
                if args.target == 'obs' and cam:
                    # Convert BGRA to RGBA for OBS
                    rgba = np.empty_like(bgra_frame)
                    rgba[:, :, 0] = bgra_frame[:, :, 2] # R
                    rgba[:, :, 1] = bgra_frame[:, :, 1] # G
                    rgba[:, :, 2] = bgra_frame[:, :, 0] # B
                    rgba[:, :, 3] = bgra_frame[:, :, 3] # A
                    cam.send(rgba)
                    cam.sleep_until_next_frame()
                elif args.target == 'unity' and pipe:
                    # Unity Capture expects raw BGRA bytes
                    win32file.WriteFile(pipe, bgra_frame.tobytes())
                    time.sleep(1/FPS)
            else:
                # Send a black frame if capture fails
                black_frame = np.zeros((TARGET_HEIGHT, TARGET_WIDTH, 4), dtype=np.uint8)
                if args.target == 'obs' and cam:
                    cam.send(black_frame)
                    cam.sleep_until_next_frame()
                elif args.target == 'unity' and pipe:
                    win32file.WriteFile(pipe, black_frame.tobytes())
                    time.sleep(1/FPS)
                time.sleep(0.01)
                    
    except Exception as e:
        print(f"Error initializing bridge: {e}")
        if args.target == 'unity':
             print("TIP: Make sure you have UnityCapture driver installed and registered.")

if __name__ == '__main__':
    main()
