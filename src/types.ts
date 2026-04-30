export type MediaType = 'video' | 'audio' | 'image' | 'camera';

export interface MediaAsset {
  id: string;
  name: string;
  path?: string; // Optional for demo assets with direct URLs
  url?: string;  // Direct URL support (for demo or web assets)
  type: MediaType;
  duration?: number;
  thumbnail?: string;
  tags?: string[];
  fileHandle?: FileSystemFileHandle;
}

export interface MeetingItem {
  id: string;
  title: string;
  time?: string;
  type: string;
  mediaIds: string[];
  active?: boolean;
}

export interface TimerState {
  seconds: number;
  isRunning: boolean;
  duration: number;
}

export interface MixerState {
  masterVolume: number;
  isMuted: boolean;
  monitorVolume: number;
  isMonitorMuted: boolean;
}

export interface DisplaySettings {
  mainDisplay: 'audience' | 'timer' | 'multiview';
  secondaryDisplay: 'audience' | 'timer' | 'multiview';
  showTimerOnAudience: boolean;
}

export interface AppState {
  previewAsset: MediaAsset | null;
  programAsset: MediaAsset | null;
  bgmAsset: MediaAsset | null;
  isPlayingBgm: boolean;
  timer: TimerState;
  previewTimer: boolean;
  mixer: MixerState;
  displaySettings: DisplaySettings;
  meetingType: 'midweek' | 'weekend';
  language: string;
  videoPaths: string[];
  imagePaths: string[];
  audioPaths: string[];
  bgmFolderPath: string | null;
  bgmSeekTo?: number;
  isPermissionGranted: boolean;
  selectedCameraId?: string;
  isMeetingLive: boolean;
  vcamMode: 'auto' | 'camera' | 'media';
  isProgramPaused: boolean;
}

export type SyncMessage = 
  | { type: 'AUDIENCE_READY' }
  | { type: 'AUDIENCE_ALIVE' }
  | { type: 'SYNC_STATE'; state: AppState }
  | { type: 'REQUEST_PLAY'; assetId: string }
  | { type: 'STATE_UPDATE'; delta: Partial<AppState> }
  | { type: 'BGM_ACTION'; action: 'seek'; offset: number }
  | { type: 'RESOLVE_ASSET'; assetId: string }
  | { type: 'ASSET_RESOLVED'; assetId: string; url?: string | null; file?: File | null }
  | { type: 'PROGRAM_ENDED'; assetId: string };
