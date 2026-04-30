const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('mediaflow', {
  getSystemSnapshot: () => ipcRenderer.invoke('getSystemSnapshot'),
  findJwDatabase: () => ipcRenderer.invoke('findJwDatabase'),
  extractJwLibraryData: (dbPath) => ipcRenderer.invoke('extractJwLibraryData', dbPath),
  scanJwMedia: (language, customPaths) => ipcRenderer.invoke('scanJwMedia', language, customPaths),
  openExternalDisplay: (windowId, options) => ipcRenderer.invoke('openExternalDisplay', windowId, options),
  openAudienceDisplay: () => ipcRenderer.invoke('openAudienceDisplay'),
  openZoomDisplay: () => ipcRenderer.invoke('openZoomDisplay'),
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
  onBridgeStatus: (callback) => {
    const listener = (event, data) => callback(data);
    ipcRenderer.on('bridge-status', listener);
    return () => ipcRenderer.removeListener('bridge-status', listener);
  },
  onUpdateStatus: (callback) => {
    const listener = (event, data) => callback(data);
    ipcRenderer.on('update-status', listener);
    return () => ipcRenderer.removeListener('update-status', listener);
  },
  onSyncView: (callback) => {
    const listener = (event, viewType) => callback(viewType);
    ipcRenderer.on('sync-view', listener);
    return () => ipcRenderer.removeListener('sync-view', listener);
  },
  sendSyncMessage: (message) => ipcRenderer.send('sync-message', message),
  onSyncMessage: (callback) => {
    const listener = (event, message) => callback(message);
    ipcRenderer.on('sync-message', listener);
    return () => ipcRenderer.removeListener('sync-message', listener);
  }
});
