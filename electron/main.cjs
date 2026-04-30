const { app, BrowserWindow, ipcMain, screen, dialog, powerSaveBlocker } = require('electron');
const { autoUpdater } = require('electron-updater');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const { pathToFileURL } = require('url');
const { spawn, spawnSync } = require('child_process');

// Auto-updater configuration
autoUpdater.autoDownload = false;
autoUpdater.autoInstallOnAppQuit = true;
// Set User-Agent to avoid 406 errors on some systems/proxies
autoUpdater.requestHeaders = { 'User-Agent': 'Media-Flow-Broadcast-Suite' };

let virtualCameraBridgeProcess = null;



app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
app.commandLine.appendSwitch('disable-background-timer-throttling');
app.commandLine.appendSwitch('disable-renderer-backgrounding');
app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
app.commandLine.appendSwitch('disable-features', 'CalculateNativeWinOcclusion');

let psbId = -1;

const isDev = !app.isPackaged;

let operatorWindow;
let audienceWindow;
let lastSyncStateMessage = null;
let lastBridgeStatus = { status: 'inactive' };


// Helper to scan for files
async function scanDir(dir, extensions, depth = 0) {
  if (depth > 5) return []; // Limit depth to prevent performance issues
  let results = [];
  try {
    const list = await fsPromises.readdir(dir, { withFileTypes: true });
    for (const file of list) {
      const fullPath = path.join(dir, file.name);
      if (file.isDirectory()) {
        const subResults = await scanDir(fullPath, extensions, depth + 1);
        results = results.concat(subResults);
      } else {
        const ext = path.extname(file.name).toLowerCase();
        if (extensions.includes(ext)) {
          results.push({
            name: file.name,
            path: fullPath,
            size: (await fsPromises.stat(fullPath)).size,
            type: ext.startsWith('.mp') ? (ext === '.mp3' ? 'audio' : 'video') : 'image'
          });
        }
      }
    }
  } catch (err) {
    // Skip directories that can't be read
  }
  return results;
}

function createOperatorWindow() {
  operatorWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: false, // Necessary for loading local video thumbnails in some cases, but protocol is better
      backgroundThrottling: false
    },
    title: 'MediaFlow - Operator'
  });

  if (isDev) {
    operatorWindow.loadURL('http://localhost:3000');
  } else {
    operatorWindow.loadFile(path.join(__dirname, '../dist/index.html'));
  }

  operatorWindow.webContents.on('did-finish-load', () => {
    operatorWindow.webContents.send('sync-message', { type: 'AUDIENCE_READY' });
  });
}

function sendCachedStateToWindow(win) {
  if (lastSyncStateMessage && win && !win.isDestroyed()) {
    win.webContents.send('sync-message', lastSyncStateMessage);
  }
}

function sendBridgeStatus(status) {
  lastBridgeStatus = status;
  if (operatorWindow && !operatorWindow.isDestroyed()) {
    operatorWindow.webContents.send('bridge-status', status);
  }
}

function getPythonCommand() {
  const candidates = [
    process.env.MEDIAFLOW_PYTHON,
    path.join(__dirname, '../resources/python/python.exe'),
    path.join(process.resourcesPath || '', 'python/python.exe'),
    path.join(process.env.LOCALAPPDATA || '', 'Programs/Python/Python312/python.exe'),
    path.join(process.env.LOCALAPPDATA || '', 'Programs/Python/Python311/python.exe'),
    path.join(process.env.LOCALAPPDATA || '', 'Programs/Python/Python310/python.exe'),
    'python',
    'python3'
  ].filter(Boolean);

  for (const candidate of candidates) {
    const lower = candidate.toLowerCase();
    if (lower.includes('\\windowsapps\\python')) continue;

    try {
      const result = spawnSync(candidate, ['--version'], {
        encoding: 'utf8',
        windowsHide: true,
        timeout: 5000
      });

      if (result.status === 0) {
        return candidate;
      }
    } catch (e) {}
  }

  return null;
}

