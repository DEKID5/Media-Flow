const syncHandlers = new Set();
const viewHandlers = new Set();

window.mediaflow = {
  isDesktop: true,
  onSyncMessage(callback) {
    syncHandlers.add(callback);
    return () => syncHandlers.delete(callback);
  },
  sendSyncMessage() {},
  onSyncView(callback) {
    viewHandlers.add(callback);
    return () => viewHandlers.delete(callback);
  },
};

window.__pushMediaflowSync = (message) => {
  syncHandlers.forEach(callback => callback(message));
};

window.__pushMediaflowView = (view) => {
  viewHandlers.forEach(callback => callback(view));
};
