import { useState, useEffect, useRef, useMemo } from 'react';
import { 
  Monitor, 
  Play, 
  Square, 
  Volume2, 
  Clock, 
  Database, 
  Wifi, 
  Settings, 
  Layers, 
  Music, 
  Image as ImageIcon, 
  Video, 
  ChevronRight,
  Activity,
  Mic2,
  Pause,
  SkipBack,
  SkipForward,
  Folder,
  FolderOpen,
  FolderPlus,
  VolumeX,
  Plus,
  Trash2,
  X,
  Globe,
  ChevronDown,
  ChevronUp,
  Unlock,
  ShieldCheck,
  AlertCircle,
  Cast,
  RotateCw,
  RotateCcw,
  Check,
  Rewind,
  FastForward,
  Camera
} from 'lucide-react';
import { motion, AnimatePresence } from 'motion/react';
import { MediaAsset, MeetingItem, AppState, MediaType } from '../types';
import { useSyncChannel } from '../hooks/useSyncChannel';
import { SmartMedia } from './SmartMedia';

const INITIAL_MEDIA_LIBRARY: MediaAsset[] = [];

const MIDWEEK_SCHEDULE: MeetingItem[] = [
  { id: 'm1', time: '19:00', title: 'Opening Song', active: true, type: 'song', mediaIds: [] },
  { id: 'm2', time: '19:05', title: 'Treasures from God\'s Word', active: false, type: 'talk', mediaIds: [] },
  { id: 'm3', time: '19:15', title: 'Spiritual Gems', active: false, type: 'demo', mediaIds: [] },
  { id: 'm4', time: '19:40', title: 'Middle Song', active: false, type: 'song', mediaIds: [] },
  { id: 'm5', time: '19:45', title: 'Living as Christians', active: false, type: 'living', mediaIds: [] },
  { id: 'm6', time: '20:10', title: 'Congregation Bible Study', active: false, type: 'study', mediaIds: [] },
  { id: 'm7', time: '20:40', title: 'Closing Song', active: false, type: 'song', mediaIds: [] },
];

const WEEKEND_SCHEDULE: MeetingItem[] = [
  { id: 'w1', time: '10:00', title: 'Opening Song', active: true, type: 'song', mediaIds: [] },
  { id: 'w2', time: '10:05', title: 'Public Discourse', active: false, type: 'talk', mediaIds: [] },
  { id: 'w3', time: '10:35', title: 'Middle Song', active: false, type: 'song', mediaIds: [] },
  { id: 'w4', time: '10:40', title: 'Watchtower', active: false, type: 'study', mediaIds: [] },
  { id: 'w5', time: '11:40', title: 'Closing Song', active: false, type: 'song', mediaIds: [] },
];

// (SmartMedia moved to its own file)

// Song titles for "Sing Out Joyfully to Jehovah" (sjj)
const SJJ_TITLES: Record<string, string> = {
  "1": "Jehovah’s Attributes",
  "2": "Jehovah Is Your Name",
  "3": "Our Strength, Our Hope, Our Confidence",
  "4": "Jehovah Is My Shepherd",
  "5": "God’s Wondrous Works",
  "6": "The Heavens Declare God’s Glory",
  "7": "Jehovah, Our Strength",
  "8": "Jehovah Is Our Refuge",
  "9": "Jehovah Is Our King!",
  "10": "Give Us More Faith",
  "11": "Creation Praises Jehovah",
  "12": "Great God, Jehovah",
  "13": "Christ, Our Model",
  "14": "Praising Earth’s New King",
  "15": "Praise Jehovah’s Firstborn!",
  "16": "Praise Jah for His Son, the Anointed",
  "17": "I Want To",
  "18": "Grateful for the Ransom",
  "19": "The Lord’s Evening Meal",
  "20": "You Gave Your Only-Begotten Son",
  "21": "Keep On Seeking First the Kingdom",
  "22": "The Kingdom Is in Place—Let It Come!",
  "23": "Jehovah Begins His Rule",
  "24": "Come to Jehovah’s Mountain",
  "25": "A Special Possession",
  "26": "You Did It for Me",
  "27": "The Meditation of My Heart",
  "28": "Your Loyal Love",
  "29": "Living Up to Our Name",
  "30": "My Father, My God and Friend",
  "31": "Oh, Walk With God!",
  "32": "Take Sides With Jehovah!",
  "33": "Throw Your Burden on Jehovah",
  "34": "Walking in Integrity",
  "35": "Make a Good Name With God",
  "36": "We Guard Our Hearts",
  "37": "Serving Jehovah Whole-Souled",
  "38": "He Will Make You Strong",
  "39": "And He Will Pave the Way",
  "40": "To Whom Do We Belong?",
  "41": "Please Hear My Prayer",
  "42": "The Prayer of God’s Servant",
  "43": "A Prayer of Thanksgiving",
  "44": "Prayer of the Lowly One",
  "45": "The Move Is on the Way",
  "46": "We Thank You, Jehovah",
  "47": "Pray to Jehovah Each Day",
  "48": "Daily Walking With Jehovah",
  "49": "Making Jehovah’s Heart Glad",
  "50": "The Lord’s Day",
  "51": "We Adhere to the Faithful Slave",
  "52": "Christian Dedication",
  "53": "Preparing to Preach",
  "54": "This Is the Way",
  "55": "Fear Them Not!",
  "56": "Make the Truth Your Own",
  "57": "Preach the Word",
  "58": "Searching for Friends of Peace",
  "59": "Praise Out of the Mouth of Babes",
  "60": "It Is Jehovah Who Makes It Grow",
  "61": "Forward, You Witnesses!",
  "62": "The Toil of Our Hands",
  "63": "We’re Jehovah’s Witnesses!",
  "64": "Share in the Joy of the Harvest",
  "65": "Move Ahead!",
  "66": "Declare the Good News",
  "67": "Preach With Zeal",
  "68": "Sow Your Seed in the Morning",
  "69": "Go Forward into the Ministry!",
  "70": "Search Out Deserving Ones",
  "71": "We Are Jehovah’s Army!",
  "72": "Making Known the Kingdom Truth",
  "73": "Grant Us Boldness",
  "74": "Join in the Kingdom Song!",
  "75": "“Here I Am! Send Me”",
  "76": "How Does It Make You Feel?",
  "77": "Light in a Darkened World",
  "78": "“Teaching the Word of God”",
  "79": "Teach Them to Stand Firm",
  "80": "“Taste and See That Jehovah Is Good”",
  "81": "The Life of a Pioneer",
  "82": "Let Your Light Shine",
  "83": "From House to House",
  "84": "Reaching Out",
  "85": "Welcome One Another",
  "86": "We Must Be Taught",
  "87": "Our Meeting Together",
  "88": "Jehovah Is My Helper",
  "89": "Listen, Obey, and Be Blessed",
  "90": "Encourage One Another",
  "91": "Our Labor of Love",
  "92": "A Place for Your Name",
  "93": "Bless Our Meeting Together",
  "94": "Grateful for God’s Word",
  "95": "The Light Gets Brighter",
  "96": "The Book of God—A Treasure",
  "97": "Life Depends on God’s Word",
  "98": "The Scriptures—Inspired by God",
  "99": "Daily Reading of God’s Word",
  "100": "The Word of God Is Alive",
  "101": "Working Together in Unity",
  "102": "The Helper",
  "103": "Shepherds—Gifts in Men",
  "104": "God’s Gift of Holy Spirit",
  "105": "“God Is Love”",
  "106": "Cultivating the Quality of Love",
  "107": "The Love of a Father",
  "108": "God’s Loyal Love",
  "109": "Love Intensely From the Heart",
  "110": "“The Joy of Jehovah”",
  "111": "Our Reasons for Joy",
  "112": "Jehovah, God of Peace",
  "113": "Our Peace, Unity, and Love",
  "114": "Exercise Patience",
  "115": "Gratitude",
  "116": "The Virtue of Kindness",
  "117": "The Quality of Goodness",
  "118": "“Give Us More Faith”",
  "119": "The Christian Quality of Mildness",
  "120": "Imitating Christ’s Mildness",
  "121": "We Need Self-Control",
  "122": "Be Steadfast, Unmovable!",
  "123": "Locally Assigned to the Truth",
  "124": "Ever Loyal",
  "125": "Happy Are the Merciful!",
  "126": "Stay Awake, Stand Firm, Grow Mighty",
  "127": "The Reward of Loyalty",
  "128": "Enduring to the End",
  "129": "We Will Keep Enduring",
  "130": "Be Forgiving",
  "131": "“What God Has Yoked Together”",
  "132": "A Family Care",
  "133": "Worship Jehovah During Youth",
  "134": "Children Are a Trust From God",
  "135": "Jehovah’s Warm Appeal: “Be Wise, My Son”",
  "136": "A Book Above All Others",
  "137": "Faithful Women, Christian Sisters",
  "138": "Beauty in Gray-Headedness",
  "139": "“See Yourself When All Is New”",
  "140": "The Life Without End—At Last!",
  "141": "Searching for Friends of Peace",
  "142": "Holding Fast to Our Hope",
  "143": "Keep Your Eyes on the Prize!",
  "144": "It Will Not Be Late!",
  "145": "Preparing to Preach",
  "146": "“Make All Things New”",
  "147": "Life Everlasting Is Promised",
  "148": "Jehovah Provides Escape",
  "149": "A Victory Song",
  "150": "Reaching Out",
  "151": "He Will Call",
  "152": "The Resurrection—Our Hope",
  "153": "How Does It Make You Feel?",
  "154": "Jehovah’s Attributes",
  "155": "Building for Jehovah",
};

const prettifyMediaName = (rawName: string): string => {
  // Remove extension
  const baseName = rawName.replace(/\.[^/.]+$/, "");
  
  // Pattern for SJJ songs: sjjm_E_097_r720P, sjj_E_002, sjjm_EW_080, etc.
  // This matches a song number (usually 3 digits) after prefix and language
  const sjjPattern = /sjj[mp]?_[A-Z]+_(\d+)/i;
  const match = baseName.match(sjjPattern);
  
  if (match) {
    const num = parseInt(match[1]).toString();
    const title = SJJ_TITLES[num];
    if (title) {
      return `${num}. ${title}`;
    }
    return `Song ${num}`;
  }
  
  // Pattern for simple numbering: "15. Screenshot" or "Screenshot (15)"
  const numberPattern = /^(?:\d+\.\s+)?(.*?)(?:\s*\(\d+\))?$/;
  const numMatch = baseName.match(numberPattern);
  
  let cleanName = baseName;
  if (numMatch && numMatch[1]) {
    cleanName = numMatch[1].trim();
  }

  // Handle other types of files: replace underscores/dashes with spaces, capitalize
  return cleanName
    .replace(/[_-]/g, " ")
    .replace(/\b\w/g, (l) => l.toUpperCase());
};

