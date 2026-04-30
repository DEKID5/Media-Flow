import pyvirtualcam

# Pyvirtualcam doesn't have a direct "list all" function in the API, 
# but it iterates through them. 
# Let's try to initialize by name.

print("Testing 'OBS Virtual Camera'...")
try:
    with pyvirtualcam.Camera(width=1280, height=720, fps=30, device="OBS Virtual Camera") as cam:
        print(f"Success: {cam.device}")
except Exception as e:
    print(f"Failed to find OBS Virtual Camera: {e}")

print("\nTesting 'Unity Video Capture'...")
try:
    with pyvirtualcam.Camera(width=1280, height=720, fps=30, device="Unity Video Capture") as cam:
        print(f"Success: {cam.device}")
except Exception as e:
    print(f"Failed to find Unity Video Capture: {e}")
