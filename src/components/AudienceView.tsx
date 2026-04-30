import { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { AppState } from '../types';
import { useSyncChannel } from '../hooks/useSyncChannel';
import { Video, Music, Image as ImageIcon, Monitor, Clock, FolderOpen, Play } from 'lucide-react';
import { SmartMedia } from './SmartMedia';

// (SmartMedia moved to its own file)

export function AudienceView() {
  const [state, setState] = useState<AppState | null>(null);
  const [permissionError, setPermissionError] = useState<boolean>(false);
  const [retryCount, setRetryCount] = useState(0);
  const [hasInteracted, setHasInteracted] = useState(!!((window as any).mediaflow?.isDesktop));

  const { send } = useSyncChannel((msg) => {
    console.log('AudienceView: Received message', msg.type);
    if (msg.type === 'SYNC_STATE') {
      console.log('AudienceView: State synced. Program Asset:', msg.state.programAsset?.name);
      setState(msg.state);
      setPermissionError(false); 
      setRetryCount(c => c + 1);
    } else if (msg.type === 'BGM_ACTION' && msg.action === 'seek') {
      const player = document.getElementById('bgm-audience-player') as HTMLMediaElement;
      if (player) {
        player.currentTime = Math.max(0, player.currentTime + msg.offset);
      }
    }
  });

  const searchParams = new URLSearchParams(window.location.search);
  const view = searchParams.get('view');

  // --- Virtual Camera Broadcast Logic ---
  useEffect(() => {
    // The previous UnityCapture named pipe logic has been removed because it relied on Shared Memory
    // which Node.js cannot write to. 
    // Instead, the MediaFlow Audience window is now rendered off-screen (x: -20000).
    // The user will use OBS Studio's "Window Capture" to grab this off-screen window natively 
    // and route it to the "OBS Virtual Camera" for Zoom. This is zero-copy and highly optimized!
  }, [view]);
  // ---------------------------------------

  const handleMediaError = (e: any) => {
    console.error('AudienceView: Media error caught:', e);
    setPermissionError(true);
  };

  useEffect(() => {
    // Initial request
    send({ type: 'AUDIENCE_READY' });

    // Send heartbeat every 4 seconds (less aggressive)
    const interval = setInterval(() => {
      send({ type: 'AUDIENCE_ALIVE' });
      if (!state) {
        send({ type: 'AUDIENCE_READY' });
      }
    }, 4000);

    return () => clearInterval(interval);
  }, [send, state]);

  // Click handler to enable audio context
  const handleInteraction = () => {
    setHasInteracted(true);
  };

  if (!state) {
    return (
      <div className="h-screen w-screen bg-black flex flex-col items-center justify-center gap-8 overflow-hidden cursor-pointer" onClick={handleInteraction}>
        <div className="relative">
          <div className="w-24 h-24 rounded-full border border-white/5 animate-[pingslow_3s_infinite]" />
          <div className="absolute inset-0 flex items-center justify-center">
            <div className="w-12 h-12 rounded-full border-2 border-t-white/40 border-white/5 animate-spin" />
          </div>
        </div>
        <div className="flex flex-col items-center gap-6">
          <h1 className="text-white/40 font-mono text-[10px] uppercase tracking-[0.8em] animate-pulse">Waiting for Signal</h1>
          <p className="text-[10px] text-white/10 font-mono uppercase tracking-[0.4em]">Please keep the Operator Dashboard open</p>
          <button 
            onClick={() => {
              send({ type: 'AUDIENCE_READY' });
              setRetryCount(c => c + 1);
            }}
            className="mt-4 px-4 py-2 bg-white/5 hover:bg-white/10 text-white/20 hover:text-white/40 border border-white/10 rounded-lg text-[10px] font-mono uppercase tracking-widest transition-all"
          >
            Force Sync Request
          </button>
        </div>

        <style>{`
          @keyframes pingslow {
            0% { transform: scale(1); opacity: 0.5; }
            100% { transform: scale(3); opacity: 0; }
          }
        `}</style>
      </div>
    );
  }

  // Interaction overlay for audio
  if (!hasInteracted) {
    return (
      <div 
        className="h-screen w-screen bg-[#020205] flex flex-col items-center justify-center gap-12 cursor-pointer group relative overflow-hidden"
        onClick={handleInteraction}
      >
        {/* Animated background hint */}
        <div className="absolute inset-0 opacity-10">
          <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[1000px] h-[1000px] bg-blue-500/5 rounded-full blur-[160px] animate-pulse" />
        </div>

        <motion.div 
          initial={{ opacity: 0, scale: 0.95 }}
          animate={{ opacity: 1, scale: 1 }}
          className="flex flex-col items-center gap-10 relative z-10"
        >
          <div className="relative">
            <div className="w-48 h-48 rounded-full bg-blue-500/10 flex items-center justify-center relative ring-2 ring-blue-500/20 group-hover:bg-blue-500/20 transition-all duration-700">
              <div className="absolute inset-x-[-40px] inset-y-[-40px] bg-blue-500/5 rounded-full animate-ping opacity-20" />
              <Play size={80} className="text-blue-500 ml-4 group-hover:scale-110 transition-transform duration-500" />
            </div>
            
            {/* Visual signal that the system is ready */}
            <div className="absolute -top-4 -right-4 bg-emerald-500/20 text-emerald-400 text-[8px] font-black uppercase tracking-widest px-3 py-1 rounded-full border border-emerald-500/20">
               Signal Detected
            </div>
          </div>

          <div className="text-center space-y-6">
             <div className="space-y-2">
               <h2 className="text-6xl font-black text-white uppercase tracking-tighter">Connection Ready</h2>
               <div className="h-1.5 w-32 bg-gradient-to-r from-transparent via-blue-500 to-transparent mx-auto rounded-full" />
             </div>
             <p className="text-blue-400 font-mono text-lg uppercase tracking-[0.8em] font-bold animate-pulse">Click to Join Stream</p>
          </div>
        </motion.div>
        
        <div className="absolute bottom-12 text-white/30 font-mono text-[9px] uppercase tracking-[0.6em] flex items-center gap-4 bg-white/5 py-3 px-8 rounded-full border border-white/10 backdrop-blur-md">
           <div className="w-2.5 h-2.5 rounded-full bg-emerald-500 shadow-[0_0_15px_rgba(16,185,129,1)] animate-pulse" />
           Local Broadcast Suite • Desktop Link Active
        </div>
      </div>
    );
  }

  const displayOverride = searchParams.get('display') || (view === 'timer' ? 'timer' : null);
  const displayMode = (displayOverride as any) || state.displaySettings?.secondaryDisplay || 'audience';

  const { programAsset } = state;

  const getMediaType = (asset: any) => {
    if (!asset) return null;
    const t = asset.type?.toLowerCase();
    if (['video', 'mp4', 'mov', 'avi', 'mkv'].includes(t)) return 'video';
    if (['audio', 'mp3', 'm4a', 'wav', 'ogg'].includes(t)) return 'audio';
    if (['image', 'jpg', 'jpeg', 'png', 'gif', 'webp'].includes(t)) return 'image';
    
    // Extension check
    const ext = asset.path?.split('.').pop()?.toLowerCase();
    if (['mp4', 'mov', 'avi', 'mkv'].includes(ext)) return 'video';
    if (['mp3', 'm4a', 'wav', 'ogg'].includes(ext)) return 'audio';
    if (['jpg', 'jpeg', 'png', 'gif', 'webp'].includes(ext)) return 'image';
    
    return t;
  };

  const mediaType = getMediaType(programAsset);
  const isMediaShowing = !!programAsset && mediaType !== 'audio';
  const showCamera = state.isMeetingLive && !isMediaShowing;

  const toggleFullscreen = () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().catch(err => {
        console.error(`Error attempting to enable full-screen mode: ${err.message}`);
      });
    } else {
      document.exitFullscreen();
    }
  };

  return (
    <div className="h-screen w-screen bg-black flex items-center justify-center overflow-hidden relative group">
      {/* Fullscreen Button Toggle (Visible on hover) */}
      <button 
        onClick={toggleFullscreen}
        className="absolute top-4 right-4 z-[1000] p-2 bg-black/50 hover:bg-black/80 rounded-full border border-white/10 text-white opacity-0 group-hover:opacity-100 transition-opacity"
        title="Toggle Fullscreen"
      >
        <Monitor size={20} />
      </button>

      {/* Background Audio Engine */}
      {state.bgmAsset && (
        <div style={{ position: 'absolute', width: 0, height: 0, opacity: 0, pointerEvents: 'none' }}>
          <SmartMedia 
            asset={state.bgmAsset}
            autoPlay={state.isPlayingBgm}
            muted={state.mixer?.isMuted ?? false}
            volume={state.mixer?.masterVolume ?? 100}
            seekTo={state.bgmSeekTo}
            id="bgm-audience-player"
            onError={handleMediaError}
          />
        </div>
      )}

      <div className={`w-full h-full flex ${displayMode === 'multiview' ? 'flex-row' : 'flex-col'}`}>
        {/* Main Feed Section */}
        <div className="relative flex-1 flex items-center justify-center overflow-hidden">
          {/* Background Layer: Camera Feed (Meeting Mode - Auto-fades when media plays) */}
          <AnimatePresence>
            {showCamera && (
              <motion.div 
                key="camera-feed"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
                transition={{ duration: 1.2, ease: "easeInOut" }}
                className="absolute inset-0 z-0 w-full h-full flex items-center justify-center overflow-hidden bg-black"
              >
                <SmartMedia 
                  asset={{ id: 'live-camera', type: 'camera', name: 'Live Camera' }}
                  className="w-full h-full object-contain"
                  autoPlay={true}
                  muted={true}
                  deviceId={state.selectedCameraId}
                />
              </motion.div>
            )}
          </AnimatePresence>

          {/* Program Layer: Media Assets */}
          <AnimatePresence mode="popLayout">
            {programAsset && (
              <motion.div 
                key={programAsset.id}
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
                transition={{ duration: 0.8, ease: "easeInOut" }}
                className={`absolute inset-0 z-10 w-full h-full flex items-center justify-center overflow-hidden ${mediaType === 'audio' ? '' : 'bg-black'}`}
              >
                <SmartMedia 
                  asset={programAsset}
                  className="w-full h-full object-contain audience-audio"
                  autoPlay={true}
                  muted={state.mixer?.isMuted ?? false}
                  volume={state.mixer?.masterVolume ?? 100}
                  controls={false}
                  onError={handleMediaError}
                  deviceId={state.selectedCameraId}
                />

                <AnimatePresence>
                  {permissionError && !(window as any).mediaflow?.isDesktop && (
                    <motion.div 
                      initial={{ opacity: 0 }}
                      animate={{ opacity: 1 }}
                      exit={{ opacity: 0 }}
                      className="absolute inset-0 z-50 bg-black/95 backdrop-blur-2xl flex flex-col items-center justify-center p-12 text-center"
                    >
                      <div className="w-24 h-24 rounded-full bg-blue-500/10 flex items-center justify-center mb-8 border border-blue-500/20">
                        <Monitor size={48} className="text-blue-500" />
                      </div>
                      <h2 className="text-3xl font-black text-white uppercase tracking-tighter mb-4">Storage Permission Required</h2>
                      <p className="text-white/40 text-sm max-w-lg uppercase tracking-widest leading-relaxed mb-12">
                        To display <span className="text-blue-400 font-mono">{programAsset.name}</span>, browser security requires you to grant access to the media folder.
                      </p>
                      <div className="flex flex-col gap-6 w-80">
                        <button 
                          onClick={async () => {
                            try {
                              await (window as any).showDirectoryPicker({ mode: 'read' });
                              setPermissionError(false);
                              setRetryCount(c => c + 1);
                            } catch (e) {
                              console.error('Permission granting failed:', e);
                            }
                          }}
                          className="px-8 py-6 bg-blue-500 text-white rounded-3xl font-black text-xs uppercase tracking-[0.3em] hover:bg-blue-400 transition-all shadow-2xl active:scale-95 flex items-center justify-center gap-4"
                        >
                          <FolderOpen size={18} />
                          Unlock Library
                        </button>
                        <button 
                          onClick={() => window.location.reload()}
                          className="px-8 py-6 bg-white/5 text-white/40 rounded-3xl font-black text-xs uppercase tracking-[0.3em] hover:bg-white/10 transition-all border border-white/10"
                        >
                          Refresh View
                        </button>
                      </div>
                    </motion.div>
                  )}
                </AnimatePresence>
              </motion.div>
            )}
          </AnimatePresence>

          {/* Standby Layer: Shown only when no media and not in live meeting */}
          <AnimatePresence>
            {!programAsset && !state.isMeetingLive && (
              <motion.div 
                key="black-screen"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
                transition={{ duration: 0.8 }}
                className="absolute inset-0 flex flex-col items-center justify-center gap-12 bg-black z-20"
              >
                 <div className="relative">
                   <div className="w-24 h-24 rounded-full border border-white/5 border-t-blue-500/40 animate-[spin_3s_linear_infinite]" />
                   <div className="absolute inset-0 flex items-center justify-center">
                     <Monitor size={32} className="text-white/5 animate-pulse" />
                   </div>
                 </div>
                 <div className="flex flex-col items-center gap-3">
                   <p className="text-white/20 font-mono text-xs uppercase tracking-[1em] font-bold">Mediaflow Suite</p>
                   <div className="flex items-center gap-3">
                      <div className="w-1.5 h-1.5 rounded-full bg-blue-500" />
                      <p className="text-white/5 font-mono text-[8px] uppercase tracking-[0.6em]">Awaiting Content from Live Feed</p>
                   </div>
                 </div>
              </motion.div>
            )}
          </AnimatePresence>
        </div>

        {/* Timer Section (Either as Fullscreen Overlay or Side Panel or Corner Overlay) */}
        <AnimatePresence>
          {state.timer?.isRunning && (
            <motion.div 
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              className={`
                ${displayMode === 'audience' ? 'absolute inset-0 z-[100] bg-black/60 backdrop-blur-md' : ''}
                ${displayMode === 'timer' ? 'absolute inset-0 z-[100] bg-black' : ''}
                ${displayMode === 'multiview' ? 'relative w-[400px] border-l border-white/10 bg-white/[0.02]' : ''}
                flex flex-col items-center justify-center
              `}
            >
              <div className="relative flex flex-col items-center justify-center text-center">
                <div className={`relative ${displayMode === 'multiview' ? 'w-[300px] h-[300px]' : 'w-[600px] h-[600px]'} flex items-center justify-center`}>
                  <svg viewBox="0 0 100 100" className="absolute inset-0 w-full h-full -rotate-90">
                    <defs>
                      <filter id="glow">
                        <feGaussianBlur stdDeviation="1.5" result="coloredBlur"/>
                        <feMerge>
                          <feMergeNode in="coloredBlur"/>
                          <feMergeNode in="SourceGraphic"/>
                        </feMerge>
                      </filter>
                    </defs>
                    
                    {[...Array(100)].map((_, i) => {
                      const progress = (state.timer?.duration || 0) > 0 
                        ? ((state.timer?.seconds || 0) / (state.timer?.duration || 1)) 
                        : ((state.timer?.seconds || 0) % 60) / 60;
                      const totalTicks = 100;
                      const activeTicks = Math.ceil(progress * totalTicks);
                      const isActive = i < activeTicks;

                      return (
                        <line
                          key={i}
                          x1="50" y1="2"
                          x2="50" y2={displayMode === 'multiview' ? '10' : '12'}
                          transform={`rotate(${(i * 360) / totalTicks}, 50, 50)`}
                          stroke={isActive ? '#06b6d4' : '#ffffff05'}
                          strokeWidth={displayMode === 'multiview' ? '1' : '1.2'}
                          filter={isActive ? 'url(#glow)' : ''}
                          className="transition-all duration-300"
                        />
                      );
                    })}
                  </svg>

                  <div className="relative z-10 flex flex-col items-center">
                    <motion.div
                      key={state.timer?.seconds || 0}
                      initial={{ opacity: 0, scale: 0.9 }}
                      animate={{ opacity: 1, scale: 1 }}
                      className={`font-mono font-black text-white leading-none tracking-tighter ${displayMode === 'multiview' ? 'text-[80px]' : 'text-[200px]'}`}
                    >
                      {(state.timer?.seconds || 0) >= 60 ? (
                        <div className="flex items-center gap-4">
                          <span>{Math.floor((state.timer?.seconds || 0) / 60)}</span>
                          <span className={`${displayMode === 'multiview' ? 'text-[40px]' : 'text-[100px]'} text-cyan-500/50`}>:</span>
                          <span>{((state.timer?.seconds || 0) % 60).toString().padStart(2, '0')}</span>
                        </div>
                      ) : (
                        (state.timer?.seconds || 0)
                      )}
                    </motion.div>
                    
                    {(state.timer?.seconds || 0) < 60 && (
                      <motion.p 
                        initial={{ opacity: 0, y: 10 }}
                        animate={{ opacity: 0.4, y: 0 }}
                        className={`${displayMode === 'multiview' ? 'text-[10px]' : 'text-xl'} text-cyan-400 font-mono uppercase tracking-[1em] mt-4`}
                      >
                        Seconds
                      </motion.p>
                    )}
                  </div>
                </div>
              </div>
              
              {displayMode !== 'multiview' && (
                <div className="absolute bottom-20 flex flex-col items-center gap-4">
                  <div className="h-[1px] w-12 bg-white/10" />
                  <p className="text-white/10 font-mono text-[10px] uppercase tracking-[1em]">JW Media Gateway</p>
                </div>
              )}
            </motion.div>
          )}
        </AnimatePresence>

        {/* Small Clock Overlay (if enabled in settings and not already showing large timer) */}
        {state.displaySettings?.showTimerOnAudience && !state.timer?.isRunning && !programAsset && (
            <motion.div 
               initial={{ opacity: 0, y: 20 }}
               animate={{ opacity: 1, y: 0 }}
               className="absolute top-10 left-10 z-[100] flex items-center gap-4 py-3 px-6 rounded-2xl bg-black/40 backdrop-blur-xl border border-white/10"
            >
               <div className="w-8 h-8 rounded-lg bg-blue-500/20 flex items-center justify-center">
                  <Clock size={16} className="text-blue-500" />
               </div>
               <div>
                  <div className="text-xl font-mono font-black text-white leading-none">
                     {new Date().toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit' })}
                  </div>
                  <div className="text-[9px] text-white/30 font-black uppercase tracking-[0.2em] mt-1">Local Time</div>
               </div>
            </motion.div>
        )}
      </div>
    </div>
  );
}
