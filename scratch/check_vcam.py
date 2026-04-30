import pyvirtualcam

print("Searching for virtual cameras...")
# List all devices
try:
    with pyvirtualcam.Camera(width=1280, height=720, fps=30) as cam:
        print(f"Successfully initialized: {cam.device}")
except Exception as e:
    print(f"Initialization failed: {e}")
