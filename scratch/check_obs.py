import os

obs_path = r"C:\Program Files\obs-studio\bin\64bit\obs64.exe"
if os.path.exists(obs_path):
    print("OBS Studio is installed at:", obs_path)
else:
    print("OBS Studio not found in default 64-bit path.")

obs_32_path = r"C:\Program Files (x86)\obs-studio\bin\32bit\obs32.exe"
if os.path.exists(obs_32_path):
    print("OBS Studio (32-bit) is installed at:", obs_32_path)

# Check if the OBS Virtual Camera registry key exists
import winreg
try:
    key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Classes\CLSID\{A3FCE0F5-3493-419F-958A-AB12111E94A2}")
    print("OBS Virtual Camera (DirectShow Filter) found in registry.")
    winreg.CloseKey(key)
except WindowsError:
    print("OBS Virtual Camera registry key not found.")