export function OperatorDashboard() {
  const [state, setState] = useState<AppState>({
    previewAsset: null,
    programAsset: null,
    bgmAsset: null,
    isPlayingBgm: false,
    timer: { seconds: 0, isRunning: false, duration: 0 },
    previewTimer: false,
    mixer: { masterVolume: 65, isMuted: false },
    displaySettings: {
      mainDisplay: 'timer',
      secondaryDisplay: 'audience',
      showTimerOnAudience: true,
    },
    meetingType: 'midweek',
    language: 'E',
    videoPaths: [],
    imagePaths: [],
    audioPaths: [],
    bgmFolderPath: null,
    bgmSeekTo: undefined,
    isPermissionGranted: false,
    selectedCameraId: '',
    isMeetingLive: false,
    vcamMode: 'auto',
    isProgramPaused: false
  });

  const [monitorMuted, setMonitorMuted] = useState(false);

  const [mediaFolders, setMediaFolders] = useState<FileSystemDirectoryHandle[]>([]);

  const [schedules, setSchedules] = useState<{ midweek: MeetingItem[], weekend: MeetingItem[] }>({
    midweek: MIDWEEK_SCHEDULE,
    weekend: WEEKEND_SCHEDULE
  });

  const [mediaLibrary, setMediaLibrary] = useState<MediaAsset[]>(INITIAL_MEDIA_LIBRARY);
  const [showAllMedia, setShowAllMedia] = useState(false);
  const [selectedMeetingItemId, setSelectedMeetingItemId] = useState<string | null>(null);
  const [songInputs, setSongInputs] = useState<Record<string, string>>({});
  const [importMenu, setImportMenu] = useState<{ itemId: string, type: 'video' | 'image' } | null>(null);
  const [scanResults, setScanResults] = useState<any[] | null>(null);
  const [hasSecondaryScreen, setHasSecondaryScreen] = useState(false);
  const [isAudienceLive, setIsAudienceLive] = useState(false);
  const [lastAudienceSignal, setLastAudienceSignal] = useState(0);
  const [bridgeStatus, setBridgeStatus] = useState<{ status: string, device?: string, message?: string }>({ status: 'inactive' });
  const [availableCameras, setAvailableCameras] = useState<MediaDeviceInfo[]>([]);
  const [audioOutputName, setAudioOutputName] = useState('System Default');
  const isObsDetected = useMemo(() => {
    return availableCameras.some(cam => cam.label.toLowerCase().includes('obs'));
  }, [availableCameras]);

  useEffect(() => {
    const handleDevices = () => {
      navigator.mediaDevices.enumerateDevices().then(devices => {
        const cams = devices.filter(d => d.kind === 'videoinput');
        const outputs = devices.filter(d => d.kind === 'audiooutput');
        const defaultOutput = outputs.find(d => d.deviceId === 'default') || outputs[0];
        setAvailableCameras(cams);
        setAudioOutputName(defaultOutput?.label || 'System Default');
        if (cams.length > 0 && !state.selectedCameraId) {
          setState(s => ({ ...s, selectedCameraId: cams[0].deviceId }));
        }
      });
    };
    handleDevices();
    navigator.mediaDevices.addEventListener('devicechange', handleDevices);
    return () => navigator.mediaDevices.removeEventListener('devicechange', handleDevices);
  }, []);
  const requiresRescan = !((window as any).mediaflow?.isDesktop) && mediaLibrary.length > 0 && mediaLibrary.some(a => !a.fileHandle);
  const [showSettings, setShowSettings] = useState(false);
  const [showAudioPanel, setShowAudioPanel] = useState(false);
  const [showManualAdd, setShowManualAdd] = useState(false);
  const [showLanguageMenu, setShowLanguageMenu] = useState(false);
  const [showFolderManager, setShowFolderManager] = useState(false);
  const [manualEntry, setManualEntry] = useState<{ name: string, type: 'video' | 'image' | 'audio', path: string, fileHandle?: any }>({
    name: '',
    type: 'video',
    path: '',
    fileHandle: null
  });

  // Background Music (BGM) State
  const [bgPlaylist, setBgPlaylist] = useState<any[]>([]);
  const [currentBgIndex, setCurrentBgIndex] = useState(0);
  const [isPlayingBg, setIsPlayingBg] = useState(false);
  
  const [updateStatus, setUpdateStatus] = useState<{ status: string, message?: string, progress?: number, info?: any } | null>(null);

  useEffect(() => {
    if ((window as any).mediaflow) {
      const cleanupUpdate = (window as any).mediaflow.onUpdateStatus((data: any) => {
        setUpdateStatus(data);
      });
      const cleanupBridge = (window as any).mediaflow.onBridgeStatus((data: any) => {
        setBridgeStatus(data);
        if (data.status === 'active') {
          setIsAudienceLive(true);
        } else if (data.status === 'inactive' || data.status === 'error') {
          setIsAudienceLive(false);
        }
      });
      return () => {
        cleanupUpdate();
        cleanupBridge();
      };
    }
  }, []);

  const lastSyncRef = useRef(0);

  const filteredBgPlaylist = useMemo(() => {
    return bgPlaylist.filter(track => {
      if (state.language === 'EW') {
        return track.name.includes('_EW_');
      } else {
        // English is _E_ and must NOT contain _EW_
        return track.name.includes('_E_') && !track.name.includes('_EW_');
      }
    });
  }, [bgPlaylist, state.language]);

  // Sync BGM to state for Audience
  useEffect(() => {
    const currentAsset = filteredBgPlaylist[currentBgIndex] || null;
    setState(s => {
      if (s.bgmAsset?.id === currentAsset?.id && s.isPlayingBgm === isPlayingBg) return s;
      return { 
        ...s, 
        bgmAsset: currentAsset,
        isPlayingBgm: isPlayingBg
      };
    });
  }, [currentBgIndex, isPlayingBg, filteredBgPlaylist]);

  // Persistance logic
  useEffect(() => {
    const savedSchedules = localStorage.getItem('meeting_schedules_v2');
    const savedLibrary = localStorage.getItem('meeting_media_library');
    const savedBgm = localStorage.getItem('meeting_bgm_playlist');
    
    if (savedSchedules) {
      try {
        const parsed = JSON.parse(savedSchedules);
        if (parsed && typeof parsed === 'object' && parsed.midweek && parsed.weekend) {
           setSchedules(parsed);
        }
      } catch (e) { console.error('Failed to load schedules', e); }
    }
    if (savedLibrary) {
      try {
        const parsed = JSON.parse(savedLibrary);
        if (Array.isArray(parsed)) {
          setMediaLibrary(parsed);
        }
      } catch (e) { 
        console.error('Failed to load library', e);
        setMediaLibrary(INITIAL_MEDIA_LIBRARY);
      }
    } else {
      setMediaLibrary(INITIAL_MEDIA_LIBRARY);
    }
    if (savedBgm) {
      try {
        const parsed = JSON.parse(savedBgm);
        if (Array.isArray(parsed)) {
          setBgPlaylist(parsed);
        }
      } catch (e) { console.error('Failed to load BGM', e); }
    }
  }, []);

  useEffect(() => {
    localStorage.setItem('meeting_schedules_v2', JSON.stringify(schedules));
  }, [schedules]);

  useEffect(() => {
    // Only save serializable parts of media library
    const serializableLibrary = mediaLibrary.map(({ fileHandle, ...rest }) => rest);
    localStorage.setItem('meeting_media_library', JSON.stringify(serializableLibrary));
  }, [mediaLibrary]);

  useEffect(() => {
    const serializableBgm = bgPlaylist.map(({ fileHandle, ...rest }) => rest);
    localStorage.setItem('meeting_bgm_playlist', JSON.stringify(serializableBgm));
  }, [bgPlaylist]);

  // --- JW Publications Scanning Logic ---
  const handleScanMWBImages = async (itemId?: string) => {
    if (itemId) {
      handleImportClick(itemId, 'image', true);
    }
  };

  const [isScanningBg, setIsScanningBg] = useState(false);

  const handleScanBgMusic = async () => {
    try {
      setIsScanningBg(true);
      let results: any[] = [];
      let folderName = "";

      if ((window as any).mediaflow?.isDesktop) {
        // Use Electron picker for full paths
        const dirPath = await (window as any).mediaflow.pickDirectory();
        if (!dirPath) return;
        
        folderName = dirPath.split(/[\\\/]/).pop() || dirPath;
        // The scanning is handled in Electron for speed and permission
        const files = await (window as any).mediaflow.scanJwMedia(null, [dirPath]);
        // Filter for MP3 only as requested
        results = files
          .filter((f: any) => f.path.toLowerCase().endsWith('.mp3'))
          .map((f: any) => ({
            id: `bgm-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`,
            name: f.name,
            path: f.path,
            type: 'audio'
          }));
      } else {
        // Web fallback
        const handle = await (window as any).showDirectoryPicker({
          mode: 'read',
          startIn: 'music'
        });
        folderName = handle.name;
        
        const scanDir = async (dirHandle: FileSystemDirectoryHandle, prefix = '') => {
          for await (const entry of (dirHandle as any).values()) {
            const fullPath = prefix ? `${prefix}/${entry.name}` : entry.name;
            const isAudio = /\.mp3$/i.test(entry.name);
            if (entry.kind === 'file' && isAudio) {
              results.push({
                id: `bgm-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`,
                name: entry.name,
                path: fullPath,
                type: 'audio',
                fileHandle: entry
              });
            } else if (entry.kind === 'directory') {
              await scanDir(entry, fullPath);
            }
          }
        };
        await scanDir(handle);
      }
      
      setBgPlaylist(results);
      setCurrentBgIndex(0);
      setState(s => ({ ...s, bgmFolderPath: folderName }));
    } catch (error) {
      console.error('BG Scan failed:', error);
    } finally {
      setIsScanningBg(false);
    }
  };

  const toggleBgMusic = () => {
    if (filteredBgPlaylist.length === 0) return;
    setIsPlayingBg(!isPlayingBg);
  };

  const nextBg = () => {
    if (filteredBgPlaylist.length === 0) return;
    setCurrentBgIndex((prev) => (prev + 1) % filteredBgPlaylist.length);
  };

  const prevBg = () => {
    if (filteredBgPlaylist.length === 0) return;
    setCurrentBgIndex((prev) => (prev - 1 + filteredBgPlaylist.length) % filteredBgPlaylist.length);
  };

  const seekBg = (seconds: number) => {
    // We'll use a trick: set bgmSeekTo in state. AudienceView will respond.
    // To allow repeated seeks to the same relative offset, we might need a timestamp or a counter, 
    // but here we'll try to guess the current time or just send a "jump" signal.
    // Since we don't have the current time of the *audience* player easily, 
    // we'll use a message-based approach or a timestamp-based trigger.
    
    // For now, let's just support "Rewind to Start" easily.
    if (seconds === 0) {
      const player = document.getElementById('bgm-monitor') as HTMLMediaElement;
      if (player) player.currentTime = 0;

      setState(s => ({ ...s, bgmSeekTo: 0 }));
      // Reset trigger after a short delay so it can be triggered again
      setTimeout(() => setState(s => ({ ...s, bgmSeekTo: undefined })), 100);
    }
  };

  const applyBgmJump = (offset: number) => {
    const player = document.getElementById('bgm-monitor') as HTMLMediaElement;
    if (player) {
      player.currentTime = Math.max(0, player.currentTime + offset);
    }
  };

  const jumpBg = (offset: number) => {
     // Apply locally
     applyBgmJump(offset);
     // Send to audience
     (window as any).__SYNC_SEND__?.({ type: 'BGM_ACTION', action: 'seek', offset });
  };

  const getMediaType = (asset: MediaAsset | null) => {
    if (!asset) return null;
    const t = asset.type?.toLowerCase();
    if (['video', 'mp4', 'mov', 'avi', 'mkv'].includes(t)) return 'video';
    if (['audio', 'mp3', 'm4a', 'wav', 'ogg'].includes(t)) return 'audio';
    if (['image', 'jpg', 'jpeg', 'png', 'gif', 'webp'].includes(t)) return 'image';
    
    // Extension check
    const ext = asset.path?.split('.').pop()?.toLowerCase();
    if (['mp4', 'mov', 'avi', 'mkv'].includes(ext || '')) return 'video';
    if (['mp3', 'm4a', 'wav', 'ogg'].includes(ext || '')) return 'audio';
    if (['jpg', 'jpeg', 'png', 'gif', 'webp'].includes(ext || '')) return 'image';
    
    return t;
  };

  const activeSchedule = state.meetingType === 'midweek' ? schedules.midweek : schedules.weekend;
  
  // Clear broken demo links
  useEffect(() => {
    setSchedules(prev => ({
      midweek: prev.midweek.map(it => ({ ...it, mediaIds: it.mediaIds.filter(id => mediaLibrary.some(a => a.id === id)) })),
      weekend: prev.weekend.map(it => ({ ...it, mediaIds: it.mediaIds.filter(id => mediaLibrary.some(a => a.id === id)) }))
    }));
  }, [mediaLibrary]);

  const linkMediaToItem = (assetId: string) => {
    if (!selectedMeetingItemId) return;
    setSchedules(prev => {
      const type = state.meetingType;
      return {
        ...prev,
        [type]: prev[type].map(it => {
          if (it.id === selectedMeetingItemId && !it.mediaIds.includes(assetId)) {
            return { ...it, mediaIds: [...it.mediaIds, assetId] };
          }
          return it;
        })
      };
    });
  };

  const filteredMedia = mediaLibrary.filter(asset => {
    // Basic schedule filter
    if (selectedMeetingItemId && !showAllMedia) {
      const item = activeSchedule.find(i => i.id === selectedMeetingItemId);
      if (!item?.mediaIds.includes(asset.id)) return false;
    }

    // Language filter for media (audio/video)
    if (asset.type === 'audio' || asset.type === 'video') {
      const uName = asset.name.toUpperCase();
      const hasEN = uName.includes('_EN_') || uName.includes('_E_');
      const hasEWE = uName.includes('_EWE_') || uName.includes('_EW_');
      
      if (state.language === 'E') {
        if (hasEWE && !hasEN) return false;
      } else if (state.language === 'EW') {
        if (hasEN && !hasEWE) return false;
      }
    }

    return true;
  });

  const isAssetLinked = (assetId: string) => {
    if (!selectedMeetingItemId) return false;
    const item = activeSchedule.find(i => i.id === selectedMeetingItemId);
    return item?.mediaIds.includes(assetId);
  };

  const renderMediaThumbnail = (asset: MediaAsset, className = 'w-full h-full object-cover') => {
    if (asset.type === 'image') {
      return <SmartMedia asset={asset} className={className} muted={true} isThumbnail={true} />;
    }

    if (asset.type === 'audio') {
      return (
        <div className={`${className} flex items-center justify-center bg-blue-500/10`}>
          <Music size={18} className="text-blue-400/70" />
        </div>
      );
    }

    return <SmartMedia asset={asset} className={className} muted={true} isThumbnail={true} />;
  };

  const mediaLibraryRef = useRef<MediaAsset[]>(INITIAL_MEDIA_LIBRARY);
  const objectUrlCacheRef = useRef<Map<string, { signature: string; url: string }>>(new Map());

  useEffect(() => {
    mediaLibraryRef.current = mediaLibrary;
    const liveIds = new Set(mediaLibrary.map(asset => asset.id));
    objectUrlCacheRef.current.forEach(({ url }, id) => {
      if (!liveIds.has(id)) {
        URL.revokeObjectURL(url);
        objectUrlCacheRef.current.delete(id);
      }
    });
  }, [mediaLibrary]);

  useEffect(() => {
    const resolveAssetForPlayback = async (assetId: string): Promise<{ url: string | null; file?: null }> => {
      const asset = mediaLibraryRef.current.find(a => a.id === assetId);
      if (!asset) return { url: null };

      if (asset.path || asset.url) {
        return { url: asset.path || asset.url || null };
      }

      if (!asset.fileHandle) return { url: null };

      try {
        const file = await asset.fileHandle.getFile();
        const signature = `${file.name}:${file.size}:${file.lastModified}`;
        const cached = objectUrlCacheRef.current.get(asset.id);
        if (cached?.signature === signature) {
          return { url: cached.url };
        }

        if (cached?.url) URL.revokeObjectURL(cached.url);
        const url = URL.createObjectURL(file);
        objectUrlCacheRef.current.set(asset.id, { signature, url });
        return { url };
      } catch (e) {
        console.error('Failed to resolve asset via API', e);
        return { url: null };
      }
    };

    (window as any).__MEDIAFLOW_API__ = {
      resolveAssetUrlById: resolveAssetForPlayback
    };

    return () => {
      objectUrlCacheRef.current.forEach(({ url }) => URL.revokeObjectURL(url));
      objectUrlCacheRef.current.clear();
      delete (window as any).__MEDIAFLOW_API__;
    };
  }, []);

  const [scanningStatus, setScanningStatus] = useState<string | null>(null);

  useEffect(() => {
    // If in Electron, automate permission and background scan
    const isDesktop = !!(window as any).mediaflow?.isDesktop;
    if (isDesktop && !state.isPermissionGranted) {
      setState(s => ({ ...s, isPermissionGranted: true }));
      
      const autoScan = async () => {
        try {
          setScanningStatus('Initializing Media Scan...');
          const results = await (window as any).mediaflow.scanJwMedia(state.language);
          if (results && results.length > 0) {
            setScanningStatus(`Found ${results.length} files...`);
            const mappedResults = results.map((r: any) => ({
              id: `el-${r.path}`,
              name: prettifyMediaName(r.name),
              path: r.path,
              type: r.type,
              size: r.size
            }));
            setMediaLibrary(prev => {
              const existingPaths = new Set(prev.map(a => a.path));
              const newAssets = mappedResults.filter((a: any) => !existingPaths.has(a.path));
              return [...prev, ...newAssets];
            });
            setTimeout(() => setScanningStatus(null), 3000);
          } else {
            setScanningStatus('No files found in default directories.');
            setTimeout(() => setScanningStatus(null), 5000);
          }
        } catch (e) {
          console.error('Background auto-scan failed:', e);
          setScanningStatus('Scan Failed');
          setTimeout(() => setScanningStatus(null), 5000);
        }
      };
      
      autoScan();
    }
  }, [state.isPermissionGranted, state.language]);

  const requestPermissions = async () => {
    try {
      const handle = await (window as any).showDirectoryPicker({
        mode: 'read',
        startIn: 'videos'
      });
      
      if (handle) {
         setState(s => ({ 
           ...s, 
           isPermissionGranted: true
         }));
         setMediaFolders(prev => [...prev, handle]);
         await scanDirectoryRecursive(handle);
      }
    } catch (err) {
      console.error('Permission request failed:', err);
    }
  };

  const removeFolder = (index: number) => {
    setMediaFolders(prev => {
      const next = [...prev];
      next.splice(index, 1);
      return next;
    });
  };

  const rescannAllFolders = async () => {
    setMediaLibrary([]); // Optional: clear or merge? Usually better to fresh scan
    for (const handle of mediaFolders) {
      await scanDirectoryRecursive(handle);
    }
  };

  const scanDirectoryRecursive = async (handle: FileSystemDirectoryHandle, results: MediaAsset[] = [], pathPrefix = '') => {
    for await (const entry of (handle as any).values()) {
      const fullPath = pathPrefix ? `${pathPrefix}/${entry.name}` : entry.name;
      if (entry.kind === 'file') {
        const file = await entry.getFile();
        const type = normalizeMediaType(file.type, file.name);
          const id = `fs-${fullPath}`; // STABLE ID based on path
          results.push({
            id,
            name: prettifyMediaName(file.name),
            path: fullPath,
            type,
            fileHandle: entry
          });
      } else if (entry.kind === 'directory') {
        await scanDirectoryRecursive(entry, results, fullPath);
      }
    }
    setMediaLibrary(prev => {
      const next = [...prev];
      results.forEach(newAsset => {
        const existingIdx = next.findIndex(a => a.path === newAsset.path);
        if (existingIdx !== -1) {
          // Update existing asset with new handle and any other data
          next[existingIdx] = { ...next[existingIdx], fileHandle: newAsset.fileHandle };
        } else {
          // Add as new
          next.push(newAsset);
        }
      });
      return next;
    });
  };

  const linkFileToItem = (file: any, overrideItemId?: string) => {
    const itemId = overrideItemId || importMenu?.itemId;
    if (!itemId) return;
    
    // Check if it already exists in library to avoid duplicates
    const existing = mediaLibrary.find(a => a.path === file.path);
    
    let linkedAsset: MediaAsset;
    
    if (existing) {
      linkedAsset = existing;
    } else {
      const assetId = `imp-${Date.now()}`;
      linkedAsset = {
        id: assetId,
        name: file.name, // Already prettified in handleImportClick mapping
        path: file.path, 
        type: file.type || (file.path?.match(/\.(mp4|mov|m4v)$/i) ? 'video' : 'image'),
        fileHandle: file.fileHandle
      };
      setMediaLibrary(prev => [...prev, linkedAsset]);
    }
    
    setSchedules(prev => {
      const type = state.meetingType;
      return {
        ...prev,
        [type]: prev[type].map(it => {
          if (it.id === itemId) {
            const currentIds = it.mediaIds || [];
            if (currentIds.includes(linkedAsset.id)) return it;
            return { ...it, mediaIds: [...currentIds, linkedAsset.id] };
          }
          return it;
        })
      };
    });
    
    if (!overrideItemId) {
      setImportMenu(null);
      setScanResults(null);
    }
  };

  const handleImportClick = async (itemId: string, type: 'video' | 'image', isJwImage: boolean = false) => {
    try {
      const isDesktop = !!(window as any).mediaflow?.isDesktop;
      
      if (isDesktop) {
        let defaultPath = undefined;
        if (isJwImage) {
           if (typeof (window as any).mediaflow.getPublicationsPath === 'function') {
             defaultPath = await (window as any).mediaflow.getPublicationsPath();
           } else if (typeof (window as any).mediaflow.getVideosPath === 'function') {
             const videosPath = await (window as any).mediaflow.getVideosPath();
             const basePath = videosPath.replace(/[\\/]Videos[\\/]?$/i, '');
             defaultPath = `${basePath}\\AppData\\Local\\Packages\\WatchtowerBibleandTractSo.45909CDBADF3C_5rz59y55nfz3e\\LocalState\\Publications`;
           }
           alert("Opening Publications Directory:\n" + defaultPath);
        }

        // Use native Electron picker to get absolute paths
        const results = await (window as any).mediaflow.pickFiles({
          filters: [
            { name: type === 'video' ? 'Videos' : 'Images', extensions: type === 'video' ? ['mp4', 'm4v', 'mov'] : ['jpg', 'png', 'jpeg', 'webp'] }
          ],
          defaultPath
        });
        
        if (results && results.length > 0) {
          const mapped = results.map((r: any) => ({
            name: prettifyMediaName(r.name),
            path: r.path,
            type: type,
          }));
          // In Desktop, since they explicitly picked the files, link them immediately
          mapped.forEach((m: any) => linkFileToItem(m, itemId));
        }
        return;
      }

      // Web fallback
      const handle = await (window as any).showDirectoryPicker({
        mode: 'read',
        startIn: type === 'video' ? 'videos' : 'pictures'
      });
      
      const results: any[] = [];
      for await (const entry of (handle as any).values()) {
        if (entry.kind === 'file') {
          const file = await entry.getFile();
          const normalizedType = normalizeMediaType(file.type || '', file.name);
          if (normalizedType === type) {
             results.push({
               name: prettifyMediaName(file.name),
               path: file.name,
               type: normalizedType,
               fileHandle: entry
             });
          }
        }
      }
      setScanResults(results);
      setImportMenu({ itemId, type });
    } catch (error) {
      console.error('Import scan failed:', error);
    }
  };

  const updateSong = async (itemId: string, songNumber: string) => {
    if (!songNumber) return;
    
    // Normalizing song number (e.g., "5" -> "005")
    const paddedNumber = songNumber.padStart(3, '0');
    
    // Try to find if we already have this song in the library
    // Match against: sjj_005, sjj_5, or just "5" in name
    const matchedAsset = mediaLibrary.find(a => 
      (a.type === 'audio' || a.type === 'video') && (
        (a.path || a.url || '').toLowerCase().includes(`sjj_${paddedNumber}`) || 
        (a.path || a.url || '').toLowerCase().includes(`sjj_${songNumber}`) ||
        (a.path || a.url || '').toLowerCase().includes(`song_${songNumber}`) ||
        (a.name || '').toLowerCase().startsWith(`${songNumber}.`) ||
        (a.name || '').toLowerCase() === `song ${songNumber}`
      )
    );

    let linkedId = matchedAsset ? matchedAsset.id : null;

    if (!linkedId && (window as any).mediaflow && typeof (window as any).mediaflow.getVideosPath === 'function') {
      try {
        const videosPath = await (window as any).mediaflow.getVideosPath();
        const filename = `sjjm_${state.language}_${paddedNumber}_r720P.mp4`;
        const fullPath = `${videosPath}\\JWLibrary\\${filename}`;
        
        const newId = `manual-song-${paddedNumber}-${state.language}`;
        const newAsset: MediaAsset = {
          id: newId,
          name: filename,
          path: fullPath,
          type: 'video'
        };
        
        setMediaLibrary(prev => {
          if (!prev.find(a => a.id === newId)) {
            return [...prev, newAsset];
          }
          return prev;
        });
        linkedId = newId;
      } catch (e) {
        console.error('Failed to construct hardcoded song path', e);
      }
    }

    setSchedules(prev => {
      const type = state.meetingType;
      const updatedSchedule = prev[type].map(it => {
        if (it.id === itemId) {
          // Update song number in title regardless of media match
          return { 
            ...it, 
            title: `Song ${songNumber}`, 
            mediaIds: linkedId ? [linkedId] : [] // Clear current mediaIds to represent the new song
          };
        }
        return it;
      });

      return {
        ...prev,
        [type]: updatedSchedule
      };
    });
    // Set active tab to let the user see the media source update
    setSelectedMeetingItemId(itemId);
  };

  const removeAsset = (assetId: string) => {
    setMediaLibrary(prev => prev.filter(a => a.id !== assetId));
    setSchedules(prev => ({
      midweek: prev.midweek.map(item => ({ ...item, mediaIds: item.mediaIds.filter(id => id !== assetId) })),
      weekend: prev.weekend.map(item => ({ ...item, mediaIds: item.mediaIds.filter(id => id !== assetId) }))
    }));
    if (state.previewAsset?.id === assetId) setState(s => ({ ...s, previewAsset: null }));
    if (state.programAsset?.id === assetId) setState(s => ({ ...s, programAsset: null }));
  };

  const clearOutlineMedia = () => {
    if (!confirm('Are you sure you want to clear all linked media from the current meeting outline?')) return;
    setSchedules(prev => ({
      ...prev,
      [state.meetingType]: prev[state.meetingType as 'midweek' | 'weekend'].map(item => ({ ...item, mediaIds: [] }))
    }));
  };

  const clearMediaLibrary = () => {
    if (!confirm('Are you sure you want to completely clear the media library? This will also unblink all media in the outline.')) return;
    setMediaLibrary([]);
  };

  const normalizeMediaType = (type: string, path: string): MediaType => {
    const t = (type || '').toLowerCase();
    const p = (path || '').toLowerCase();
    
    // Check MIME type first
    if (t.startsWith('video/')) return 'video';
    if (t.startsWith('audio/')) return 'audio';
    if (t.startsWith('image/')) return 'image';
    
    // Fallback to shorthands
    if (['video', 'v', 'mp4', 'mov', 'avi', 'mkv', 'm4v', 'webm', '3gp', 'mpg', 'mpeg', 'wmv'].includes(t)) return 'video';
    if (['audio', 'a', 'mp3', 'm4a', 'wav', 'ogg', 'aac', 'flac', 'm4p', 'opus', 'mp4a'].includes(t)) return 'audio';
    if (['image', 'i', 'jpg', 'jpeg', 'png', 'gif', 'webp', 'svg', 'bmp', 'ico', 'tiff'].includes(t)) return 'image';
    
    // Fallback to extension check
    const ext = p.split('.').pop() || '';
    if (['mp4', 'mov', 'avi', 'mkv', 'm4v', 'webm', '3gp', 'mpg', 'mpeg', 'wmv'].includes(ext)) return 'video';
    if (['mp3', 'm4a', 'wav', 'ogg', 'aac', 'flac', 'm4p', 'opus', 'mp4a'].includes(ext)) return 'audio';
    if (['jpg', 'jpeg', 'png', 'gif', 'webp', 'svg', 'bmp', 'ico', 'tiff'].includes(ext)) return 'image';
    
    return 'video'; // Default to video if unknown
  };

  const handleScan = async () => {
    try {
      const handle = await (window as any).showDirectoryPicker({ mode: 'read' });
      await scanDirectoryRecursive(handle);
    } catch (error) {
      console.error('Scan failed:', error);
    }
  };

  // Auto-link songs when media library changes
  useEffect(() => {
    const autoLinkSongs = () => {
      let changed = false;
      const updatedMidweek = schedules.midweek.map(item => {
        if (item.type === 'song' && item.title.startsWith('Song ') && item.mediaIds.length === 0) {
          const songNum = item.title.replace('Song ', '');
          const paddedNumber = songNum.padStart(3, '0');
          const matchedAsset = mediaLibrary.find(a => 
            (a.type === 'audio' || a.type === 'video') && (
              (a.path || a.url || '').toLowerCase().includes(`sjj_${paddedNumber}`) || 
              (a.path || a.url || '').toLowerCase().includes(`sjj_${songNum}`) ||
              (a.name || '').toLowerCase().startsWith(`${songNum}.`)
            )
          );
          if (matchedAsset) {
            changed = true;
            return { ...item, mediaIds: [matchedAsset.id] };
          }
        }
        return item;
      });

      const updatedWeekend = schedules.weekend.map(item => {
        if (item.type === 'song' && item.title.startsWith('Song ') && item.mediaIds.length === 0) {
          const songNum = item.title.replace('Song ', '');
          const paddedNumber = songNum.padStart(3, '0');
          const matchedAsset = mediaLibrary.find(a => 
            (a.type === 'audio' || a.type === 'video') && (
              (a.path || a.url || '').toLowerCase().includes(`sjj_${paddedNumber}`) || 
              (a.path || a.url || '').toLowerCase().includes(`sjj_${songNum}`) ||
              (a.name || '').toLowerCase().startsWith(`${songNum}.`)
            )
          );
          if (matchedAsset) {
            changed = true;
            return { ...item, mediaIds: [matchedAsset.id] };
          }
        }
        return item;
      });

      if (changed) {
        setSchedules({ midweek: updatedMidweek, weekend: updatedWeekend });
      }
    };

    if (mediaLibrary.length > 0) {
      autoLinkSongs();
    }
  }, [mediaLibrary, schedules.midweek, schedules.weekend]);

  useEffect(() => {
    if (!(window as any).mediaflow) return;
    
    const checkScreens = async () => {
      try {
        const snapshot = await (window as any).mediaflow.getSystemSnapshot();
        setHasSecondaryScreen(snapshot.displays.length > 1);
      } catch (err) {
        console.error('Failed to check screens:', err);
      }
    };

    checkScreens();
    
    const unsubscribe = (window as any).mediaflow.onDisplayChange((displays: any[]) => {
      setHasSecondaryScreen(displays.length > 1);
    });

    return () => unsubscribe();
  }, []);

  const { send } = useSyncChannel(async (msg) => {
    if (msg.type === 'AUDIENCE_READY' || (msg as any).type === 'AUDIENCE_ALIVE') {
      setIsAudienceLive(true);
      setLastAudienceSignal(Date.now());
      send({ type: 'SYNC_STATE', state });

  
    } else if (msg.type === 'BGM_ACTION' && msg.action === 'seek') {
      const player = document.getElementById('bgm-monitor') as HTMLMediaElement;
      if (player) {
        player.currentTime = Math.max(0, player.currentTime + msg.offset);
      }
    } else if (msg.type === 'RESOLVE_ASSET') {
      const api = (window as any).__MEDIAFLOW_API__;
      if (api && typeof api.resolveAssetUrlById === 'function') {
        const { url } = await api.resolveAssetUrlById(msg.assetId);
        send({ type: 'ASSET_RESOLVED', assetId: msg.assetId, url, file: null });
      }
    } else if (msg.type === 'PROGRAM_ENDED') {
      setState(s => (
        s.programAsset?.id === msg.assetId
          ? { ...s, programAsset: null, isProgramPaused: false }
          : s
      ));
    }
  });

  // Expose send globally for jumpBg etc.
  useEffect(() => {
    (window as any).__SYNC_SEND__ = send;
  }, [send]);

  useEffect(() => {
    let interval: NodeJS.Timeout;
    if (state.timer.isRunning && state.timer.seconds > 0) {
      interval = setInterval(() => {
        setState(s => {
          if (s.timer.seconds <= 0) return s;
          return {
            ...s,
            timer: { ...s.timer, seconds: Math.max(0, s.timer.seconds - 1) },
            ...(s.timer.seconds <= 1 ? { timer: { ...s.timer, seconds: 0, isRunning: false } } : {})
          };
        });
      }, 1000);
    }
    return () => clearInterval(interval);
  }, [state.timer.isRunning, state.timer.seconds]);

  // Sync state to audience whenever it changes (with throttling)
  useEffect(() => {
    const now = Date.now();
    // Throttle syncs to 500ms min, unless it's a critical state change
    if (now - lastSyncRef.current < 500 && state.timer.isRunning) return;
    
    lastSyncRef.current = now;

    // Clean state of non-serializable objects (like FileSystemHandle)
    const stripHandles = (asset: MediaAsset | null) => {
      if (!asset) return null;
      const { fileHandle, ...rest } = asset;
      return rest;
    };

    const serializableState = {
      ...state,
      previewAsset: stripHandles(state.previewAsset),
      programAsset: stripHandles(state.programAsset),
      bgmAsset: stripHandles(state.bgmAsset)
    };

    send({ type: 'SYNC_STATE', state: serializableState as AppState });
  }, [state, send]);

  

  const formatTime = (seconds: number) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
  };

  const handleTake = () => {
    if (state.previewAsset) {
      setState(s => ({ ...s, programAsset: s.previewAsset, isProgramPaused: false }));
    }
  };

  const handleCut = () => {
    setState(s => ({ ...s, programAsset: null, isProgramPaused: false }));
  };

  const setUnifiedVolume = (volume: number) => {
    setState(s => ({
      ...s,
      mixer: {
        ...s.mixer,
        masterVolume: volume
      }
    }));
  };

  const toggleUnifiedMute = () => {
    setState(s => ({
      ...s,
      mixer: {
        ...s.mixer,
        isMuted: !s.mixer.isMuted
      }
    }));
  };

  useEffect(() => {
    const interval = setInterval(() => {
      if (isAudienceLive && Date.now() - lastAudienceSignal > 5000) {
        setIsAudienceLive(false);
      }
    }, 1000);
    return () => clearInterval(interval);
  }, [isAudienceLive, lastAudienceSignal]);

  const openAudienceFeed = async () => {
    try {
      if ((window as any).mediaflow) {
        if (typeof (window as any).mediaflow.openAudienceDisplay === 'function') {
          await (window as any).mediaflow.openAudienceDisplay();
        } else if (typeof (window as any).mediaflow.openExternalDisplay === 'function') {
          await (window as any).mediaflow.openExternalDisplay();
        } else {
          window.open('/?view=audience', 'AudienceView', 'width=1280,height=720');
        }
      } else {
        window.open('/?view=audience', 'AudienceView', 'width=1280,height=720');
      }
    } catch (err) {
      console.error('Failed to open external display:', err);
      try {
        window.open('/?view=audience', 'AudienceView', 'width=1280,height=720');
      } catch (e2) {
        alert("Broadcast extension window blocked. Please disable popup blockers.");
      }
    }
  };

  const openZoomFeed = async () => {
    try {
      if ((window as any).mediaflow) {
        if (typeof (window as any).mediaflow.openZoomDisplay === 'function') {
          await (window as any).mediaflow.openZoomDisplay();
        } else if (typeof (window as any).mediaflow.openExternalDisplay === 'function') {
          await (window as any).mediaflow.openExternalDisplay('zoom');
        } else {
          window.open('/?view=zoom', 'ZoomFeed', 'width=1920,height=1080');
        }
      } else {
        window.open('/?view=zoom', 'ZoomFeed', 'width=1920,height=1080');
      }
    } catch (err) {
      console.error('Failed to open zoom display:', err);
      try {
        if ((window as any).mediaflow && typeof (window as any).mediaflow.openExternalDisplay === 'function') {
          await (window as any).mediaflow.openExternalDisplay('zoom');
        } else {
          window.open('/?view=zoom', 'ZoomFeed', 'width=1920,height=1080');
        }
      } catch (fallbackErr) {
        console.error('Zoom fallback failed:', fallbackErr);
      }
    }
  };

  const handleManualFileSelect = async () => {
    try {
      if ((window as any).mediaflow?.isDesktop) {
        const files = await (window as any).mediaflow.pickFiles({
          filters: [
            { name: 'Media Files', extensions: ['mp4', 'webm', 'mov', 'jpg', 'jpeg', 'png', 'gif', 'webp', 'mp3', 'wav', 'ogg', 'm4a'] }
          ]
        });
        if (files && files.length > 0) {
          const file = files[0];
          const type = normalizeMediaType('', file.name);
          setManualEntry(prev => ({ 
            ...prev, 
            name: file.name, 
            type: type as any,
            path: file.path
          }));
        }
      } else {
        const [handle] = await (window as any).showOpenFilePicker({
          types: [
            {
              description: 'Media Files',
              accept: {
                'video/*': ['.mp4', '.webm', '.mov'],
                'image/*': ['.jpg', '.jpeg', '.png', '.gif', '.webp'],
                'audio/*': ['.mp3', '.wav', '.ogg', '.m4a']
              }
            }
          ]
        });
        const file = await handle.getFile();
        const type = normalizeMediaType(file.type, file.name);
        setManualEntry(prev => ({ 
          ...prev, 
          name: file.name, 
          type: type as any,
          fileHandle: handle 
        }));
      }
    } catch (err) {
      console.error("File selection failed:", err);
    }
  };

  return (
    <div className="flex flex-col h-screen overflow-hidden font-sans text-sm bg-black relative">
      
      {/* Background Audio Engine for Operator */}
      {state.bgmAsset && (
        <div style={{ 
          position: "absolute", 
          top: 0, right: 0, 
          zIndex: 100,
          background: "rgba(0,0,0,0.8)",
          display: "flex",
          pointerEvents: "auto"
        }}
        className={(!state.isPlayingBgm || state.bgmAsset?.id) ? "" : "pointer-events-none opacity-0 w-0 h-0"}
        >
          {/* We only want it visible if it's NOT playing or there's an error, 
              but for now let's just make it visible if we suspect an issue.
              Actually, let's keep it invisible normally but provide a way to see it. */}
          <div className="sr-only">
            <SmartMedia 
              asset={state.bgmAsset}
              id="bgm-monitor"
              autoPlay={state.isPlayingBgm}
              muted={state.mixer.isMuted}
              volume={state.mixer.masterVolume}
              seekTo={state.bgmSeekTo}
              channelOneOutput={true}
              monitorMuted={monitorMuted}
              onEnd={nextBg}
            />
          </div>
        </div>
      )}

      {/* Permission Overlay */}
      {!state.isPermissionGranted && (
        <motion.div 
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          className="absolute inset-0 z-[200] bg-neutral-900/40 backdrop-blur-3xl flex items-center justify-center p-8 text-center"
        >
          <div className="max-w-md w-full p-12 bg-black rounded-3xl border border-white/10 shadow-2xl flex flex-col items-center gap-8">
            <div className="w-24 h-24 rounded-full bg-blue-500/10 flex items-center justify-center relative overflow-hidden group">
                <div className="absolute inset-0 bg-blue-500/20 animate-ping opacity-20" />
                <Unlock size={48} className="text-blue-500 relative z-10" />
            </div>
            <div className="space-y-4">
              <h2 className="text-2xl font-black text-white uppercase tracking-widest leading-tight">System Setup Required</h2>
              <p className="text-sm text-white/40 leading-relaxed italic">
                Broadcast Suite needs direct access to your local media directories to enable playback and sync across displays.
              </p>
            </div>
            <div className="w-full h-px bg-white/5" />
            <div className="space-y-4 w-full">
                <button 
                  onClick={requestPermissions}
                  className="w-full py-4 bg-blue-500 hover:bg-blue-400 text-white font-black uppercase tracking-widest rounded-2xl transition-all shadow-xl shadow-blue-500/20 active:scale-95 flex items-center justify-center gap-3"
                >
                  <ShieldCheck size={18} />
                  Grant Media Access
                </button>
                <div className="space-y-2 pt-4">
                  <div className="flex items-center justify-center gap-2">
                    <AlertCircle size={10} className="text-blue-400/40" />
                    <p className="text-[10px] text-white/20 uppercase tracking-[0.2em]">Select JWLibrary or Publications</p>
                  </div>
                  <div className="bg-white/5 rounded-xl p-3 text-left space-y-2 border border-white/5">
                    <p className="text-[8px] font-black text-white/30 uppercase tracking-[0.1em]">Recommended Path (Videos):</p>
                    <code className="text-[8px] font-mono text-blue-400/60 break-all select-all block bg-black/40 p-1.5 rounded leading-tight">C:\Users\SETH\Videos\JWLibrary</code>
                    <p className="text-[8px] font-black text-white/30 uppercase tracking-[0.1em] pt-1">Recommended Path (Images/Pubs):</p>
                    <code className="text-[8px] font-mono text-blue-400/60 break-all select-all block bg-black/40 p-1.5 rounded leading-tight">C:\Users\SETH\...\LocalState\Publications</code>
                  </div>
                </div>
            </div>
          </div>
        </motion.div>
      )}
      {/* Navbar etc. (simplified for now but maintaining the design vibe) */}
      <header className="flex items-center justify-between px-4 py-2 border-b bg-neutral-900/50 backdrop-blur-md h-16 z-40">
        <div className="flex items-center gap-6">
           <div className="flex items-center gap-3">
             <div className="w-8 h-8 bg-blue-500 rounded-lg flex items-center justify-center font-bold text-sm shadow-lg shadow-blue-500/20">MF</div>
             <div>
               <h1 className="font-bold tracking-tighter text-white leading-none">MediaFlow</h1>
               <p className="text-[10px] uppercase tracking-[0.2em] text-blue-400 font-bold mt-1">Broadcast Suite</p>
             </div>
           </div>

           <div className="flex items-center bg-white/5 rounded-lg p-1 border border-white/10 ml-4">
              <button 
                onClick={() => setState(s => ({ ...s, meetingType: 'midweek' }))}
                className={`px-4 py-1.5 rounded-md text-[10px] font-black uppercase tracking-widest transition-all ${state.meetingType === 'midweek' ? 'bg-blue-500 text-white shadow-lg' : 'text-white/40 hover:text-white/60'}`}
              >
                Midweek
              </button>
              <button 
                onClick={() => setState(s => ({ ...s, meetingType: 'weekend' }))}
                className={`px-4 py-1.5 rounded-md text-[10px] font-black uppercase tracking-widest transition-all ${state.meetingType === 'weekend' ? 'bg-blue-500 text-white shadow-lg' : 'text-white/40 hover:text-white/60'}`}
              >
                Weekend
              </button>
           </div>

           <div className="relative ml-4">
             <button 
               onClick={() => setShowLanguageMenu(!showLanguageMenu)}
               className="flex items-center gap-2 px-3 py-1.5 bg-white/5 hover:bg-white/10 text-white/60 text-[10px] font-black rounded-lg border border-white/10 uppercase tracking-widest transition-all"
             >
               <Globe size={12} className="text-blue-400" />
               {state.language === 'E' ? 'English' : 'Ewe'}
               <ChevronDown size={10} className={`transition-transform ${showLanguageMenu ? 'rotate-180' : ''}`} />
             </button>
             
             <AnimatePresence>
               {showLanguageMenu && (
                 <motion.div 
                   initial={{ opacity: 0, y: 10 }}
                   animate={{ opacity: 1, y: 0 }}
                   exit={{ opacity: 0, y: 10 }}
                   className="absolute top-full left-0 mt-2 w-32 bg-neutral-900 border border-white/10 rounded-xl shadow-2xl overflow-hidden py-1 z-50"
                 >
                   <button 
                     onClick={() => {
                       setState(s => ({ ...s, language: 'E' }));
                       setShowLanguageMenu(false);
                     }}
                     className={`w-full text-left px-4 py-2 text-[10px] font-black uppercase tracking-widest transition-colors ${state.language === 'E' ? 'bg-blue-500 text-white' : 'text-white/40 hover:bg-white/5 hover:text-white'}`}
                   >
                     English
                   </button>
                   <button 
                     onClick={() => {
                       setState(s => ({ ...s, language: 'EW' }));
                       setShowLanguageMenu(false);
                     }}
                     className={`w-full text-left px-4 py-2 text-[10px] font-black uppercase tracking-widest transition-colors ${state.language === 'EW' ? 'bg-blue-500 text-white' : 'text-white/40 hover:bg-white/5 hover:text-white'}`}
                   >
                     Ewe
                   </button>
                 </motion.div>
               )}
             </AnimatePresence>
           </div>
        </div>

        {/* Audio Controls Dropdown */}
        <div className="relative ml-auto mr-4 flex items-center gap-2">
          <button 
            onClick={() => setShowAudioPanel(!showAudioPanel)}
            className={`flex items-center gap-2 px-3 py-1.5 rounded-lg border transition-all text-[10px] font-black uppercase tracking-widest ${
              showAudioPanel 
                ? 'bg-blue-500/20 border-blue-500/40 text-blue-400' 
                : 'bg-white/5 hover:bg-white/10 border-white/10 text-white/50 hover:text-white/70'
            }`}
          >
            {state.mixer.isMuted
              ? <VolumeX size={14} className="text-red-400" /> 
              : <Volume2 size={14} className="text-blue-400" />
            } 
            <span className="hidden md:inline">Audio</span>
            <span className="text-[9px] font-mono text-blue-400/70">{state.mixer.masterVolume}%</span>
            {showAudioPanel ? <ChevronUp size={10} /> : <ChevronDown size={10} />}
          </button>

          <button 
            onClick={() => setMonitorMuted(!monitorMuted)}
            className={`flex items-center gap-2 px-3 py-1.5 rounded-lg border transition-all text-[10px] font-black uppercase tracking-widest ${
              !monitorMuted 
                ? 'bg-emerald-500/20 border-emerald-500/40 text-emerald-400 shadow-[0_0_15px_rgba(16,185,129,0.2)]' 
                : 'bg-red-500/10 border-red-500/20 text-red-500/40 hover:text-red-500/60'
            }`}
            title={monitorMuted ? "Unmute local operator speakers" : "Mute local operator speakers"}
          >
            {monitorMuted ? <VolumeX size={14} /> : <Volume2 size={14} className="animate-pulse" />}
            <span>Operator</span>
            <div className={`w-1.5 h-1.5 rounded-full ${!monitorMuted ? 'bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.5)]' : 'bg-red-500/20'}`} />
          </button>

          <AnimatePresence>
            {showAudioPanel && (
              <motion.div
                initial={{ opacity: 0, y: 8, scale: 0.95 }}
                animate={{ opacity: 1, y: 0, scale: 1 }}
                exit={{ opacity: 0, y: 8, scale: 0.95 }}
                transition={{ duration: 0.15 }}
                className="absolute top-full right-0 mt-2 w-72 bg-neutral-900/95 backdrop-blur-xl border border-white/10 rounded-2xl shadow-2xl shadow-black/50 overflow-hidden z-50"
              >
                <div className="px-4 py-3 border-b border-white/5 bg-white/[0.02]">
                  <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40 flex items-center gap-2">
                    <Volume2 size={12} className="text-blue-500" />
                    Sound Mixer
                  </span>
                </div>
                <div className="p-4 space-y-4">
                  <div className="space-y-2">
                    <div className="flex justify-between items-center text-[9px] font-black uppercase tracking-widest">
                      <span className="text-blue-400">Program Volume</span>
                      <span className="text-white/40 font-mono">{state.mixer.masterVolume}%</span>
                    </div>
                    <div className="flex items-center gap-3">
                      <button 
                        onClick={toggleUnifiedMute}
                        className={`p-1.5 rounded-lg transition-colors ${state.mixer.isMuted ? 'bg-red-500/20 text-red-500' : 'bg-white/5 text-white/40 hover:text-white'}`}
                      >
                        {state.mixer.isMuted ? <VolumeX size={14} /> : <Volume2 size={14} />}
                      </button>
                      <input 
                        type="range"
                        className="flex-1 accent-blue-500 appearance-none bg-white/5 h-1 rounded-full cursor-pointer"
                        min="0" max="100"
                        value={state.mixer.masterVolume}
                        onChange={(e) => setUnifiedVolume(parseInt(e.target.value))}
                      />
                    </div>
                  </div>

                  <div className="h-px bg-white/5" />

                  <div className="rounded-lg border border-white/5 bg-black/30 p-3">
                    <div className="flex items-center justify-between gap-3">
                      <span className="text-[8px] font-black uppercase tracking-[0.2em] text-white/30">Audio Output</span>
                      <span className="min-w-0 truncate text-right text-[9px] font-mono text-white/60" title={audioOutputName}>
                        {audioOutputName}
                      </span>
                    </div>
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </div>

        <div className="flex items-center gap-6">
           <AnimatePresence>
             {scanningStatus && (
                <motion.div 
                  initial={{ opacity: 0, x: 20 }}
                  animate={{ opacity: 1, x: 0 }}
                  exit={{ opacity: 0, x: -20 }}
                  className="flex items-center gap-2 px-3 py-1 bg-blue-500/10 border border-blue-500/20 rounded-full"
                >
                  <div className="w-1.5 h-1.5 rounded-full bg-blue-500 animate-pulse" />
                  <span className="text-[9px] font-black text-blue-400 uppercase tracking-widest">{scanningStatus}</span>
                </motion.div>
             )}
           </AnimatePresence>
           
           <div className="flex items-center gap-6 text-[10px] font-mono font-medium text-white/30 border-r border-white/10 pr-6 mr-2">
              <div className="flex items-center gap-2">
                <div className={`w-1.5 h-1.5 rounded-full ${hasSecondaryScreen ? 'bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.5)]' : 'bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.5)]'}`} /> 
                DISPLAY 2: {hasSecondaryScreen ? 'READY' : 'OFFLINE'}
              </div>
              <div className="flex items-center gap-2">
                <div className="w-1.5 h-1.5 rounded-full bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.5)]" /> 
                SYNC: OK
              </div>
            </div>

        <div className="flex items-center gap-4">
              {/* Session Controls */}
              <div className="flex items-center gap-1.5 p-1 bg-white/5 rounded-xl border border-white/10">
                <button 
                  onClick={() => setState(s => ({ ...s, isMeetingLive: !s.isMeetingLive }))}
                  className={`flex items-center gap-2 px-3 py-1.5 rounded-lg text-[10px] font-black uppercase tracking-widest transition-all ${state.isMeetingLive ? 'bg-red-600 text-white shadow-lg shadow-red-600/40' : 'text-white/40 hover:text-white/60'}`}
                >
                  <Activity size={12} className={state.isMeetingLive ? 'animate-pulse' : ''} />
                  {state.isMeetingLive ? 'Meeting Live' : 'Go Live'}
                </button>
                <div className="w-px h-4 bg-white/10 mx-1" />
                <button 
                  onClick={() => setShowSettings(true)}
                  className="p-2 bg-transparent hover:bg-white/5 text-white/40 hover:text-white rounded-lg transition-all"
                  title="Settings"
                >
                  <Settings size={14} />
                </button>
                <button 
                  onClick={() => {
                    if (confirm('Are you sure you want to clear all saved data and reset the application?')) {
                      localStorage.clear();
                      window.location.reload();
                    }
                  }}
                  className="p-2 bg-transparent hover:bg-red-500/10 text-white/20 hover:text-red-500 rounded-lg transition-all"
                  title="Factory Reset"
                >
                  <RotateCw size={14} />
                </button>
              </div>

              {/* Broadcast Group */}
              <div className="flex items-center gap-3 pl-4 border-l border-white/10">
                <div className="flex flex-col gap-1">
                  <button 
                    onClick={openZoomFeed}
                    className={`
                      relative flex items-center gap-3 px-4 py-2 rounded-xl text-[10px] font-black uppercase tracking-widest transition-all overflow-hidden
                      ${bridgeStatus.status === 'active' ? 'bg-zinc-900 text-white border border-emerald-500/50' : 
                        bridgeStatus.status === 'error' ? 'bg-zinc-900 text-red-500 border border-red-500/50' :
                        bridgeStatus.status === 'starting' ? 'bg-zinc-900 text-blue-300 border border-blue-500/50' :
                        'bg-zinc-800 hover:bg-zinc-700 text-white/60 border border-white/10'}
                    `}
                  >
                    <div className={`w-2 h-2 rounded-full ${
                      bridgeStatus.status === 'active' ? 'bg-emerald-500 animate-pulse' : 
                      bridgeStatus.status === 'error' ? 'bg-red-500' :
                      bridgeStatus.status === 'starting' ? 'bg-blue-500 animate-pulse' : 'bg-white/10'
                    }`} />
                    <span>
                      {bridgeStatus.status === 'active' ? 'Broadcast Live' :
                       bridgeStatus.status === 'starting' ? 'Starting Zoom Feed' :
                       bridgeStatus.status === 'error' ? 'Zoom Feed Error' :
                       'Broadcast to Zoom'}
                    </span>
                  </button>
                  {bridgeStatus.status === 'error' && bridgeStatus.message && (
                    <p className="max-w-72 text-[8px] font-bold uppercase tracking-wider text-red-400/80 leading-tight" title={bridgeStatus.message}>
                      {bridgeStatus.message}
                    </p>
                  )}
                  
                  <div className="flex bg-black/40 p-0.5 rounded-lg border border-white/5">
                    <button 
                      onClick={() => setState(s => ({ ...s, vcamMode: 'auto' }))}
                      className={`flex-1 py-1 px-2 rounded-md text-[7px] font-bold uppercase transition-all ${state.vcamMode === 'auto' ? 'bg-white/10 text-white' : 'text-white/30 hover:text-white/50'}`}
                    >Auto</button>
                    <button 
                      onClick={() => setState(s => ({ ...s, vcamMode: 'camera' }))}
                      className={`flex-1 py-1 px-2 rounded-md text-[7px] font-bold uppercase transition-all ${state.vcamMode === 'camera' ? 'bg-emerald-500/20 text-emerald-400' : 'text-white/30 hover:text-white/50'}`}
                    >Cam</button>
                    <button 
                      onClick={() => setState(s => ({ ...s, vcamMode: 'media' }))}
                      className={`flex-1 py-1 px-2 rounded-md text-[7px] font-bold uppercase transition-all ${state.vcamMode === 'media' ? 'bg-blue-500/20 text-blue-400' : 'text-white/30 hover:text-white/50'}`}
                    >Media</button>
                  </div>
                </div>

                <button 
                  onClick={openAudienceFeed}
                  className="px-4 py-2 bg-white/5 hover:bg-white/10 text-white/60 text-[10px] font-black rounded-xl border border-white/10 uppercase tracking-widest transition-all h-full"
                >
                  Extend Feed
                </button>
              </div>
            </div>
        </div>
      </header>

      <main className="flex-1 flex flex-col min-h-0 relative z-10">
        <div className="grid grid-cols-2 flex-1 p-6 gap-6">
          {/* Preview Screen */}
          <section className="relative border border-white/5 bg-neutral-900/40 rounded-xl overflow-hidden flex flex-col group shadow-inner">
             <div className="absolute top-4 left-4 z-10">
               <span className="bg-blue-500/20 text-blue-400 ring-1 ring-blue-500/40 px-3 py-1 rounded-full text-[10px] font-black tracking-[0.2em] uppercase backdrop-blur-md">Preview</span>
             </div>
             <div className="flex-1 flex items-center justify-center bg-zinc-950 relative overflow-hidden">
                    {state.previewAsset ? (
                      <motion.div 
                        key={state.previewAsset.id}
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        className="absolute inset-0 z-0 flex items-center justify-center bg-black"
                      >
                        {renderMediaThumbnail(state.previewAsset, 'w-full h-full object-contain')}
                        <div className="absolute bottom-5 left-1/2 -translate-x-1/2 z-20 px-3 py-1.5 rounded-full bg-black/70 border border-blue-400/20 text-[8px] font-black uppercase tracking-widest text-blue-200/70">
                          Thumbnail Preview
                        </div>
                      </motion.div>
                    ) : null}

                    {/* Overlay for Timer when active */}
                    <AnimatePresence>
                      {(state.previewTimer || state.timer.isRunning) && (
                        <motion.div 
                          key="timer-preview"
                          initial={{ opacity: 0, scale: 0.9 }}
                          animate={{ opacity: 1, scale: 1 }}
                          exit={{ opacity: 0, scale: 1.1 }}
                          className="relative z-20 flex flex-col items-center bg-black/60 backdrop-blur-sm inset-0 absolute flex items-center justify-center"
                        >
                          <div className="text-7xl font-mono font-black text-white tracking-widest drop-shadow-[0_0_30px_rgba(59,130,246,0.5)] mb-4">
                            {formatTime(state.timer.seconds)}
                          </div>
                          <div className={`px-4 py-1.5 rounded-full border ${state.timer.isRunning ? 'bg-red-500/20 border-red-500/40 text-red-400' : 'bg-blue-500/20 border-blue-500/40 text-blue-400'} text-[10px] font-black uppercase tracking-widest`}>
                            {state.timer.isRunning ? 'Broadcasting Focus' : 'Timer Staged / Preview'}
                          </div>
                        </motion.div>
                      )}
                    </AnimatePresence>

                    {!state.previewAsset && !state.previewTimer && !state.timer.isRunning && (
                      <div className="text-white/10 flex flex-col items-center gap-4 z-10">
                        <Monitor size={64} strokeWidth={0.5} />
                        <span className="font-mono text-[10px] uppercase tracking-[0.4em]">Standby • No Signal</span>
                      </div>
                    )}
             </div>
             <div className="h-12 border-t border-white/5 bg-black/60 backdrop-blur flex items-center justify-between px-4">
               <div className="flex items-center gap-4">
                <span className="text-[10px] font-mono text-white/20 uppercase tracking-widest font-medium">Internal Routing: Bus 1</span>
                {state.previewAsset && (
                  <span className="border-l border-white/10 pl-4 text-[9px] font-black uppercase tracking-widest text-blue-300/50">
                    Thumbnail staged
                  </span>
                )}
               </div>
               <div className="flex gap-2">
                 <button 
                  onClick={handleCut}
                  className={`h-7 px-4 text-[10px] font-black rounded-full transition-all uppercase tracking-widest border border-red-500/30 ${state.programAsset ? 'bg-red-500 hover:bg-red-400 text-white' : 'bg-white/5 text-white/20 cursor-not-allowed'}`}
                 >
                   Cut Live
                 </button>
                 <button 
                  onClick={handleTake}
                  className={`h-7 px-4 text-[10px] font-black rounded-full transition-all uppercase tracking-widest shadow-lg ${state.previewAsset ? 'bg-blue-500 hover:bg-blue-400 text-white shadow-blue-500/20' : 'bg-white/5 text-white/20 cursor-not-allowed'}`}
                 >
                   Take Live
                 </button>
               </div>
             </div>
          </section>

          {/* Program Screen */}
          <section className="relative border-2 border-red-500/40 bg-zinc-950 rounded-xl overflow-hidden flex flex-col shadow-[0_0_80px_rgba(239,68,68,0.08)]">
             <div className="absolute top-4 left-4 z-10 flex items-center gap-2">
               <div className="w-2 h-2 rounded-full bg-red-500 animate-ping" />
               <span className="bg-red-500 text-white px-3 py-1 rounded-full text-[10px] font-black tracking-[0.2em] uppercase shadow-lg shadow-red-500/20">Live</span>
               {state.programAsset && (
                 <motion.div 
                   initial={{ opacity: 0, x: -10 }}
                   animate={{ opacity: 1, x: 0 }}
                   className="ml-2 flex items-center gap-1.5 px-2 py-1 bg-emerald-500/10 border border-emerald-500/20 rounded-lg"
                 >
                    <Activity size={10} className="text-emerald-500" />
                    <span className="text-[8px] font-black text-emerald-500 uppercase tracking-widest">Feed Confirmed • OK</span>
                 </motion.div>
               )}
             </div>
             <div className="flex-1 flex items-center justify-center bg-black relative">
                <AnimatePresence mode="wait">
                  {state.programAsset ? (
                     <motion.div 
                        key={state.programAsset.id}
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        exit={{ opacity: 0 }}
                        transition={{ duration: 0.8, ease: "easeInOut" }}
                        className="absolute inset-0 flex items-center justify-center"
                      >
                        <SmartMedia 
                          asset={state.programAsset}
                          className="w-full h-full object-contain"
                          autoPlay={!state.isProgramPaused}
                          muted={state.mixer.isMuted}
                          volume={state.mixer.masterVolume}
                          channelOneOutput={true}
                          monitorMuted={monitorMuted}
                          controls={false}
                          deviceId={state.selectedCameraId}
                          onEnd={handleCut}
                        />
                      </motion.div>
                  ) : (
                    <motion.div 
                      key="idle"
                      initial={{ opacity: 0 }}
                      animate={{ opacity: 1 }}
                      exit={{ opacity: 0 }}
                      transition={{ duration: 0.8 }}
                      className="text-center z-10"
                    >
                      <p className="text-white/20 font-mono uppercase tracking-[0.4em] text-[10px] font-bold">Broadcast Pipeline Active</p>
                      <p className="text-white/10 text-[9px] font-mono mt-2 italic">Idle</p>
                    </motion.div>
                  )}
                </AnimatePresence>
             </div>
             <div className="h-12 border-t border-red-500/20 bg-red-500/5 backdrop-blur flex items-center justify-between px-4">
                <div className="flex items-center gap-2">
                  <div className="w-2 h-2 rounded-full bg-red-500 shadow-[0_0_8px_rgba(239,68,68,1)]" />
                  <span className="text-[10px] font-mono text-red-500 font-black uppercase tracking-widest">Master Feed</span>
                </div>
                <div className="flex items-center gap-2">
                  {state.programAsset && (
                    <button
                      onClick={() => setState(s => ({ ...s, isProgramPaused: !s.isProgramPaused }))}
                      className={`h-8 px-3 rounded-full border flex items-center gap-2 text-[9px] font-black uppercase tracking-widest transition-all ${
                        state.isProgramPaused
                          ? 'bg-amber-500/20 border-amber-500/40 text-amber-300 hover:bg-amber-500/30'
                          : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10 hover:text-white'
                      }`}
                      title={state.isProgramPaused ? 'Play live feed' : 'Pause live feed'}
                    >
                      {state.isProgramPaused ? <Play size={11} fill="currentColor" /> : <Pause size={11} fill="currentColor" />}
                      <span>{state.isProgramPaused ? 'Play Feed' : 'Pause Feed'}</span>
                    </button>
                  )}
                </div>
                <div className={`flex items-center gap-1.5 px-2 py-0.5 rounded border ${isAudienceLive ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-500' : 'bg-white/5 border-white/10 text-white/20'}`}>
                  <div className={`w-1 h-1 rounded-full ${isAudienceLive ? 'bg-emerald-500 animate-pulse' : 'bg-white/20'}`} />
                  <span className="text-[8px] font-mono font-black uppercase tracking-tighter">
                    AUDIENCE: {isAudienceLive ? 'LIVE' : 'OFFLINE'}
                  </span>
                </div>
                <span className="text-[10px] font-mono text-white/20 uppercase">4.2gb/s</span>
             </div>
          </section>
        </div>

        <div className="h-[430px] grid grid-cols-3 gap-6 px-6 pb-6">
           <section className="col-span-1 border border-white/5 bg-neutral-900/40 rounded-xl flex flex-col overflow-hidden backdrop-blur-md group relative min-h-0">
             <div className="px-4 py-3 border-b border-white/5 bg-white/[0.02] flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <Database size={14} className="text-blue-500" />
                  <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40">Media Source</span>
                  <button 
                    onClick={() => setShowAllMedia(!showAllMedia)}
                    className={`ml-2 px-2 py-0.5 rounded text-[8px] font-black uppercase tracking-widest ${showAllMedia ? 'bg-blue-500 text-white' : 'bg-white/5 text-white/40'}`}
                  >
                    {showAllMedia ? 'All' : 'Staged'}
                  </button>
                  <button 
                    onClick={clearMediaLibrary}
                    className="ml-1 px-2 py-0.5 rounded bg-red-500/10 hover:bg-red-500/20 text-red-500 text-[8px] font-black uppercase tracking-widest border border-red-500/20 transition-all opacity-40 hover:opacity-100"
                    title="Clear Media Library"
                  >
                    <Trash2 size={8} />
                  </button>
                  {requiresRescan && (
                    <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-yellow-500/10 border border-yellow-500/20 ml-2 animate-pulse">
                       <AlertCircle size={8} className="text-yellow-500" />
                       <span className="text-[7px] font-black text-yellow-500 uppercase tracking-widest whitespace-nowrap">Rescan Required</span>
                    </div>
                  )}
                </div>
                <div className="flex items-center gap-2">
                  <button 
                    onClick={requestPermissions}
                    className="p-1.5 hover:bg-white/10 rounded-lg text-white/20 hover:text-blue-400 transition-all border border-white/5"
                    title="Add Folder"
                  >
                    <FolderPlus size={14} />
                  </button>
                  <button 
                    onClick={rescannAllFolders}
                    className="p-1.5 hover:bg-white/10 rounded-lg text-white/20 hover:text-white transition-all border border-white/5"
                    title="Scan"
                  >
                    <Activity size={14} />
                  </button>
                </div>
             </div>
             
             <div className="flex-1 overflow-y-auto p-3 scrollbar-thin">
                {filteredMedia.length >= 0 ? (
                  <div className="grid grid-cols-2 gap-3">
                    {/* Live Webcam Source */}
                    <div 
                      onClick={() => {
                        const webcamAsset: MediaAsset = {
                          id: 'webcam-primary',
                          name: 'LIVE WEBCAM',
                          type: 'camera'
                        };
                        setState(s => ({ ...s, previewAsset: webcamAsset }));
                      }}
                      className={`group relative aspect-video rounded-xl overflow-hidden border transition-all cursor-pointer ${state.previewAsset?.id === 'webcam-primary' ? 'border-emerald-500 shadow-[0_0_15px_rgba(16,185,129,0.5)] ring-1 ring-emerald-500' : 'border-white/5 bg-emerald-500/5 hover:border-emerald-500/20'}`}
                    >
                      <div className="absolute inset-0 z-0 pointer-events-none">
                        <SmartMedia asset={{ id: 'webcam-thumb', name: 'Webcam', type: 'camera' }} className="w-full h-full object-cover" muted={true} isThumbnail={true} deviceId={state.selectedCameraId} />
                      </div>
                      <div className="absolute inset-x-0 bottom-0 z-10 bg-gradient-to-t from-black/90 to-transparent p-2 pointer-events-none">
                        <p className="text-[9px] font-black truncate uppercase tracking-tight text-emerald-400 text-left leading-none">Live Webcam</p>
                        <div className="flex items-center gap-1 mt-1 opacity-40">
                          <Camera size={8} className="text-emerald-400" />
                          <span className="text-[7px] font-mono text-emerald-400 uppercase tracking-widest leading-none">input</span>
                        </div>
                      </div>
                    </div>

                    {filteredMedia.map(item => (
                       <div 
                        key={item.id}
                        onClick={() => {
                          setState(s => ({ ...s, previewAsset: item }));
                        }}
                         className={`group relative aspect-video rounded-xl overflow-hidden border transition-all cursor-pointer ${state.previewAsset?.id === item.id ? 'border-blue-500 shadow-[0_0_15px_rgba(59,130,246,0.5)] ring-1 ring-blue-500' : 'border-white/5 bg-black/20 hover:border-white/20'}`}
                       >
                         <div className="absolute inset-0 z-0 pointer-events-none">
                           {renderMediaThumbnail(item)}
                         </div>
                         <div className="absolute inset-x-0 bottom-0 z-10 bg-gradient-to-t from-black/90 to-transparent p-2 pointer-events-none">
                           <p className="text-[9px] font-black truncate uppercase tracking-tight text-white/90 text-left leading-none">{item.name}</p>
                           <div className="flex items-center gap-1 mt-1 opacity-40">
                             {item.type === 'video' ? <Video size={8} className="text-white" /> : item.type === 'image' ? <ImageIcon size={8} className="text-white" /> : <Music size={8} className="text-white" />}
                             <span className="text-[7px] font-mono text-white uppercase tracking-widest leading-none">{item.type}</span>
                           </div>
                         </div>
                         {state.previewAsset?.id === item.id && (
                           <div className="absolute inset-0 bg-blue-500/10 z-[1] pointer-events-none" />
                         )}
                         <div className="absolute inset-0 bg-white/0 group-hover:bg-white/5 transition-colors z-[2] pointer-events-none" />
                         
                         {/* Link Button */}
                         {selectedMeetingItemId && (
                           <button 
                             onClick={(e) => {
                               e.stopPropagation();
                               linkMediaToItem(item.id);
                             }}
                             className="absolute top-2 left-2 z-[30] px-2 py-1 rounded-lg bg-emerald-500/80 text-white font-black text-[9px] uppercase tracking-widest flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-all hover:bg-emerald-500 shadow-xl border border-emerald-400/20"
                             title="Link to Selected Segment"
                           >
                             <Plus size={10} /> Link
                           </button>
                         )}

                         {/* Remove Button */}
                         <button 
                           onClick={(e) => {
                             e.stopPropagation();
                             if (confirm(`Remove ${item.name} from media library?`)) removeAsset(item.id);
                           }}
                           className="absolute top-2 right-2 z-[30] w-6 h-6 rounded-lg bg-red-500/80 text-white flex items-center justify-center opacity-0 group-hover:opacity-100 transition-all hover:bg-red-500 shadow-xl border border-red-400/20"
                           title="Remove Asset"
                         >
                           <Trash2 size={12} />
                         </button>
                       </div>
                    ))}
                  </div>
                ) : (
                  <div className="h-full flex flex-col items-center justify-center opacity-10 gap-3">
                     <Database size={32} strokeWidth={0.5} />
                     <p className="text-[8px] uppercase tracking-widest">Library Empty</p>
                  </div>
                )}
             </div>
           </section>

            {/* Sequence - Middle Column */}
            <section className="col-span-1 border border-white/5 bg-neutral-900/40 rounded-xl flex flex-col overflow-hidden backdrop-blur-md min-h-0">
              <div className="px-4 py-3 border-b border-white/5 bg-white/[0.02] flex items-center justify-between group">
                <div className="flex items-center gap-2">
                 <Layers size={14} className="text-blue-500" />
                 <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40">Sequence</span>
                  <button 
                    onClick={clearOutlineMedia}
                    className="ml-2 px-2 py-0.5 rounded bg-red-500/10 hover:bg-red-500/20 text-red-500 text-[8px] font-black uppercase tracking-widest border border-red-500/20 transition-all opacity-40 hover:opacity-100"
                    title="Clear All Linked Media"
                  >
                    Clear All
                  </button>
               </div>
               <span className="text-[9px] font-mono text-white/20 uppercase tracking-widest">{activeSchedule.length} Segments</span>
             </div>
             
             <div className="flex-1 overflow-y-auto p-3 space-y-3 scrollbar-thin">
                {activeSchedule.map(item => (
                  <div 
                    key={item.id}
                    onClick={() => {
                      setSelectedMeetingItemId(item.id);
                      setShowAllMedia(false);
                      if (item.mediaIds.length > 0) {
                        const firstAsset = mediaLibrary.find(a => a.id === item.mediaIds[0]);
                        if (firstAsset) {
                          setState(s => ({ ...s, previewAsset: firstAsset }));
                        }
                      }
                    }}
                    className={`w-full text-left p-4 rounded-2xl transition-all relative overflow-hidden border cursor-pointer ${selectedMeetingItemId === item.id ? 'bg-blue-500/20 border-blue-500/40 shadow-2xl' : 'bg-black/20 border-white/5 hover:bg-white/5 text-white/40'}`}
                  >
                     <div className="flex justify-between items-start mb-3">
                        <span className={`text-[10px] font-mono font-bold ${selectedMeetingItemId === item.id ? 'text-blue-400' : 'text-blue-400/50'}`}>{item.time}</span>
                        <div className="flex items-center gap-1.5 opacity-20">
                           <Activity size={12} />
                        </div>
                     </div>
                     <p className={`text-[12px] font-black leading-tight truncate uppercase tracking-widest ${selectedMeetingItemId === item.id ? 'text-white' : 'text-white/60'}`}>{item.title}</p>
                     
                     <div className="mt-4 flex items-center gap-3 overflow-x-auto pb-1 scrollbar-none" onClick={e => e.stopPropagation()}>
                        {item.mediaIds.length > 0 ? (
                          <div className="flex gap-2 flex-wrap">
                            {item.mediaIds.map(mediaId => {
                              const asset = mediaLibrary.find(a => a.id === mediaId);
                              if (!asset) return null;
                              return (
                                <div 
                                  key={mediaId} 
                                  className="group relative w-20 h-14 rounded-xl overflow-hidden border border-white/10 bg-black/40 shrink-0 cursor-pointer"
                                  onClick={(e) => {
                                    e.stopPropagation();
                                    setState(s => ({ ...s, previewAsset: asset }));
                                  }}
                                >
                                   {renderMediaThumbnail(asset, 'w-full h-full object-cover pointer-events-none')}
                                   <div className="absolute inset-0 bg-blue-500/0 group-hover:bg-blue-500/20 transition-all" />
                                   <button
                                     onClick={(e) => {
                                       e.stopPropagation();
                                       if (confirm(`Remove ${asset.name} from this segment?`)) {
                                         setSchedules(prev => {
                                           const type = state.meetingType;
                                           const updatedSchedule = prev[type].map(it => {
                                             if (it.id === item.id) {
                                               return { ...it, mediaIds: it.mediaIds.filter(id => id !== mediaId) };
                                             }
                                             return it;
                                           });
                                           return { ...prev, [type]: updatedSchedule };
                                         });
                                       }
                                     }}
                                     className="absolute top-1 right-1 w-5 h-5 bg-red-500/90 text-white rounded flex items-center justify-center opacity-0 group-hover:opacity-100 transition-all hover:bg-red-500 hover:scale-110 shadow-lg"
                                     title="Unlink Media"
                                   >
                                     <Trash2 size={10} />
                                   </button>
                                </div>
                              );
                            })}
                            <div className="flex gap-1 items-center">
                               <button onClick={() => handleImportClick(item.id, 'video')} className="w-10 h-10 flex items-center justify-center bg-white/5 hover:bg-white/10 rounded-xl text-white/20 hover:text-blue-400 border border-white/5 transition-all"> <Plus size={14} /> </button>
                            </div>
                          </div>
                        ) : (
                          <>
                            {item.type === 'song' ? (
                               <div className="flex-1 flex items-center gap-2">
                                 <div className="relative flex items-center gap-2">
                                   <div className="flex flex-col">
                                     <input 
                                       type="number" 
                                       placeholder="Song #" 
                                       value={songInputs[item.id] !== undefined ? songInputs[item.id] : (item.title.startsWith('Song ') ? item.title.replace('Song ', '') : '')}
                                       className="bg-black/40 border border-white/10 rounded-xl px-3 py-1.5 w-24 text-xs font-mono font-black placeholder:text-white/5 text-center focus:border-blue-500/50 outline-none transition-all"
                                       onChange={e => setSongInputs(prev => ({ ...prev, [item.id]: e.target.value }))}
                                       onKeyDown={e => {
                                         if (e.key === 'Enter') {
                                           updateSong(item.id, songInputs[item.id] || e.currentTarget.value);
                                         }
                                       }}
                                     />
                                     {(songInputs[item.id] ? SJJ_TITLES[songInputs[item.id]] : (item.title.startsWith('Song ') && SJJ_TITLES[item.title.replace('Song ', '')])) && (
                                       <span className="text-[7px] text-blue-400 font-bold uppercase tracking-tighter mt-1 absolute -bottom-4 left-0 truncate w-32">
                                         {songInputs[item.id] ? SJJ_TITLES[songInputs[item.id]] : SJJ_TITLES[item.title.replace('Song ', '')]}
                                       </span>
                                     )}
                                   </div>
                                   <button 
                                     onClick={() => {
                                       const num = songInputs[item.id] || (item.title.startsWith('Song ') ? item.title.replace('Song ', '') : '');
                                       if (num) updateSong(item.id, num);
                                     }}
                                     className="p-1.5 bg-blue-500/20 hover:bg-blue-500 text-blue-500 hover:text-white rounded-lg transition-all border border-blue-500/20"
                                     title="Save Song"
                                   >
                                     <Check size={14} />
                                   </button>
                                 </div>
                                 <span className="text-[9px] font-mono opacity-20 uppercase tracking-widest italic leading-none ml-4">
                                   {item.mediaIds.length > 0 ? '✓ Linked' : 'Unlinked'}
                                 </span>
                               </div>
                            ) : (
                              <div className="flex items-center gap-2">
                                 <button onClick={() => handleImportClick(item.id, 'video')} className="flex items-center gap-2 px-3 py-1.5 bg-black/40 hover:bg-black/60 rounded-xl text-white/20 hover:text-blue-400 border border-white/5 transition-all text-[10px] font-black uppercase tracking-widest">
                                    <Video size={14} />
                                    <span>Add Video</span>
                                 </button>
                                 <button onClick={() => handleScanMWBImages(item.id)} className="flex items-center gap-2 px-3 py-1.5 bg-blue-500/10 hover:bg-blue-500/20 rounded-xl text-blue-500 hover:text-blue-400 border border-blue-500/20 transition-all text-[10px] font-black uppercase tracking-widest">
                                    <ImageIcon size={14} />
                                    <span>Add JW Image</span>
                                 </button>
                                 <button onClick={() => handleImportClick(item.id, 'image')} className="flex items-center gap-2 px-3 py-1.5 bg-black/40 hover:bg-black/60 rounded-xl text-white/20 hover:text-blue-400 border border-white/5 transition-all text-[10px] font-black uppercase tracking-widest" title="Pick from any folder">
                                    <Folder size={14} />
                                 </button>
                              </div>
                            )}
                          </>
                        )}
                     </div>

                     {selectedMeetingItemId === item.id && (
                        <motion.div layoutId="seq-active" className="absolute left-0 top-0 bottom-0 w-1 bg-blue-500 shadow-[2px_0_15px_rgba(59,130,246,0.5)]" />
                     )}
                  </div>
                ))}
             </div>
           </section>

           {/* Right Column: Timer, Mixer & Background Music */}
           <div className="col-span-1 flex flex-col gap-6 min-h-0 overflow-y-auto scrollbar-thin pr-2 pb-2">
              {/* Sound Mixer */}
              <section className="border border-white/5 bg-neutral-900/30 rounded-xl flex flex-col overflow-hidden backdrop-blur-sm group">
                <div className="px-4 py-3 border-b border-white/5 bg-white/[0.02] flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <Volume2 size={14} className="text-blue-500" />
                    <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40">Sound Mixer</span>
                  </div>
                </div>
                <div className="p-4 space-y-4">
                  <div className="space-y-2">
                    <div className="flex justify-between items-center text-[9px] font-black uppercase tracking-widest">
                      <span className="text-blue-400">Program Volume</span>
                      <span className="text-white/40 font-mono">{state.mixer.masterVolume}%</span>
                    </div>
                    <div className="flex items-center gap-3">
                      <button 
                        onClick={toggleUnifiedMute}
                        className={`p-2 rounded-lg transition-colors ${state.mixer.isMuted ? 'bg-red-500/20 text-red-500' : 'bg-white/5 text-white/40 hover:text-white'}`}
                      >
                        {state.mixer.isMuted ? <VolumeX size={16} /> : <Volume2 size={16} />}
                      </button>
                      <div className="flex-1 h-8 flex items-center">
                       <input 
                         type="range"
                         className="w-full accent-blue-500 appearance-none bg-white/5 h-1 rounded-full cursor-pointer"
                         min="0" max="100"
                         value={state.mixer.masterVolume}
                         onChange={(e) => setUnifiedVolume(parseInt(e.target.value))}
                       />
                      </div>
                    </div>
                  </div>

                  <div className="rounded-lg border border-white/5 bg-black/30 p-3">
                    <div className="flex items-center justify-between gap-3">
                      <span className="text-[8px] font-black uppercase tracking-[0.2em] text-white/30">Audio Output</span>
                      <span className="min-w-0 truncate text-right text-[9px] font-mono text-white/60" title={audioOutputName}>
                        {audioOutputName}
                      </span>
                    </div>
                  </div>
                </div>
              </section>

              {/* Timer */}
              <section className="border border-white/5 bg-neutral-900/30 rounded-xl p-6 backdrop-blur-sm">
                <div className="flex justify-between items-center mb-6">
                  <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40">Timer</span>
                  <div className="flex items-center gap-4">
                    <div className="flex flex-col gap-1">
                       <button 
                         onClick={() => setState(s => ({ ...s, timer: { ...s.timer, seconds: s.timer.seconds + 60 } }))}
                         className="p-1 hover:bg-white/5 rounded text-white/20 hover:text-blue-400 transition-colors"
                       >
                         <ChevronRight size={14} className="-rotate-90" />
                       </button>
                       <button 
                         onClick={() => setState(s => ({ ...s, timer: { ...s.timer, seconds: Math.max(0, s.timer.seconds - 60) } }))}
                         className="p-1 hover:bg-white/5 rounded text-white/20 hover:text-blue-400 transition-colors"
                       >
                         <ChevronRight size={14} className="rotate-90" />
                       </button>
                    </div>
                    <div className="text-3xl font-mono font-black text-white tracking-tighter">{formatTime(state.timer.seconds)}</div>
                  </div>
                </div>
                <div className="flex gap-2">
                  <button 
                    onClick={() => setState(s => ({ ...s, timer: { ...s.timer, isRunning: !s.timer.isRunning }, previewTimer: false }))}
                    className={`flex-1 h-12 rounded-xl font-black text-[10px] uppercase tracking-widest shadow-lg transition-all ${state.timer.isRunning ? 'bg-red-500 text-white shadow-red-500/20' : 'bg-emerald-500 text-white shadow-emerald-500/20'}`}
                  >
                    {state.timer.isRunning ? 'Stop Timer' : 'Start Timer'}
                  </button>
                  <button 
                    onClick={() => setState(s => ({ ...s, previewTimer: !s.previewTimer && !s.timer.isRunning }))}
                    className={`px-4 h-12 rounded-xl font-black text-[10px] uppercase tracking-widest transition-all ${state.previewTimer ? 'bg-blue-500 text-white shadow-lg shadow-blue-500/20' : 'bg-white/5 text-white/40 border border-white/5 hover:bg-white/10'}`}
                  >
                    Stage
                  </button>
                  <button 
                    onClick={() => setState(s => ({ ...s, timer: { ...s.timer, seconds: 0, isRunning: false }, previewTimer: false }))}
                    className="px-4 h-12 rounded-xl bg-white/5 text-white/40 hover:text-white border border-white/5 uppercase text-[10px] font-black transition-all hover:bg-white/10"
                  >
                    Reset
                  </button>
                </div>
              </section>

              {/* Background Music */}
              <section className="min-h-[250px] border border-white/5 bg-neutral-900/30 rounded-xl flex flex-col overflow-hidden backdrop-blur-sm group">
                <div className="px-4 py-3 border-b border-white/5 bg-white/[0.02] flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <Music size={14} className="text-emerald-500" />
                      <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white/40">Background Music</span>
                    </div>
                      <div className="flex items-center gap-2">
                        <div className={`w-1.5 h-1.5 rounded-full ${isPlayingBg ? 'bg-emerald-500 animate-pulse' : 'bg-white/10'}`} />
                        <button 
                          onClick={clearMediaLibrary}
                          className="p-1 hover:bg-red-500/10 rounded text-white/20 hover:text-red-500 transition-all"
                          title="Clear Library"
                        >
                          <Trash2 size={14} />
                        </button>
                        <button 
                          onClick={handleScanBgMusic}
                          disabled={isScanningBg}
                          className="p-1 hover:bg-white/5 rounded text-white/20 hover:text-white transition-all disabled:opacity-50"
                          title="Add Music Folder"
                        >
                          <Folder size={14} />
                        </button>
                      </div>
                </div>
                
                <div className="flex-1 overflow-y-auto p-2 scrollbar-thin">
                    {filteredBgPlaylist.length === 0 ? (
                      <div className="h-full flex flex-col items-center justify-center gap-3 opacity-10 py-8">
                        <Music size={32} strokeWidth={0.5} />
                        <p className="text-[9px] uppercase tracking-[0.3em]">No Tracks Detected</p>
                        <button 
                          onClick={handleScanBgMusic}
                          className="mt-4 px-4 py-2 border border-white/10 rounded-full text-[8px] font-black uppercase tracking-widest hover:bg-white/5"
                        >
                          Select Music Folder
                        </button>
                      </div>
                    ) : (
                      <div className="space-y-1">
                        {filteredBgPlaylist.map((track, i) => (
                          <button 
                            key={i}
                            onClick={() => {
                              setCurrentBgIndex(i);
                              setIsPlayingBg(true);
                            }}
                            className={`w-full flex items-center gap-3 p-2 rounded transition-all group ${currentBgIndex === i && isPlayingBg ? 'bg-emerald-500/10' : 'hover:bg-white/5'}`}
                          >
                            <div className="w-10 h-10 rounded-lg overflow-hidden flex-shrink-0 border border-white/5 bg-white/5 relative group/thumb">
                               <SmartMedia 
                                 asset={track} 
                                 isThumbnail={true} 
                                 className="w-full h-full" 
                               />
                               <div className={`absolute inset-0 flex items-center justify-center transition-all ${currentBgIndex === i && isPlayingBg ? 'bg-emerald-500/40 opacity-100' : 'bg-black/40 opacity-0 group-hover/thumb:opacity-100'}`}>
                                 {currentBgIndex === i && isPlayingBg ? <Activity size={12} className="text-white animate-pulse" /> : <Play size={10} fill="currentColor" className="text-white" />}
                               </div>
                            </div>
                            <p className={`text-[10px] font-black truncate uppercase tracking-tight flex-1 text-left ${currentBgIndex === i && isPlayingBg ? 'text-emerald-500' : 'text-white/60 group-hover:text-white'}`}>
                              {track.name.replace(/\.mp3$/i, '')}
                            </p>
                          </button>
                        ))}
                      </div>
                    )}
                </div>

                {filteredBgPlaylist.length > 0 && (
                    <div className="p-3 border-t border-white/5 bg-black/40 flex items-center justify-between gap-3">
                      <div className="flex items-center gap-2">
                          <button onClick={() => seekBg(0)} className="p-1 hover:bg-white/5 rounded text-white/40 hover:text-white" title="Rewind to Start"> <RotateCcw size={12} /> </button>
                          <button onClick={() => jumpBg(-10)} className="p-1 hover:bg-white/5 rounded text-white/40 hover:text-white" title="Back 10s"> <Rewind size={12} /> </button>
                          <button onClick={prevBg} className="p-1 hover:bg-white/5 rounded text-white/40 hover:text-white"> <SkipBack size={12} /> </button>
                          <button onClick={toggleBgMusic} className="w-8 h-8 bg-emerald-500 hover:bg-emerald-400 text-white rounded-full flex items-center justify-center shadow-lg shadow-emerald-500/20">
                            {isPlayingBg ? <Pause size={12} fill="currentColor" /> : <Play size={12} fill="currentColor" className="ml-0.5" />}
                          </button>
                          <button onClick={nextBg} className="p-1 hover:bg-white/5 rounded text-white/40 hover:text-white"> <SkipForward size={12} /> </button>
                          <button onClick={() => jumpBg(10)} className="p-1 hover:bg-white/5 rounded text-white/40 hover:text-white" title="Forward 10s"> <FastForward size={12} /> </button>
                      </div>
                      <div className="flex-1 min-w-0">
                          <p className="text-[9px] font-black text-white/40 uppercase truncate mb-1">{filteredBgPlaylist[currentBgIndex]?.name}</p>
                          <div className="h-1 bg-white/5 rounded-full overflow-hidden">
                            <motion.div 
                              className="h-full bg-emerald-500" 
                              initial={{ width: '0%' }}
                              animate={{ width: isPlayingBg ? '100%' : '0%' }} 
                              transition={{ duration: 180, ease: "linear" }}
                            />
                          </div>
                      </div>
                      {/* Local monitor handles playback via AppState sync above */}
                    </div>
                )}
              </section>
           </div>
        </div>
      </main>

      <footer className="h-8 bg-zinc-950 border-t border-white/5 flex items-center justify-between px-6 text-[10px] font-mono font-medium text-white/20 uppercase tracking-[0.2em]">
        <div className="flex gap-8">
           <div className="flex items-center gap-2 text-blue-400">
             <div className="w-1.5 h-1.5 rounded-full bg-blue-400 animate-pulse" />
             LOGGED IN AS OPERATOR
           </div>
        </div>
        <div>MEDIAFLOW SUITE v1.0.0</div>
      </footer>



      {/* Import Selection Modal */}
      <AnimatePresence>
        {importMenu && (
          <motion.div 
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="absolute inset-0 z-[110] bg-black/90 backdrop-blur-md flex items-center justify-center p-6"
          >
            <motion.div 
              initial={{ scale: 0.9, y: 20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0.9, y: 20 }}
              className="bg-neutral-900 border border-white/10 p-4 rounded-3xl max-w-xl w-full max-h-[80vh] flex flex-col shadow-2xl"
            >
              <div className="flex justify-between items-center mb-6 px-4">
                <h3 className="text-lg font-black text-white uppercase tracking-tighter">
                  Select {importMenu.type} to Add
                </h3>
                <div className="flex items-center gap-2">
                  {(window as any).mediaflow && (
                    <button 
                      onClick={async () => {
                        setScanResults(null);
                        const results = await (window as any).mediaflow.scanJwMedia(state.language);
                        const filtered = results.filter((r: any) => r.type === importMenu.type);
                        setScanResults(filtered);
                      }}
                      className="px-3 py-1.5 bg-emerald-500/10 hover:bg-emerald-500 text-emerald-500 hover:text-white border border-emerald-500/20 text-[9px] font-black uppercase tracking-widest rounded-lg transition-all flex items-center gap-2"
                    >
                      <RotateCw size={10} />
                      Auto-Scan Device
                    </button>
                  )}
                  <button 
                    onClick={() => { setImportMenu(null); setScanResults(null); }}
                    className="p-2 hover:bg-white/5 rounded-full text-white/40 hover:text-white transition-colors"
                  >
                    <Square size={20} className="rotate-45" />
                  </button>
                </div>
              </div>

              <div className="flex-1 overflow-y-auto px-2 space-y-2 mb-6">
                {!scanResults ? (
                  <div className="flex flex-col items-center justify-center h-48 gap-4">
                    <div className="w-10 h-10 border-2 border-blue-500/20 border-t-blue-500 rounded-full animate-spin" />
                    <p className="text-[10px] font-black uppercase text-white/20 tracking-widest">Scanning Directory...</p>
                  </div>
                ) : scanResults.length === 0 ? (
                  <div className="text-center py-12 opacity-30 uppercase text-xs font-bold tracking-widest">
                    No {importMenu.type}s found in directory
                  </div>
                ) : scanResults.map((file, i) => (
                  <button 
                    key={i}
                    onClick={() => linkFileToItem(file)}
                    className="w-full text-left p-4 rounded-2xl bg-white/5 hover:bg-blue-500/10 border border-transparent hover:border-blue-500/30 transition-all flex items-center justify-between group"
                  >
                    <div className="flex items-center gap-4 min-w-0">
                      <div className="w-24 h-16 bg-white/5 rounded-xl overflow-hidden flex items-center justify-center text-white/20 group-hover:text-blue-500 transition-colors relative shrink-0">
                        {renderMediaThumbnail({
                            id: `scan-${i}`,
                            name: file.name,
                            path: file.path,
                            type: file.type,
                            fileHandle: file.fileHandle
                          } as MediaAsset)}
                      </div>
                      <div className="min-w-0">
                        <p className="text-xs font-black text-white/80 truncate mb-1">{file.name}</p>
                        <div className="flex items-center gap-1.5 overflow-hidden">
                          <Folder size={8} className="text-white/20 shrink-0" />
                          <p className="text-[9px] font-mono text-white/20 hover:text-white/40 transition-colors truncate">
                            {file.path.split(/[\\\/]/).slice(-2, -1)[0] || 'Root'} / {file.name}
                          </p>
                        </div>
                      </div>
                    </div>
                    <ChevronRight size={18} className="text-white/10 group-hover:text-blue-500 group-hover:translate-x-1 transition-all" />
                  </button>
                ))}
              </div>

              <div className="p-4 border-t border-white/5 text-center">
                 <p className="text-[9px] font-bold text-white/20 uppercase tracking-[0.3em]">
                   Suggested: {importMenu.type === 'video' ? "Videos/JWLibrary" : "LocalState/Publications"}
                 </p>
                 <p className="text-[8px] font-mono text-white/10 mt-1 truncate max-w-xs mx-auto">
                   {importMenu.type === 'video' 
                     ? "C:\\Users\\SETH\\Videos\\JWLibrary" 
                     : "C:\\Users\\SETH\\AppData\\Local\\Packages\\Watchtower...\\Publications"}
                 </p>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      <AnimatePresence>
        {showSettings && (
          <motion.div 
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 z-[100] flex items-center justify-center p-6 bg-black/80 backdrop-blur-sm"
          >
            <motion.div 
              initial={{ scale: 0.9, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0.9, opacity: 0 }}
              className="w-full max-w-lg max-h-[85vh] bg-neutral-900 rounded-2xl border border-white/10 shadow-2xl flex flex-col overflow-hidden"
            >
              <div className="px-6 py-4 border-b border-white/5 flex items-center justify-between bg-white/[0.02]">
                <div className="flex items-center gap-3">
                  <div className="w-8 h-8 rounded-lg bg-blue-500/20 flex items-center justify-center text-blue-500">
                    <Settings size={16} />
                  </div>
                  <div>
                    <h2 className="text-sm font-black text-white uppercase tracking-widest">Application Settings</h2>
                    <p className="text-[10px] text-white/40 font-bold uppercase tracking-wider">Configure MediaFlow Preferences</p>
                  </div>
                </div>
                <button 
                  onClick={() => setShowSettings(false)}
                  className="p-2 hover:bg-white/5 rounded-lg text-white/20 hover:text-white transition-all"
                >
                  <X size={20} />
                </button>
              </div>

              <div className="p-6 flex-1 overflow-y-auto min-h-[400px]">
                <section className="mb-8">
                  <h3 className="text-[10px] font-black text-white/40 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Monitor size={12} className="text-blue-500" />
                    Display Configuration
                  </h3>
                  <div className="space-y-4 bg-white/[0.02] border border-white/5 rounded-xl p-4">
                    <div className="flex items-center justify-between">
                       <div className="flex flex-col gap-1">
                         <span className="text-[10px] font-black text-white uppercase tracking-widest">Secondary Display</span>
                         <span className="text-[9px] text-white/20 uppercase font-bold tracking-wider">What shows on the audience screen</span>
                       </div>
                       <select 
                         value={state.displaySettings.secondaryDisplay}
                         onChange={(e) => setState(s => ({ ...s, displaySettings: { ...s.displaySettings, secondaryDisplay: e.target.value as any } }))}
                         className="bg-zinc-950 border border-white/10 rounded h-8 px-2 text-[10px] text-white focus:outline-none"
                       >
                         <option value="audience">Live Feed Only</option>
                         <option value="timer">Full Timer Only</option>
                         <option value="multiview">Split: Feed + Timer</option>
                       </select>
                    </div>
                    <div className="h-px bg-white/5" />
                    <div className="flex items-center justify-between">
                       <div className="flex flex-col gap-1">
                         <span className="text-[10px] font-black text-white uppercase tracking-widest">Clock Overlay</span>
                         <span className="text-[9px] text-white/20 uppercase font-bold tracking-wider">Show local time when feed is idle</span>
                       </div>
                       <button 
                         onClick={() => setState(s => ({ ...s, displaySettings: { ...s.displaySettings, showTimerOnAudience: !s.displaySettings.showTimerOnAudience } }))}
                         className={`w-10 h-5 rounded-full transition-all relative ${state.displaySettings.showTimerOnAudience ? 'bg-blue-500' : 'bg-white/10'}`}
                       >
                         <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${state.displaySettings.showTimerOnAudience ? 'left-6' : 'left-1'}`} />
                       </button>
                    </div>
                  </div>
                </section>

                <section className="mb-8 opacity-100 transition-opacity">
                  <h3 className="text-[10px] font-black text-blue-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Video size={12} />
                    Custom Video Paths
                  </h3>
                  <div className="space-y-3">
                    {!state.isPermissionGranted ? (
                      <div className="bg-blue-500/5 border border-blue-500/10 rounded-xl p-4 text-center">
                        <p className="text-[9px] font-black text-blue-400/60 uppercase tracking-widest">Permissions Required to Add Paths</p>
                      </div>
                    ) : (
                      <div className="flex gap-2">
                        <input 
                          id="new-video-dir"
                          type="text" 
                          placeholder="C:\Videos..." 
                          className="flex-1 bg-white/[0.03] border border-white/5 rounded-lg px-4 h-10 text-xs text-white placeholder:text-white/10 focus:outline-none focus:border-blue-500/50 transition-all"
                        />
                        <button 
                          onClick={() => {
                            const input = document.getElementById('new-video-dir') as HTMLInputElement;
                            if (input.value) {
                              setState(s => ({ ...s, videoPaths: [...s.videoPaths, input.value] }));
                              input.value = '';
                            }
                          }}
                          className="w-10 h-10 bg-blue-500 hover:bg-blue-400 text-white rounded-lg flex items-center justify-center transition-all"
                        >
                          <Plus size={18} />
                        </button>
                      </div>
                    )}

                    {state.videoPaths.map((path, idx) => (
                      <div key={idx} className="flex items-center justify-between p-2 bg-white/[0.02] border border-white/5 rounded-lg">
                        <span className="text-[10px] font-mono text-white/40 truncate flex-1 pr-2">{path}</span>
                        <button onClick={() => setState(s => ({ ...s, videoPaths: s.videoPaths.filter(p => p !== path) }))} className="text-white/10 hover:text-red-500"><Trash2 size={12} /></button>
                      </div>
                    ))}
                  </div>
                </section>

                <section className="mb-8 overflow-hidden">
                  <h3 className="text-[10px] font-black text-emerald-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <ImageIcon size={12} />
                    Custom Image Paths
                  </h3>
                  <div className="space-y-3">
                    {!state.isPermissionGranted ? (
                      <div className="bg-emerald-500/5 border border-emerald-500/10 rounded-xl p-4 text-center">
                        <p className="text-[9px] font-black text-emerald-400/60 uppercase tracking-widest">Permissions Required to Add Paths</p>
                      </div>
                    ) : (
                      <div className="flex gap-2">
                        <input 
                          id="new-image-dir"
                          type="text" 
                          placeholder="C:\Images..." 
                          className="flex-1 bg-white/[0.03] border border-white/5 rounded-lg px-4 h-10 text-xs text-white placeholder:text-white/10 focus:outline-none focus:border-emerald-500/50 transition-all"
                        />
                        <button 
                          onClick={() => {
                            const input = document.getElementById('new-image-dir') as HTMLInputElement;
                            if (input.value) {
                              setState(s => ({ ...s, imagePaths: [...s.imagePaths, input.value] }));
                              input.value = '';
                            }
                          }}
                          className="w-10 h-10 bg-emerald-500 hover:bg-emerald-400 text-white rounded-lg flex items-center justify-center transition-all"
                        >
                          <Plus size={18} />
                        </button>
                      </div>
                    )}
                    {state.imagePaths.map((path, idx) => (
                      <div key={idx} className="flex items-center justify-between p-2 bg-white/[0.02] border border-white/5 rounded-lg">
                        <span className="text-[10px] font-mono text-white/40 truncate flex-1 pr-2">{path}</span>
                        <button onClick={() => setState(s => ({ ...s, imagePaths: s.imagePaths.filter(p => p !== path) }))} className="text-white/10 hover:text-red-500"><Trash2 size={12} /></button>
                      </div>
                    ))}
                  </div>
                </section>

                <section className="mb-8">
                  <h3 className="text-[10px] font-black text-purple-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Music size={12} />
                    Custom Song/Audio Paths
                  </h3>
                  <div className="space-y-3">
                    <div className="flex gap-2">
                      <input 
                        id="new-audio-dir"
                        type="text" 
                        placeholder="C:\Music..." 
                        className="flex-1 bg-white/[0.03] border border-white/5 rounded-lg px-4 h-10 text-xs text-white placeholder:text-white/10 focus:outline-none focus:border-purple-500/50 transition-all"
                      />
                      <button 
                        onClick={() => {
                          const input = document.getElementById('new-audio-dir') as HTMLInputElement;
                          if (input.value) {
                            setState(s => ({ ...s, audioPaths: [...s.audioPaths, input.value] }));
                            input.value = '';
                          }
                        }}
                        className="w-10 h-10 bg-purple-500 hover:bg-purple-400 text-white rounded-lg flex items-center justify-center transition-all"
                      >
                        <Plus size={18} />
                      </button>
                    </div>
                    {state.audioPaths.map((path, idx) => (
                      <div key={idx} className="flex items-center justify-between p-2 bg-white/[0.02] border border-white/5 rounded-lg">
                        <span className="text-[10px] font-mono text-white/40 truncate flex-1 pr-2">{path}</span>
                        <button onClick={() => setState(s => ({ ...s, audioPaths: s.audioPaths.filter(p => p !== path) }))} className="text-white/10 hover:text-red-500"><Trash2 size={12} /></button>
                      </div>
                    ))}
                  </div>
                </section>

                <section className="mb-8">
                  <h3 className="text-[10px] font-black text-blue-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Monitor size={12} />
                    Display & Outputs
                  </h3>
                  <div className="space-y-4">
                    <div className="p-4 rounded-xl bg-white/[0.02] border border-white/5 space-y-4">
                      <div>
                        <label className="block text-[9px] font-black text-white/40 uppercase tracking-widest mb-2">Secondary Display (Extended)</label>
                        <select 
                          value={state.displaySettings.secondaryDisplay}
                          onChange={(e) => setState(s => ({ ...s, displaySettings: { ...s.displaySettings, secondaryDisplay: e.target.value as any } }))}
                          className="w-full bg-white/[0.03] border border-white/10 rounded-lg px-3 h-10 text-[11px] text-white focus:outline-none transition-all"
                        >
                          <option value="audience" className="bg-neutral-900">Audience Main Feed</option>
                          <option value="timer" className="bg-neutral-900">Timer Count Down</option>
                          <option value="multiview" className="bg-neutral-900">Multi-View (All)</option>
                        </select>
                      </div>

                      <div className="flex items-center justify-between">
                        <div className="min-w-0">
                          <p className="text-[11px] font-bold text-white/80">Show Timer Overlay</p>
                          <p className="text-[9px] text-white/20">Display clock on audience feed during songs</p>
                        </div>
                        <button 
                          onClick={() => setState(s => ({ 
                            ...s, 
                            displaySettings: { ...s.displaySettings, showTimerOnAudience: !s.displaySettings.showTimerOnAudience } 
                          }))}
                          className={`w-10 h-5 rounded-full transition-all relative ${state.displaySettings.showTimerOnAudience ? 'bg-blue-500' : 'bg-white/10'}`}
                        >
                          <div className={`absolute top-1 w-3 h-3 rounded-full bg-white transition-all ${state.displaySettings.showTimerOnAudience ? 'right-1' : 'left-1'}`} />
                        </button>
                      </div>
                    </div>
                  </div>
                </section>

                <section className="mb-8">
                  <h3 className="text-[10px] font-black text-blue-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Globe size={12} />
                    Language Identification
                  </h3>
                  <div className="space-y-4 bg-white/[0.02] border border-white/5 rounded-xl p-4">
                    <div className="space-y-3">
                      <div className="flex items-start gap-3">
                        <div className="w-6 h-6 rounded bg-blue-500/20 flex items-center justify-center text-[10px] font-black text-blue-400 shrink-0">E</div>
                        <div className="min-w-0">
                          <p className="text-[10px] font-black text-white uppercase tracking-wider">English Identifier</p>
                          <p className="text-[9px] text-white/40 uppercase font-medium leading-relaxed">Files containing <code className="text-blue-400 bg-white/5 px-1 rounded">_E_</code> or <code className="text-blue-400 bg-white/5 px-1 rounded">_EN_</code></p>
                        </div>
                      </div>
                      <div className="flex items-start gap-3">
                        <div className="w-6 h-6 rounded bg-emerald-500/20 flex items-center justify-center text-[10px] font-black text-emerald-400 shrink-0">EW</div>
                        <div className="min-w-0">
                          <p className="text-[10px] font-black text-white uppercase tracking-wider">Ewe Identifier</p>
                          <p className="text-[9px] text-white/40 uppercase font-medium leading-relaxed">Files containing <code className="text-emerald-400 bg-white/5 px-1 rounded">_EW_</code> or <code className="text-emerald-400 bg-white/5 px-1 rounded">_EWE_</code></p>
                        </div>
                      </div>
                    </div>
                    <div className="mt-4 p-3 bg-white/5 rounded-lg border border-white/5">
                      <p className="text-[8px] font-mono text-white/20 uppercase leading-relaxed tracking-wider">
                        Files without these identifiers (like background images) will always be visible regardless of the selected language.
                      </p>
                    </div>
                  </div>
                </section>
                <section className="mb-8">
                  <h3 className="text-[10px] font-black text-amber-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Camera size={12} />
                    Camera Configuration
                  </h3>
                  <div className="space-y-3">
                    <p className="text-[10px] text-white/40 uppercase tracking-widest mb-1">Active Camera Input</p>
                    <div className="relative">
                      <select 
                        value={state.selectedCameraId || ''}
                        onChange={(e) => setState(s => ({ ...s, selectedCameraId: e.target.value }))}
                        className="w-full bg-white/[0.03] border border-white/5 rounded-lg px-4 h-10 text-xs text-white focus:outline-none focus:border-amber-500/50 transition-all appearance-none"
                      >
                        {availableCameras.map(cam => (
                          <option key={cam.deviceId} value={cam.deviceId} className="bg-neutral-900 text-white">
                            {cam.label || `Camera ${cam.deviceId.slice(0, 5)}`}
                          </option>
                        ))}
                        {availableCameras.length === 0 && <option value="">No Cameras Detected</option>}
                      </select>
                      <ChevronDown size={14} className="absolute right-3 top-3 text-white/20 pointer-events-none" />
                    </div>
                    <div className="mt-6 p-4 rounded-xl bg-blue-500/10 border border-blue-500/20">
                        <p className="text-[9px] text-white/60 leading-relaxed uppercase tracking-tight">
                          To use OBS Virtual Camera, ensure OBS Studio is installed and the Virtual Camera is started in OBS settings.
                        </p>
                    </div>
                  </div>
                </section>

                <section>
                  <h3 className="text-[10px] font-black text-blue-400 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Database size={12} />
                    System Information
                  </h3>
                  <div className="grid grid-cols-2 gap-3">
                    <div className="p-3 rounded-xl bg-white/[0.02] border border-white/5">
                      <p className="text-[9px] font-black text-white/20 uppercase tracking-widest mb-1">App Version</p>
                      <p className="text-xs font-mono text-white/60">v4.2.0-stable</p>
                    </div>
                    <div className="p-3 rounded-xl bg-white/[0.02] border border-white/5">
                      <p className="text-[9px] font-black text-white/20 uppercase tracking-widest mb-1">Local Host</p>
                      <p className="text-xs font-mono text-white/60">MediaFlow-Main</p>
                    </div>
                  </div>
                </section>
                
                <section className="mb-4">
                  <h3 className="text-[10px] font-black text-white/40 uppercase tracking-[0.2em] mb-4 flex items-center gap-2">
                    <Activity size={12} className="text-purple-500" />
                    Software Updates
                  </h3>
                  <div className="bg-white/[0.02] border border-white/5 rounded-xl p-6">
                    <div className="flex flex-col gap-4">
                      <div className="flex items-center justify-between">
                        <div className="flex flex-col gap-1">
                          <span className="text-[10px] font-black text-white uppercase tracking-widest">Version {state.isDesktop ? '1.0.17' : 'Web Preview'}</span>
                          <span className="text-[9px] text-white/20 uppercase font-bold tracking-wider">
                            {updateStatus?.status === 'checking' ? 'Checking for updates...' :
                             updateStatus?.status === 'available' ? 'Update Available!' :
                             updateStatus?.status === 'downloading' ? `Downloading: ${Math.round(updateStatus.progress || 0)}%` :
                             updateStatus?.status === 'downloaded' ? 'Update Ready to Install' :
                             updateStatus?.status === 'error' ? `Error: ${updateStatus.message}` :
                             'System is Up to Date'}
                          </span>
                        </div>
                        
                        {(!updateStatus || updateStatus.status === 'not-available' || updateStatus.status === 'error') && (
                          <button 
                            onClick={() => (window as any).mediaflow?.checkForUpdate()}
                            className="px-4 py-2 bg-white/5 hover:bg-white/10 text-white/60 text-[9px] font-black rounded-lg border border-white/10 uppercase tracking-widest transition-all"
                          >
                            Check Now
                          </button>
                        )}

                        {updateStatus?.status === 'available' && (
                          <button 
                            onClick={() => (window as any).mediaflow?.downloadUpdate()}
                            className="px-4 py-2 bg-blue-500 hover:bg-blue-400 text-white text-[9px] font-black rounded-lg uppercase tracking-widest transition-all shadow-lg shadow-blue-500/20"
                          >
                            Download {updateStatus.info?.version}
                          </button>
                        )}

                        {updateStatus?.status === 'downloaded' && (
                          <button 
                            onClick={() => (window as any).mediaflow?.installUpdate()}
                            className="px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-white text-[9px] font-black rounded-lg uppercase tracking-widest transition-all shadow-lg shadow-emerald-500/20"
                          >
                            Restart & Install
                          </button>
                        )}
                      </div>

                      {updateStatus?.status === 'downloading' && (
                        <div className="w-full h-1.5 bg-white/5 rounded-full overflow-hidden">
                          <motion.div 
                            className="h-full bg-blue-500" 
                            initial={{ width: 0 }}
                            animate={{ width: `${updateStatus.progress}%` }}
                          />
                        </div>
                      )}
                    </div>
                  </div>
                </section>
              </div>

              <div className="p-6 bg-white/[0.02] border-t border-white/5 flex justify-end">
                <button 
                  onClick={() => setShowSettings(false)}
                  className="px-6 h-10 bg-white/5 hover:bg-white/10 text-white/60 hover:text-white text-[10px] font-black rounded-lg uppercase tracking-[0.2em] border border-white/10 transition-all"
                >
                  Close Settings
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