function startBridge() {
  if (virtualCameraBridgeProcess) {
    sendBridgeStatus(lastBridgeStatus.status === 'inactive'
      ? { status: 'starting', message: 'Virtual camera bridge is already starting...' }
      : lastBridgeStatus
    );
    return;
  }

  const scriptPath = isDev 
    ? path.join(__dirname, '../resources/bridge/virtual_camera_bridge.py')
    : path.join(process.resourcesPath, 'bridge/virtual_camera_bridge.py');
  
  console.log(`Starting OBS Bridge: ${scriptPath}`);
  
  const pythonCmd = getPythonCommand();
  if (!pythonCmd) {
    sendBridgeStatus({
      status: 'error',
      message: 'Python runtime not found. Install Python 3 and bridge packages: numpy, pyvirtualcam, pywin32.'
    });
    return;
  }

  console.log(`Using Python runtime: ${pythonCmd}`);
  
  const bridgeProcess = spawn(pythonCmd, ['-u', scriptPath, '--target', 'obs']);
  virtualCameraBridgeProcess = bridgeProcess;
  
  sendBridgeStatus({ status: 'starting', message: `Initializing virtual camera with ${pythonCmd}` });

  let reportedError = false;

  bridgeProcess.stdout.on('data', (data) => {
    if (virtualCameraBridgeProcess !== bridgeProcess) return;

    const output = data.toString().trim();
    console.log(`[VirtualCam] ${output}`);
    
    if (output.includes('DEVICE_ACTIVE:')) {
      const deviceName = output.split('DEVICE_ACTIVE:')[1].trim();
      sendBridgeStatus({ status: 'active', device: deviceName });
    } else if (output.includes('FATAL_ERROR') || output.includes('ERROR')) {
      reportedError = true;
      sendBridgeStatus({ status: 'error', message: output });
    }
  });
  
  bridgeProcess.stderr.on('data', (data) => {
    if (virtualCameraBridgeProcess !== bridgeProcess) return;

    const error = data.toString().trim();
    console.error(`[VirtualCam Error] ${error}`);
    // Only send significant errors to UI
    if (error.includes('ERROR') || error.includes('Exception') || error.includes('Failed')) {
      reportedError = true;
      sendBridgeStatus({ status: 'error', message: error });
    }
  });

  bridgeProcess.on('error', (err) => {
    if (virtualCameraBridgeProcess !== bridgeProcess) return;

    console.error(`[VirtualCam Process Error] ${err.message}`);
    reportedError = true;
    sendBridgeStatus({ status: 'error', message: `Process failed to start: ${err.message}` });
  });

  bridgeProcess.on('exit', (code) => {
    if (virtualCameraBridgeProcess !== bridgeProcess) return;

    console.log(`[VirtualCam Process Exit] Code: ${code}`);
    virtualCameraBridgeProcess = null;
    if (!reportedError && audienceWindow && !audienceWindow.isDestroyed() && audienceWindow.getTitle() === 'MEDIAFLOW_NATIVE_BRIDGE_TARGET') {
      sendBridgeStatus({ status: 'starting', message: 'Virtual camera bridge stopped. Restarting...' });
      setTimeout(() => {
        if (!virtualCameraBridgeProcess && audienceWindow && !audienceWindow.isDestroyed() && audienceWindow.getTitle() === 'MEDIAFLOW_NATIVE_BRIDGE_TARGET') {
          startBridge();
        }
      }, 1000);
    } else if (!reportedError) {
      sendBridgeStatus({ status: 'inactive' });
    }
  });
}

function createAudienceWindow(viewType = 'audience') {
  const displays = screen.getAllDisplays();
  const externalDisplay = displays.find((display) => {
    return display.bounds.x !== 0 || display.bounds.y !== 0;
  });

  const isZoom = viewType === 'zoom';

  const createdWindow = new BrowserWindow({
    width: 1920, // High resolution broadcast
    height: 1080,
    x: isZoom ? 9999 : (externalDisplay ? externalDisplay.bounds.x : 100), 
    y: isZoom ? 9999 : (externalDisplay ? externalDisplay.bounds.y : 100),
    show: true, 
    useContentSize: isZoom,
    frame: !isZoom && !externalDisplay,
    fullscreen: !isZoom && !!externalDisplay,
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: false,
      backgroundThrottling: false,
      offscreen: false 
    },
    title: isZoom ? 'MEDIAFLOW_NATIVE_BRIDGE_TARGET' : 'MediaFlow - Audience',
    autoHideMenuBar: true,
    skipTaskbar: isZoom
  });
  audienceWindow = createdWindow;

  if (isZoom) {
    audienceWindow.setTitle('MEDIAFLOW_NATIVE_BRIDGE_TARGET');
  }

  // Prevent window from being throttled when hidden
  if (isZoom) {
    createdWindow.webContents.setBackgroundThrottling(false);
    startBridge();
  }

    createdWindow.on('closed', () => {
      if (audienceWindow === createdWindow) {
        audienceWindow = null;
      }
      if (isZoom && virtualCameraBridgeProcess) {
        virtualCameraBridgeProcess.kill();
        virtualCameraBridgeProcess = null;
        sendBridgeStatus({ status: 'inactive' });
      }
    });

    if (isDev) {
      createdWindow.loadURL(`http://localhost:3000?view=${viewType}`);
    } else {
      createdWindow.loadFile(path.join(__dirname, '../dist/index.html'), {
        query: { view: viewType }
      });
    }

    createdWindow.webContents.on('did-finish-load', () => {
      sendCachedStateToWindow(createdWindow);
      if (operatorWindow && !operatorWindow.isDestroyed()) {
        operatorWindow.webContents.send('sync-message', { type: 'AUDIENCE_READY' });
      }
    });
}

