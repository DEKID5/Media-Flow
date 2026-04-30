import { useEffect, useRef, useCallback } from 'react';
import { SyncMessage } from '../types';

const makeSerializable = (message: SyncMessage): SyncMessage => {
  return JSON.parse(JSON.stringify(message, (key, value) => {
    if (typeof FileSystemHandle !== 'undefined' && value instanceof FileSystemHandle) return '[FileSystemHandle]';
    return value;
  }));
};

export function useSyncChannel(onMessage?: (msg: SyncMessage) => void) {
  const channelRef = useRef<BroadcastChannel | null>(null);
  const handlerRef = useRef(onMessage);

  useEffect(() => {
    handlerRef.current = onMessage;
  }, [onMessage]);

  useEffect(() => {
    const handleMessage = (data: SyncMessage) => {
      if (handlerRef.current) handlerRef.current(data);
    };

    if (typeof BroadcastChannel !== 'undefined') {
      const channel = new BroadcastChannel('mediaflow_sync');
      channelRef.current = channel;
  
      channel.onmessage = (event) => {
        handleMessage(event.data);
      };
    }

    const cleanupElectronSync =
      typeof (window as any).mediaflow?.onSyncMessage === 'function'
        ? (window as any).mediaflow.onSyncMessage(handleMessage)
        : undefined;

    // Fallback: localStorage for cross-tab sync
    const storageHandler = (e: StorageEvent) => {
      if (e.key === 'mediaflow_sync_msg' && e.newValue) {
        try {
          const msg = JSON.parse(e.newValue);
          handleMessage(msg);
        } catch (err) {
          // Ignore
        }
      }
    };

    window.addEventListener('storage', storageHandler);

    return () => {
      if (channelRef.current) {
        channelRef.current.close();
        channelRef.current = null;
      }
      if (typeof cleanupElectronSync === 'function') cleanupElectronSync();
      window.removeEventListener('storage', storageHandler);
    };
  }, []);

  const send = useCallback((message: SyncMessage) => {
    // BroadcastChannel is preferred and supports FileHandles
    try {
      channelRef.current?.postMessage(message);
    } catch (err) {
      console.warn('BroadcastChannel sync failed, falling back to storage:', err);
    }

    try {
      if (typeof (window as any).mediaflow?.sendSyncMessage === 'function') {
        (window as any).mediaflow.sendSyncMessage(makeSerializable(message));
      }
    } catch (err) {
      console.warn('Electron sync relay failed:', err);
    }
    
    // Fallback: localStorage can only store strings, will lose FileHandles
    try {
      const serializableMsg = makeSerializable(message);
      localStorage.setItem('mediaflow_sync_msg', JSON.stringify(serializableMsg));
      localStorage.setItem('mediaflow_sync_at', Date.now().toString());
    } catch (e) {
      // console.warn('Sync fallback failed or truncated:', e);
    }
  }, []);

  return { send };
}
