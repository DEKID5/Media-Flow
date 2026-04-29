import { useEffect, useRef, useCallback } from 'react';
import { SyncMessage, AppState } from '../types';

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
    
    // Fallback: localStorage can only store strings, will lose FileHandles
    try {
      // Check if message contains non-serializable objects (like FileSystemHandle)
      // If it's a SYNC_STATE message, it's likely to contain file handles
      const serializableMsg = JSON.parse(JSON.stringify(message, (key, value) => {
        if (typeof FileSystemHandle !== 'undefined' && value instanceof FileSystemHandle) return '[FileSystemHandle]';
        return value;
      }));
      
      localStorage.setItem('mediaflow_sync_msg', JSON.stringify(serializableMsg));
      localStorage.setItem('mediaflow_sync_at', Date.now().toString());
    } catch (e) {
      // console.warn('Sync fallback failed or truncated:', e);
    }
  }, []);

  return { send };
}