app.whenReady().then(() => {
  psbId = powerSaveBlocker.start('prevent-app-suspension');

  // Auto-grant camera and microphone permissions for ALL windows in this app.
  // This is critical for the zoom broadcast window which is positioned off-screen
  // (x:9999, y:9999) — the permission dialog would never be visible to the user.
  const { session } = require('electron');
  session.defaultSession.setPermissionRequestHandler((webContents, permission, callback) => {
    const allowedPermissions = ['media', 'camera', 'microphone', 'mediaKeySystem'];
    if (allowedPermissions.includes(permission)) {
      callback(true); // Auto-grant
    } else {
      callback(false);
    }
  });
  // Also handle the check handler (for already-granted permissions)
  session.defaultSession.setPermissionCheckHandler((webContents, permission) => {
    const allowedPermissions = ['media', 'camera', 'microphone'];
    return allowedPermissions.includes(permission);
  });

  createOperatorWindow();

  
  ipcMain.handle('getSystemSnapshot', async () => {
    return {
      memory: await process.getProcessMemoryInfo(),
      displays: screen.getAllDisplays(),
      platform: process.platform,
      versions: process.versions
    };
  });

  screen.on('display-added', () => {
    if (operatorWindow && !operatorWindow.isDestroyed()) {
      operatorWindow.webContents.send('displays-changed', screen.getAllDisplays());
    }
  });

  screen.on('display-removed', () => {
    if (operatorWindow && !operatorWindow.isDestroyed()) {
      operatorWindow.webContents.send('displays-changed', screen.getAllDisplays());
    }
  });

  ipcMain.handle('findJwDatabase', async () => {
    if (process.platform !== 'win32') return null;

    const localAppData = process.env.LOCALAPPDATA;
    // Common package name and the user's specific one
    const pkgs = [
      'WatchtowerBibleandTractSo.JWLibrary_5z594pnt97tep',
      'WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e'
    ];

    for (const pkg of pkgs) {
      const localState = path.join(localAppData, 'Packages', pkg, 'LocalState');
      const candidates = [
        path.join(localState, 'userData.db'),
        path.join(localState, 'Data', 'userData.db')
      ];

      for (const candidate of candidates) {
        try {
          await fsPromises.access(candidate);
          return candidate;
        } catch (e) {}
      }
    }
    return null;
  });

  ipcMain.handle('scanJwMedia', async (event, language, customPaths = []) => {
    const extensions = ['.mp4', '.m4v', '.mp3', '.jpg', '.png', '.jpeg', '.webp'];
    let allMedia = [];

    const roots = [];
    const home = app.getPath('home');

    if (process.platform === 'win32') {
      const localAppData = process.env.LOCALAPPDATA;
      
      // User specific package path
      const specificPackage = 'WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e';
      const alternativePackage = 'WatchtowerBibleandTractSo.JWLibrary_5z594pnt97tep';
      
      const packages = [specificPackage, alternativePackage];
      
      for (const pkg of packages) {
        const localState = path.join(localAppData, 'Packages', pkg, 'LocalState');
        // The core data folders
        roots.push(path.join(localState, 'Publications'));
        roots.push(path.join(localState, 'Data'));
        roots.push(path.join(localState, 'Data', 'Media'));
      }
      
      // The video/song folder requested by user
      roots.push(path.join(home, 'Videos', 'JWLibrary'));
    } else {
      // Mac/Linux fallbacks
      roots.push(path.join(home, 'Movies', 'JWLibrary'));
    }

    const scanRoots = [...new Set([...roots, ...customPaths])]; // Deduplicate
    
    for (const root of scanRoots) {
      try {
        await fsPromises.access(root);
        // Deep scan
        const results = await scanDir(root, extensions);
        allMedia = allMedia.concat(results);
      } catch (e) {}
    }

    // Filter by language if provided (e.g., _e.mp4, sjj_e, etc.)
    if (language) {
      const langSuffix = `_${language.toLowerCase()}`;
      allMedia = allMedia.filter(m => 
        m.name.toLowerCase().includes(langSuffix) || 
        m.name.toLowerCase().startsWith('sjj') // Songbook often matches pattern but check specifically
      );
    }

    // Deduplicate by path
    const uniqueMedia = Array.from(new Map(allMedia.map(item => [item.path, item])).values());
    return uniqueMedia;
  });

  ipcMain.handle('openExternalDisplay', (event, viewType) => {
    if (!audienceWindow || audienceWindow.isDestroyed()) {
      createAudienceWindow(viewType);
    } else {
      const isZoom = viewType === 'zoom';
      const displays = screen.getAllDisplays();
      const externalDisplay = displays.find(d => d.bounds.x !== 0 || d.bounds.y !== 0);

      if (isZoom && audienceWindow.getTitle() !== 'MEDIAFLOW_NATIVE_BRIDGE_TARGET') {
        audienceWindow.close();
        audienceWindow = null;
        createAudienceWindow('zoom');
        return;
      }

      if (isZoom) {
        startBridge();
        
        audienceWindow.setBounds({ x: 9999, y: 9999, width: 1920, height: 1080 });
        audienceWindow.setFullScreen(false);
        audienceWindow.setSkipTaskbar(true);
      } else {
        const targetBounds = externalDisplay ? externalDisplay.bounds : { x: 100, y: 100, width: 1920, height: 1080 };
        audienceWindow.setBounds(targetBounds);
        if (externalDisplay) audienceWindow.setFullScreen(true);
        audienceWindow.setSkipTaskbar(false);
        
        // Kill bridge if switching away from zoom
        if (virtualCameraBridgeProcess) {
          virtualCameraBridgeProcess.kill();
          virtualCameraBridgeProcess = null;
        }
      }

      audienceWindow.setTitle(isZoom ? 'MEDIAFLOW_NATIVE_BRIDGE_TARGET' : 'MediaFlow - Audience');
      
      // Update URL if type changed
      if (isDev) {
        audienceWindow.loadURL(`http://localhost:3000?view=${viewType || 'audience'}`);
      } else {
        audienceWindow.loadFile(path.join(__dirname, '../dist/index.html'), {
          query: { view: viewType || 'audience' }
        });
      }

      audienceWindow.show();
      if (!isZoom) {
        audienceWindow.focus();
      }
    }
  });

  ipcMain.on('sync-message', (event, message) => {
    if (message?.type === 'SYNC_STATE') {
      lastSyncStateMessage = message;
    }

    if ((message?.type === 'AUDIENCE_READY' || message?.type === 'AUDIENCE_ALIVE') && lastSyncStateMessage) {
      event.sender.send('sync-message', lastSyncStateMessage);
    }

    BrowserWindow.getAllWindows().forEach((win) => {
      if (!win.isDestroyed() && win.webContents.id !== event.sender.id) {
        win.webContents.send('sync-message', message);
      }
    });
  });

  ipcMain.handle('pickFiles', async (event, options) => {
    const dialogOptions = {
      properties: ['openFile', 'multiSelections'],
      filters: options?.filters || [
        { name: 'Media', extensions: ['mp4', 'm4v', 'mp3', 'jpg', 'png', 'jpeg', 'webp'] }
      ]
    };
    if (options?.defaultPath) {
      dialogOptions.defaultPath = options.defaultPath;
    }
    const result = await dialog.showOpenDialog(operatorWindow, dialogOptions);
    
    if (result.canceled) return [];
    
    return result.filePaths.map(fp => ({
      name: path.basename(fp),
      path: fp,
      type: path.extname(fp).substring(1)
    }));
  });

  ipcMain.handle('getVideosPath', () => {
    return app.getPath('videos');
  });

  ipcMain.handle('getPublicationsPath', () => {
    const localAppData = process.env.LOCALAPPDATA || path.join(app.getPath('home'), 'AppData', 'Local');
    return path.join(localAppData, 'Packages', 'WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e', 'LocalState', 'Publications');
  });

  ipcMain.handle('pickDirectory', async () => {
    const result = await dialog.showOpenDialog(operatorWindow, {
      properties: ['openDirectory']
    });
    if (result.canceled) return null;
    return result.filePaths[0];
  });



  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createOperatorWindow();
  });
});

// --- Update Logic ---
ipcMain.on('check-for-update', () => {
  if (app.isPackaged) {
    autoUpdater.checkForUpdates();
  } else {
    operatorWindow.webContents.send('update-status', { status: 'error', message: 'Update check only available in production.' });
  }
});

ipcMain.on('start-download', () => {
  autoUpdater.downloadUpdate();
});

ipcMain.on('update-now', () => {
  autoUpdater.quitAndInstall();
});

autoUpdater.on('checking-for-update', () => {
  operatorWindow.webContents.send('update-status', { status: 'checking' });
});

autoUpdater.on('update-available', (info) => {
  operatorWindow.webContents.send('update-status', { status: 'available', info });
});

autoUpdater.on('update-not-available', () => {
  operatorWindow.webContents.send('update-status', { status: 'not-available' });
});

autoUpdater.on('error', (err) => {
  operatorWindow.webContents.send('update-status', { status: 'error', message: err.message });
});

autoUpdater.on('download-progress', (progressObj) => {
  operatorWindow.webContents.send('update-status', { status: 'downloading', progress: progressObj.percent });
});

autoUpdater.on('update-downloaded', () => {
  operatorWindow.webContents.send('update-status', { status: 'downloaded' });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
