const { app, BrowserWindow, ipcMain, dialog, screen, protocol, powerSaveBlocker, net } = require('electron');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs').promises;
const { pathToFileURL } = require('url');



app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
app.commandLine.appendSwitch('disable-background-timer-throttling');
app.commandLine.appendSwitch('disable-renderer-backgrounding');
app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
app.commandLine.appendSwitch('disable-features', 'CalculateNativeWinOcclusion');

let psbId = -1;

const isDev = process.env.NODE_ENV !== 'production';

let operatorWindow;
let audienceWindow;
let broadcastPipe;

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

  const url = isDev 
    ? 'http://localhost:3000' 
    : `file://${path.join(__dirname, '../dist/index.html')}`;

  operatorWindow.loadURL(url);
}

function createAudienceWindow(viewType = 'audience') {
  const displays = screen.getAllDisplays();
  const externalDisplay = displays.find((display) => {
    return display.bounds.x !== 0 || display.bounds.y !== 0;
  });

  const isZoom = viewType === 'zoom';

  audienceWindow = new BrowserWindow({
    width: 1280, // Target broadcast resolution
    height: 720,
    x: isZoom ? -2000 : (externalDisplay ? externalDisplay.bounds.x : 100), // Hide zoom window off-screen if needed
    y: isZoom ? -2000 : (externalDisplay ? externalDisplay.bounds.y : 100),
    show: !isZoom, // Do not show the window if it's for Zoom broadcast
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: false,
      backgroundThrottling: false,
      offscreen: false // We need it on-screen (but hidden) to use standard CSS/rendering
    },
    title: isZoom ? 'Mediaflow cam' : 'MediaFlow - Audience',
    frame: !isZoom && !externalDisplay,
    fullscreen: !isZoom && !!externalDisplay,
    autoHideMenuBar: true,
    skipTaskbar: isZoom
  });

  // Prevent window from being throttled when hidden
  if (isZoom) {
    audienceWindow.webContents.setBackgroundThrottling(false);
  }

  const url = isDev 
    ? `http://localhost:3000?view=${viewType}` 
    : `file://${path.join(__dirname, '../dist/index.html')}?view=${viewType}`;

  audienceWindow.loadURL(url);
}

app.whenReady().then(() => {
  psbId = powerSaveBlocker.start('prevent-app-suspension');
  


  createOperatorWindow();
  
  ipcMain.handle('getSystemSnapshot', async () => {
    return {
      memory: process.getProcessMemoryInfo(),
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
      audienceWindow.setTitle(isZoom ? 'Mediaflow cam' : 'MediaFlow - Audience');
      
      // Update URL if type changed
      const url = isDev 
        ? `http://localhost:3000?view=${viewType || 'audience'}` 
        : `file://${path.join(__dirname, '../dist/index.html')}?view=${viewType || 'audience'}`;
      
      if (audienceWindow.getURL() !== url) {
        audienceWindow.loadURL(url);
      }

      if (!isZoom) {
        audienceWindow.show();
        audienceWindow.focus();
      }
    }
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

  ipcMain.on('startBroadcast', () => {
    if (broadcastPipe && !broadcastPipe.destroyed) return;
    
    const net = require('net');
    try {
      broadcastPipe = net.createConnection('\\\\.\\pipe\\UnityCapture', () => {
        console.log('Main: Connected to UnityCapture Virtual Camera Pipe');
      });

      broadcastPipe.on('error', (err) => {
        console.warn('Main: Virtual Camera Pipe not found. Ensure UnityCapture driver is installed and a consumer (like Zoom) is active.');
        broadcastPipe = null;
      });

      broadcastPipe.on('close', () => {
        broadcastPipe = null;
      });
    } catch (e) {
      console.error('Main: Failed to connect to pipe:', e);
    }
  });

  ipcMain.on('broadcastFrame', (event, buffer) => {
    if (broadcastPipe && !broadcastPipe.destroyed) {
      try {
        broadcastPipe.write(buffer);
      } catch (e) {
        broadcastPipe = null;
      }
    }
  });

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createOperatorWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
