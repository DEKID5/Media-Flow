# Build Notes

## Windows Desktop Installer

The Windows desktop build bundles an embedded Python runtime for the Zoom/OBS bridge.

```powershell
npm run build:desktop
```

`build:desktop` runs `npm run bridge:python` first. That script downloads the official Python embeddable runtime, installs the bridge packages from `resources/bridge/requirements.txt`, verifies imports, and places the finished runtime in `resources/python`.

The generated installer includes:

- `resources/bridge/virtual_camera_bridge.py`
- `resources/bridge/requirements.txt`
- `resources/python/python.exe`
- Python packages required by the bridge: `numpy`, `pyvirtualcam`, `pywin32`

Users still need OBS Studio with OBS Virtual Camera available, but they do not need to install Python manually.
