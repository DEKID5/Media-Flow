import { useState, useEffect, useRef } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { AppState, MediaAsset } from '../types';
import { useSyncChannel } from '../hooks/useSyncChannel';
import { Video, Music, Image as ImageIcon, Monitor, Clock, FolderOpen, Play, Camera, ChevronRight, Pause } from 'lucide-react';
import { SmartMedia } from './SmartMedia';

export function AudienceView() {
  const searchParams = new URLSearchParams(window.location.search);
  const [view, setView] = useState(searchParams.get('view') || 'audience');
  const [state, setState] = useState<AppState | null>(null);
  const [permissionError, setPermissionError] = useState<boolean>(false);
  const [retryCount, setRetryCount] = useState(0);
  const [hasInteracted, setHasInteracted] = useState(!!((window as any).mediaflow?.isDesktop));
  const [endedProgramId, setEndedProgramId] = useState<string | null>(null);
  const bgmRef = useRef<HTMLAudioElement | null>(null);
  const bgmAudioGraphRef = useRef<null | {
    context: AudioContext;
    source: MediaElementAudioSourceNode;
    splitter: ChannelSplitterNode;
    merger: ChannelMergerNode;
    gain: GainNode;
  }>(null);

  useEffect(() => {
    if ((window as any).mediaflow) {
      const cleanup = (window as any).mediaflow.onSyncView((newView: string) => {
        console.log('AudienceView: Syncing view to', newView);
        setView(newView);
      });
      return cleanup;
    }
  }, []);

  const { send } = useSyncChannel((msg) => {
    if (msg.type === 'SYNC_STATE') {
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

  // --- Virtual Camera Broadcast Logic ---
  useEffect(() => {
    if (view === 'zoom') {
      document.title = 'MEDIAFLOW_NATIVE_BRIDGE_TARGET';
    } else {
      document.title = 'MediaFlow - Audience';
    }
  }, [view]);

  useEffect(() => {
    send({ type: 'AUDIENCE_READY' });
    const interval = setInterval(() => {
      send({ type: 'AUDIENCE_ALIVE' });
      if (!state) send({ type: 'AUDIENCE_READY' });
    }, 4000);
    return () => clearInterval(interval);
  }, [send, state]);

  useEffect(() => {
    setEndedProgramId(null);
  }, [state?.programAsset?.id]);

  useEffect(() => {
    const el = bgmRef.current;
    if (!el || !state?.bgmAsset || !hasInteracted) return;

    const ensureBgmRouting = async () => {
      try {
        if (!bgmAudioGraphRef.current) {
          const context = new (window.AudioContext || (window as any).webkitAudioContext)({
            latencyHint: 'playback',
            sampleRate: 48000
          });
          const source = context.createMediaElementSource(el);
          const splitter = context.createChannelSplitter(2);
          const merger = context.createChannelMerger(2);
          const gain = context.createGain();

          source.connect(splitter);
          splitter.connect(merger, 0, 0); // Channel One only
          merger.connect(gain);
          gain.connect(context.destination);

          bgmAudioGraphRef.current = { context, source, splitter, merger, gain };
          el.volume = 1;
          el.muted = false;
        }

        const graph = bgmAudioGraphRef.current;
        const targetVolume = state.mixer.isMuted ? 0 : (state.mixer.masterVolume ?? 100) / 100;
        graph.gain.gain.setTargetAtTime(targetVolume, graph.context.currentTime, 0.015);
        
        if (graph.context.state === 'suspended' && state.isPlayingBgm) {
          await graph.context.resume();
        }
      } catch (err) {
        console.warn('AudienceView: BGM routing failed', err);
      }
    };

    void ensureBgmRouting();
  }, [state?.bgmAsset?.id, state?.isPlayingBgm, state?.mixer.isMuted, state?.mixer.masterVolume, hasInteracted]);

  useEffect(() => {
    return () => {
      if (bgmAudioGraphRef.current) {
        bgmAudioGraphRef.current.source.disconnect();
        bgmAudioGraphRef.current.splitter.disconnect();
        bgmAudioGraphRef.current.merger.disconnect();
        bgmAudioGraphRef.current.gain.disconnect();
        bgmAudioGraphRef.current.context.close().catch(() => {});
        bgmAudioGraphRef.current = null;
      }
    };
  }, []);

  const handleInteraction = () => setHasInteracted(true);

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

  const programAsset = state.programAsset;
  const isZoom = view === 'zoom';
  const isProgramVisible = !!programAsset && endedProgramId !== programAsset.id;
  
  // VCam Mode Logic:
  // - auto: camera when no media, media when media is present
  // - camera: camera only when program media is not active
  // - media: keep camera off and show media if present
  const isProgramMediaActive = isProgramVisible && programAsset.type !== 'camera';
  const isMediaShowing = isProgramMediaActive;
  const showCamera = isZoom && !isProgramMediaActive && state.vcamMode !== 'media';
  const showProgramLayer = isProgramVisible && (!isZoom || isMediaShowing);
  const effectivelyLive = isZoom || state.isMeetingLive;
  const zoomMirrorCompensationClass = isZoom ? 'scale-x-[-1]' : '';
  const programType = programAsset?.type?.toLowerCase();
  const showSoftBackdrop = !!programAsset && (
    programType === 'image' ||
    /\.(jpg|jpeg|png|gif|webp|svg|bmp|ico|tiff)$/i.test(programAsset.path || '')
  );

  if (!hasInteracted) {
    return (
      <div className="h-screen w-screen bg-black flex flex-col items-center justify-center gap-4 cursor-pointer" onClick={handleInteraction}>
        <div className="w-16 h-16 rounded-full bg-blue-500/20 flex items-center justify-center text-blue-500 animate-pulse">
          <Play size={32} fill="currentColor" />
        </div>
        <p className="text-blue-500/60 font-black uppercase text-[10px] tracking-widest">Click to Enable Audio</p>
      </div>
    );
  }

  return (
    <div className="h-screen w-screen bg-black overflow-hidden flex flex-col font-sans select-none">
      <div className={`flex-1 relative flex items-center justify-center overflow-hidden ${zoomMirrorCompensationClass}`}>
        
        {/* Background Music Player (Hidden) */}
        {state.bgmAsset && (
          <audio 
            id="bgm-audience-player"
            ref={(node) => {
              bgmRef.current = node;
            }}
            autoPlay={state.isPlayingBgm}
            src={state.bgmAsset.path}
            onEnded={() => send({ type: 'BGM_ACTION', action: 'next' })}
          />
        )}

        {/* Live Camera Feed (Zoom View Only) */}
        <AnimatePresence>
          {showCamera && (
          <motion.div
            key="camera-feed"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.35, ease: "easeOut" }}
            className="absolute inset-0 z-0"
          >
            <SmartMedia
              asset={{ id: 'live-camera', type: 'camera', name: 'Live Camera' }}
              className="w-full h-full object-cover"
              autoPlay={true}
              muted={true}
              deviceId={state.selectedCameraId}
              isZoomView={true}
            />
          </motion.div>
        )}
        </AnimatePresence>

        {/* Media Layer (Videos/Images) */}
        <AnimatePresence>
          {programAsset && showProgramLayer && (
            <motion.div 
              key={programAsset.id}
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              transition={{ duration: 0.25, ease: "easeOut" }}
              className="absolute inset-0 z-10 flex items-center justify-center bg-black"
            >
              <SmartMedia 
                asset={programAsset}
                className="w-full h-full object-contain relative z-10"
                autoPlay={!state.isProgramPaused}
                muted={state.mixer.isMuted}
                volume={state.mixer.masterVolume}
                deviceId={state.selectedCameraId}
                isZoomView={isZoom}
                channelOneOutput={true}
                onEnd={() => {
                  setEndedProgramId(programAsset.id);
                  send({ type: 'PROGRAM_ENDED', assetId: programAsset.id });
                }}
              />
              
              {/* Soft backdrop for still images only. Avoid decoding live video twice. */}
              {showSoftBackdrop && (
                <div className="absolute inset-0 z-0 opacity-50 blur-3xl scale-110 overflow-hidden">
                  <SmartMedia 
                    asset={programAsset}
                    className="w-full h-full object-cover"
                    autoPlay={false}
                    muted={true}
                    isThumbnail={true}
                  />
                </div>
              )}

              {state.isProgramPaused && (
                <div className="absolute bottom-8 left-1/2 -translate-x-1/2 z-30 flex items-center gap-3 px-4 py-2 rounded-full bg-black/60 backdrop-blur-md border border-amber-400/30 text-amber-200">
                  <Pause size={14} fill="currentColor" />
                  <span className="text-[10px] font-black uppercase tracking-[0.3em]">Feed Paused</span>
                </div>
              )}
            </motion.div>
          )}
        </AnimatePresence>

        {/* Timer/Standby Layer */}
        <AnimatePresence>
          {!programAsset && !effectivelyLive && (
            <motion.div 
              key="standby-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              className="absolute inset-0 z-20 flex flex-col items-center justify-center bg-zinc-950"
            >
              <div className="flex flex-col items-center gap-12">
                <div className="relative">
                  <div className="w-48 h-48 rounded-full border border-white/5 animate-[pingslow_4s_infinite]" />
                  <div className="absolute inset-0 flex items-center justify-center">
                    <Monitor size={64} strokeWidth={0.5} className="text-white/10" />
                  </div>
                </div>
                <div className="flex flex-col items-center gap-4">
                  <h2 className="text-white/20 font-mono text-xs uppercase tracking-[1em]">Standby • No Signal</h2>
                  <div className="h-[1px] w-12 bg-white/10" />
                  <p className="text-white/10 font-mono text-[10px] uppercase tracking-[1em]">JW Media Gateway</p>
                </div>
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Timer Overlay */}
        {state.timer.isRunning && (
            <motion.div 
              key="active-timer"
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 1.1 }}
              className="absolute inset-0 z-50 flex flex-col items-center justify-center bg-black/60 backdrop-blur-md"
            >
              <div className="text-[200px] font-mono font-black text-white tracking-tighter drop-shadow-[0_0_50px_rgba(59,130,246,0.3)]">
                {Math.floor(state.timer.seconds / 60)}:{(state.timer.seconds % 60).toString().padStart(2, '0')}
              </div>
              <div className="px-8 py-3 rounded-full border border-blue-500/30 bg-blue-500/10 text-blue-400 text-xs font-black uppercase tracking-[0.4em] backdrop-blur-md">
                Meeting Focus
              </div>
            </motion.div>
        )}

        {/* Small Clock Overlay */}
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
        
        {/* Broadcast Status Indicator */}
        {isZoom && (
          <motion.div 
            initial={{ opacity: 0, y: -20 }}
            animate={{ opacity: 1, y: 0 }}
            className="absolute top-6 left-1/2 -translate-x-1/2 z-[200] flex items-center gap-3 px-4 py-2 rounded-full bg-black/40 backdrop-blur-md border border-white/10 pointer-events-none"
          >
            <div className="w-2 h-2 rounded-full bg-emerald-500 animate-pulse shadow-[0_0_10px_rgba(16,185,129,0.8)]" />
            <span className="text-[10px] font-black text-white/80 uppercase tracking-[0.3em]">Broadcast Active</span>
          </motion.div>
        )}
      </div>
    </div>
  );
}
