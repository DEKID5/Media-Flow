import { useState, useEffect, useRef, useMemo } from 'react';
import { MediaAsset, SyncMessage } from '../types';
import { Music, Camera, Video } from 'lucide-react';
import { motion } from 'motion/react';

export interface SmartMediaProps {
  asset: MediaAsset | any;
  className?: string;
  autoPlay?: boolean;
  muted?: boolean;
  volume?: number; // 0 to 100
  controls?: boolean;
  seekTo?: number; // timestamp to seek to
  id?: string;
  onEnd?: () => void;
  onError?: (e: any) => void;
  isThumbnail?: boolean;
  deviceId?: string;
  isZoomView?: boolean; // When true, do NOT mirror the camera (bridge capture is not flipped)
  channelOneOutput?: boolean;
}

const CAMERA_ICON = "https://images.unsplash.com/photo-1516035069371-29a1b244cc32?q=80&w=1000&auto=format&fit=crop";

export function SmartMedia({ 
  asset, 
  className, 
  autoPlay, 
  muted, 
  volume,
  controls, 
  seekTo,
  id,
  onEnd, 
  onError, 
  isThumbnail,
  deviceId,
  isZoomView = false,
  channelOneOutput = false
}: SmartMediaProps) {
  const [url, setUrl] = useState<string | null>(null);
  const isCameraAsset = asset?.type === 'camera';
  const assetType = asset?.type?.toLowerCase() || '';
  const isVideoAsset = isCameraAsset || ['video', 'mp4', 'mov', 'avi', 'mkv', 'm4v', 'webm', '3gp', 'mpg', 'mpeg', 'wmv'].includes(assetType) || !!asset?.path?.match(/\.(mp4|webm|mov|m4v|3gp|mpg|mpeg|wmv|mkv|avi)$/i);
  const thumbnailPlaceholderRef = useRef<HTMLDivElement>(null);
  const [thumbnailVisible, setThumbnailVisible] = useState(!isThumbnail || !isVideoAsset);

  useEffect(() => {
    if (!isCameraAsset) setUrl(null);
    setThumbnailVisible(!isThumbnail || !isVideoAsset);
  }, [asset?.id, isCameraAsset, isThumbnail, isVideoAsset]);

  useEffect(() => {
    if (!isThumbnail || !isVideoAsset || thumbnailVisible) return;

    const node = thumbnailPlaceholderRef.current;
    if (!node || typeof IntersectionObserver === 'undefined') {
      setThumbnailVisible(true);
      return;
    }

    const observer = new IntersectionObserver(([entry]) => {
      if (entry.isIntersecting) {
        setThumbnailVisible(true);
        observer.disconnect();
      }
    }, { rootMargin: '200px' });

    observer.observe(node);
    return () => observer.disconnect();
  }, [isThumbnail, isVideoAsset, thumbnailVisible]);

  const normalizedUrl = useMemo(() => {
    if (!url) return null;
    if (url.startsWith('blob:') || url.startsWith('http:') || url.startsWith('https:') || url.startsWith('file:')) return url;
    if (url.includes(':/') || url.includes(':\\') || url.startsWith('/')) {
       return `file:///${url.replace(/\\/g, '/')}`;
    }
    return url;
  }, [url]);
  const videoRef = useRef<HTMLVideoElement>(null);
  const audioRef = useRef<HTMLAudioElement>(null);
  const [cameraStream, setCameraStream] = useState<MediaStream | null>(null);
  const audioGraphRef = useRef<{
    context: AudioContext;
    source: MediaElementAudioSourceNode;
    splitter: ChannelSplitterNode;
    merger: ChannelMergerNode;
    gain: GainNode;
  } | null>(null);

  useEffect(() => {
    if (isCameraAsset && !isThumbnail) {
      let active = true;
      let activeStream: MediaStream | null = null;
      const videoQuality = {
        width: { min: 1280, ideal: 1920 },
        height: { min: 720, ideal: 1080 },
        frameRate: { ideal: 30 }
      };
      const constraints = {
        video: deviceId
          ? { deviceId: { exact: deviceId }, ...videoQuality }
          : videoQuality,
        audio: false
      };
      navigator.mediaDevices.getUserMedia(constraints)
        .then(stream => {
          if (active) {
            activeStream = stream;
            setCameraStream(stream);
            if (videoRef.current) {
              videoRef.current.srcObject = stream;
              if (autoPlay) {
                videoRef.current.play().catch(e => {
                  if (e.name !== 'AbortError') console.warn('SmartMedia: Camera play failed', e);
                });
              }
            }
          } else {
            stream.getTracks().forEach(track => track.stop());
          }
        })
        .catch(err => console.error("Camera access failed:", err));
      
      return () => {
        active = false;
        activeStream?.getTracks().forEach(track => track.stop());
        if (videoRef.current) videoRef.current.srcObject = null;
      };
    }
  }, [isCameraAsset, asset?.id, isThumbnail, deviceId]);

  // Sync volume, muted and playback state
  useEffect(() => {
    const el = videoRef.current || audioRef.current;
    if (el) {
      if (channelOneOutput && audioGraphRef.current) {
        el.volume = 1;
        el.muted = false;
      } else {
        el.volume = (volume ?? 100) / 100;
        el.muted = muted ?? false;
      }
    }
  }, [channelOneOutput, volume, muted, url]);

  useEffect(() => {
    const el = videoRef.current || audioRef.current;
    if (el && !isThumbnail) {
      if (autoPlay) {
        el.play().catch(e => {
          if (e.name !== 'AbortError') console.warn('SmartMedia: Play failed', e);
        });
      } else {
        el.pause();
      }
    }
  }, [autoPlay, url, isThumbnail, cameraStream]);

  // Handle Seeking
  useEffect(() => {
    const el = videoRef.current || audioRef.current;
    if (el && seekTo !== undefined) {
      el.currentTime = seekTo;
    }
  }, [seekTo, url]);

  useEffect(() => {
    const el = videoRef.current || audioRef.current;
    if (!el || isThumbnail || isCameraAsset) return;
    if (typeof window === 'undefined' || typeof window.AudioContext === 'undefined') return;
    if (!channelOneOutput) return;

    let cancelled = false;

    const ensureChannelOneOutput = async () => {
      try {
        if (!audioGraphRef.current) {
          const context = new window.AudioContext();
          const source = context.createMediaElementSource(el);
          const splitter = context.createChannelSplitter(2);
          const merger = context.createChannelMerger(2);
          const gain = context.createGain();

          source.connect(splitter);
          splitter.connect(merger, 0, 0);
          splitter.connect(merger, 0, 1);
          merger.connect(gain);
          gain.connect(context.destination);
          gain.gain.value = muted ? 0 : (volume ?? 100) / 100;

          audioGraphRef.current = { context, source, splitter, merger, gain };
          el.volume = 1;
          el.muted = false;
        }

        const graph = audioGraphRef.current;
        if (!graph || cancelled) return;

        if (graph.context.state === 'suspended' && autoPlay) {
          await graph.context.resume();
        }
      } catch (error) {
        console.warn('SmartMedia: channel-one audio routing failed', error);
      }
    };

    void ensureChannelOneOutput();

    return () => {
      cancelled = true;
      if (audioGraphRef.current) {
        audioGraphRef.current.source.disconnect();
        audioGraphRef.current.splitter.disconnect();
        audioGraphRef.current.merger.disconnect();
        audioGraphRef.current.gain.disconnect();
        audioGraphRef.current.context.close().catch(() => {});
        audioGraphRef.current = null;
      }
    };
  }, [asset?.id, channelOneOutput, isCameraAsset, isThumbnail, url]);

  useEffect(() => {
    const graph = audioGraphRef.current;
    if (!graph || !channelOneOutput) return;

    const nextGain = muted ? 0 : (volume ?? 100) / 100;
    graph.gain.gain.setTargetAtTime(nextGain, graph.context.currentTime, 0.015);
    if (graph.context.state === 'suspended' && autoPlay) {
      graph.context.resume().catch(() => {});
    }
  }, [autoPlay, channelOneOutput, muted, volume]);

  useEffect(() => {
    if (!asset) return;
    if (isThumbnail && isVideoAsset && !thumbnailVisible) return;
    let active = true;
    let currentUrl: string | null = null;

    const resolve = async () => {
      if (isCameraAsset) {
        if (active) setUrl(null);
        return;
      }
      if (active) setUrl(null);
      try {
        const isDesktop = !!(window as any).mediaflow?.isDesktop;

        // Try to use the Global API if available (for Audience View on same-origin)
        if (typeof window !== 'undefined' && !isDesktop) {
          let api = null;
          try {
            api = (window.opener as any)?.__MEDIAFLOW_API__ || (window.parent as any)?.__MEDIAFLOW_API__;
          } catch (e) {
            // Context is likely cross-origin
          }
          
          if (api && typeof api.resolveAssetUrlById === 'function') {
            try {
              const result = await api.resolveAssetUrlById(asset.id);
              if (result && active) {
                if (result.file) {
                  currentUrl = URL.createObjectURL(result.file);
                  setUrl(currentUrl);
                } else if (result.url) {
                  setUrl(result.url);
                }
                return;
              }
            } catch (e) {
              console.warn('SmartMedia: API resolution failed', e);
            }
          }

          // ROBUST FALLBACK: Use BroadcastChannel for all cases (especially cross-origin frame layouts)
          const channel = new BroadcastChannel('mediaflow_sync');
          const resolutionPromise = new Promise<{url: string | null, file: File | null}>((resolve) => {
            const timeout = setTimeout(() => {
              channel.removeEventListener('message', handleMsg);
              resolve({ url: null, file: null });
            }, 5000);

            const handleMsg = (event: MessageEvent) => {
              const msg = event.data;
              if (msg.type === 'ASSET_RESOLVED' && msg.assetId === asset.id) {
                clearTimeout(timeout);
                channel.removeEventListener('message', handleMsg);
                resolve({ url: msg.url || null, file: msg.file || null });
              }
            };

            channel.addEventListener('message', handleMsg);
            channel.postMessage({ type: 'RESOLVE_ASSET', assetId: asset.id });
          });

          const res = await resolutionPromise;
          channel.close();

          if (res.file && active) {
            currentUrl = URL.createObjectURL(res.file);
            setUrl(currentUrl);
            return;
          } else if (res.url && active) {
            setUrl(res.url);
            return;
          }
        }

        // Handle FileHandle (Web or Desktop fallback)
        if (asset.fileHandle) {
          try {
            const handle = asset.fileHandle as FileSystemFileHandle;
            const status = typeof (handle as any).queryPermission === 'function' 
              ? await (handle as any).queryPermission({ mode: 'read' }) 
              : 'granted';
            if (status !== 'granted') {
              console.warn('SmartMedia: Permission not granted for handle', asset.name);
              if (active) setUrl('__PERMISSION__');
              return;
            }

            const file = await handle.getFile();
            currentUrl = URL.createObjectURL(file);
            console.log('SmartMedia: Resolved local blob URL for', asset.name);
            if (active) setUrl(currentUrl);
            return;
          } catch (e) {
            console.warn('SmartMedia: Local file handle resolution failed', e);
          }
        } 
        
        if (asset.url) {
          if (active) setUrl(asset.url);
        }
        else if (asset.path) {
          let p = asset.path;
          if (isDesktop && (p.includes(':') || p.startsWith('/') || p.startsWith('\\'))) {
            const normalizedPath = p.replace(/\\/g, '/');
            p = `file:///${normalizedPath.startsWith('/') ? '' : '/'}${normalizedPath}`;
          }
          if (active) setUrl(p);
        } else {
          if (active) setUrl('__ERROR__');
        }
      } catch (err) {
        console.error('SmartMedia: Failed to resolve media:', err, asset);
        if (active) {
          setUrl('__ERROR__');
          if (onError) onError(err);
        }
      }
    };

    resolve();

    return () => {
      active = false;
      // ONLY revoke if we are sure this URL is no longer needed. 
      // For blobs, we'll let them persist a bit longer or avoid revoking if we suspect re-renders.
      if (currentUrl?.startsWith('blob:')) {
        const toRevoke = currentUrl;
        // Increase timeout to be safe against double-renders in Dev
        setTimeout(() => {
          try { URL.revokeObjectURL(toRevoke); } catch (e) {}
        }, 5000);
      }
    };
  }, [asset?.id, asset?.fileHandle, asset?.path, asset?.url, isCameraAsset, isThumbnail, isVideoAsset, thumbnailVisible]);

  // Force thumbnail seek
  useEffect(() => {
    if (isThumbnail && videoRef.current && url) {
      const v = videoRef.current;
      const handleSeek = () => {
        if (v.duration >= 15) {
          v.currentTime = 15.0; 
        } else if (v.duration >= 5) {
          v.currentTime = 5.0;
        } else if (v.duration > 0) {
          v.currentTime = v.duration / 2;
        }
      };

      if (v.readyState >= 2) { 
        handleSeek();
      } else {
        v.onloadeddata = () => {
          handleSeek();
          v.onloadeddata = null;
        };
      }
    }
  }, [isThumbnail, url]);

  if (!asset) return null;

  if (isThumbnail && isVideoAsset && !thumbnailVisible) {
    return (
      <div ref={thumbnailPlaceholderRef} className={`relative overflow-hidden bg-zinc-950 flex items-center justify-center ${className}`}>
        <div className="w-9 h-9 rounded-full bg-white/10 border border-white/10 flex items-center justify-center">
          <Video size={14} className="text-white/40" />
        </div>
      </div>
    );
  }

  if (!isCameraAsset && (!url || url === '__ERROR__' || url === '__PERMISSION__')) {
    return (
      <div className={`flex flex-col items-center justify-center gap-4 ${className} bg-white/5 ${url === null ? 'animate-pulse' : ''}`}>
        {url === null ? (
          <>
            <div className="w-8 h-8 rounded-full border-2 border-t-blue-500 border-blue-500/10 animate-spin" />
            <span className="text-[10px] font-mono text-white/20 uppercase tracking-widest">Resolving Asset...</span>
          </>
        ) : url === '__PERMISSION__' ? (
          <div className="text-center p-4">
             <p className="text-[9px] font-black text-yellow-500 uppercase tracking-widest mb-1">Permission Required</p>
             <p className="text-[7px] text-white/20 uppercase tracking-widest">Rescan Folder</p>
          </div>
        ) : asset.type === 'camera' ? (
          <div className="flex flex-col items-center gap-6 opacity-40">
            <div className="relative">
              <div className="w-16 h-16 rounded-full border border-white/5 border-t-blue-500/40 animate-spin" />
              <div className="absolute inset-0 flex items-center justify-center">
                <Camera size={24} className="text-white/20 animate-pulse" />
              </div>
            </div>
            <div className="flex flex-col items-center gap-1 text-center">
              <p className="text-[10px] font-mono text-white/40 uppercase tracking-[0.5em] font-bold">Camera Standby</p>
              <p className="text-[8px] text-white/10 font-mono uppercase tracking-[0.3em]">Check Settings or Input Source</p>
            </div>
          </div>
        ) : (
          <div className="text-center p-8 bg-black/40 rounded-3xl border border-red-500/10 max-w-xs">
             <p className="text-xs font-mono text-red-500 font-bold uppercase tracking-widest mb-2">Media Resolution Error</p>
             <p className="text-[10px] text-white/40 uppercase tracking-widest leading-relaxed">
               The asset <span className="text-white/60 font-mono">"{asset.name}"</span> could not be resolved from the host.
             </p>
          </div>
        )}
      </div>
    );
  }



  const isBlob = url?.startsWith('blob:') ?? false;

  const t = asset.type?.toLowerCase();
  const isVideo = isVideoAsset;
  const isAudio = ['audio', 'mp3', 'm4a', 'wav', 'ogg', 'aac', 'flac', 'm4p', 'opus', 'mp4a'].includes(t) || asset.path?.match(/\.(mp3|wav|m4a|ogg|aac|flac|m4p|opus|mp4a)$/i);
  const isImage = ['image', 'jpg', 'jpeg', 'png', 'gif', 'webp', 'svg', 'bmp', 'ico', 'tiff'].includes(t) || asset.path?.match(/\.(jpg|jpeg|png|gif|webp|svg|bmp|ico|tiff)$/i);

  if (asset.type === 'camera' && isThumbnail) {
    return (
      <div className={`relative overflow-hidden bg-zinc-900 flex items-center justify-center ${className}`}>
        <img src={CAMERA_ICON} className="absolute inset-0 w-full h-full object-cover opacity-30 grayscale" alt="Camera" />
        <Camera className="text-white/40 relative z-10" size={16} />
      </div>
    );
  }

  if (isImage) {
    return (
      <img 
        src={normalizedUrl || ''} 
        className={className} 
        alt={asset.name}
        onError={onError}
        loading="lazy"
      />
    );
  }

  if (isVideo) {
    const videoSrc = asset.type === 'camera' ? undefined : (normalizedUrl || '') + (isThumbnail && !url?.includes('#t=') ? '#t=15.0' : '');
    // Mirror the camera ONLY for operator preview — NOT for the zoom broadcast view
    // (the bridge captures the window as-is; mirroring here would make text appear backwards in Zoom)
    const cameraMirrorClass = '';
    return (
      <video 
        ref={videoRef}
        id={id}
        src={videoSrc} 
        className={`${className} ${cameraMirrorClass}`}
        autoPlay={isThumbnail ? false : autoPlay} 
        muted={isThumbnail ? true : (asset.type === 'camera' ? true : (channelOneOutput ? false : muted))}
        controls={asset.type === 'camera' ? false : controls}
        controlsList="nodownload noplaybackrate noremoteplayback"
        disableRemotePlayback
        onEnded={onEnd}
        onError={onError}
        onLoadedMetadata={(e) => {
          if (isThumbnail) {
            e.currentTarget.currentTime = e.currentTarget.duration >= 15 ? 15.0 : 1.0; 
          }
        }}
        onSeeked={(e) => {
          if (isThumbnail) e.currentTarget.pause();
        }}
        preload={isThumbnail ? 'metadata' : 'auto'}
        playsInline
        disablePictureInPicture
      />
    );
  }

  if (isAudio) {
    if (isThumbnail) {
      return (
        <div className={`flex items-center justify-center bg-gradient-to-br from-blue-600/20 to-indigo-900/40 border border-white/5 shadow-inner ${className}`}>
          <Music size={18} className="text-blue-400/60 drop-shadow-lg" />
        </div>
      );
    }

     // Special styling for Audio in Audience View
     if (className?.includes('audience-audio')) {
        return (
          <div className="flex flex-col items-center gap-12">
            <motion.div 
              animate={{ scale: [1, 1.05, 1] }} 
              transition={{ repeat: Infinity, duration: 2 }}
              className="w-48 h-48 rounded-full border-4 border-blue-500/20 flex items-center justify-center bg-blue-500/5 shadow-2xl shadow-blue-500/10"
            >
              <Music size={80} className="text-blue-500" />
              <audio 
                ref={audioRef}
                id={id}
                src={normalizedUrl || ''}
                autoPlay={autoPlay}
                muted={channelOneOutput ? false : muted}
                onEnded={onEnd}
                onError={onError}
                preload="auto"
              />
            </motion.div>
            <div>
              <h2 className="text-5xl font-bold text-white tracking-tighter mb-2">{asset.name}</h2>
              <p className="text-blue-400 font-mono text-sm uppercase tracking-[0.5em] opacity-60">Now Playing</p>
            </div>
          </div>
        );
     }

    return (
      <audio 
        ref={audioRef}
        id={id}
        src={normalizedUrl || ''} 
        autoPlay={autoPlay} 
        muted={channelOneOutput ? false : muted}
        onEnded={onEnd}
        onError={onError}
        preload="auto"
      />
    );
  }

  return <div className="text-white/20 text-xs">Unsupported Format</div>;
}
