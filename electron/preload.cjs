const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('mediaflow', {
  getSystemSnapshot: () => ipcRenderer.invoke('getSystemSnapshot'),
  findJwDatabase: () => ipcRenderer.invoke('findJwDatabase'),
  extractJwLibraryData: (dbPath) => ipcRenderer.invoke('extractJwLibraryData', dbPath),
  scanJwMedia: (language, customPaths) => ipcRenderer.invoke('scanJwMedia', language, customPaths),
  openExternalDisplay: (windowId, options) => ipcRenderer.invoke('openExternalDisplay', windowId, options),
  playMedia: (filePath, options) => ipcRenderer.invoke('playMedia', filePath, options),
  setOutputDevice: (name) => ipcRenderer.invoke('setOutputDevice', name),
  pickFiles: (options) => ipcRenderer.invoke('pickFiles', options),
  pickDirectory: () => ipcRenderer.invoke('pickDirectory'),
  getVideosPath: () => ipcRenderer.invoke('getVideosPath'),
  getPublicationsPath: () => ipcRenderer.invoke('getPublicationsPath'),
  onDisplayChange: (callback) => {
    const listener = (event, displays) => callback(displays);
    ipcRenderer.on('displays-changed', listener);
    return () => ipcRenderer.removeListener('displays-changed', listener);
  },
  isDesktop: true,
  startBroadcast: () => ipcRenderer.send('startBroadcast'),
  broadcastFrame: (buffer) => ipcRenderer.send('broadcastFrame', buffer),
  checkForUpdate: () => ipcRenderer.send('check-for-update'),
  downloadUpdate: () => ipcRenderer.send('start-download'),
  installUpdate: () => ipcRenderer.send('update-now'),
  onUpdateStatus: (callback) => {
    const listener = (event, data) => callback(data);
    ipcRenderer.on('update-status', listener);
    return () => ipcRenderer.removeListener('update-status', listener);
  }
});
