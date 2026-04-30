import winreg

def list_dshow_devices():
    try:
        # DirectShow Category: CLSID_VideoInputDeviceCategory
        path = r"SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path) as key:
            i = 0
            while True:
                try:
                    name = winreg.EnumKey(key, i)
                    with winreg.OpenKey(key, name) as subkey:
                        friendly_name, _ = winreg.QueryValueEx(subkey, "FriendlyName")
                        print(f"Found Device: {friendly_name}")
                    i += 1
                except WindowsError:
                    break
    except WindowsError:
        print("Could not access DirectShow registry category.")

list_dshow_devices()
