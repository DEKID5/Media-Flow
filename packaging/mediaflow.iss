; MediaFlow installer.
;
; Bundles the app (via windeployqt, staged into dist\ by build_installer.ps1)
; together with the OBS Virtual Camera DirectShow driver files
; (..\drivers\obs-virtualcam) and registers them with regsvr32 during
; install -- this is the one and only thing OBS Studio's own installer does
; to make "OBS Virtual Camera" appear as a camera device system-wide, so
; users never need to install OBS Studio itself. See
; drivers\obs-virtualcam\THIRD_PARTY_NOTICES.txt for licensing.

#define MyAppName "MediaFlow"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "MediaFlow"
#define MyAppExeName "MediaFlow.exe"

[Setup]
AppId={{8F2B4C1A-2E7D-4B6A-9C3F-6D1A8E5F4B21}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=installer_output
OutputBaseFilename=MediaFlow-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
; Registering the virtual camera driver (regsvr32) requires admin rights.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupIconFile=..\qml\assets\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Staged by build_installer.ps1 via windeployqt -- everything MediaFlow.exe
; needs at runtime (Qt DLLs, plugins, translations, QML modules).
Source: "dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

; OBS Virtual Camera driver -- both bitnesses, matching what OBS Studio's own
; installer ships, since some DirectShow-consuming apps run 32-bit.
Source: "..\drivers\obs-virtualcam\obs-virtualcam-module32.dll"; DestDir: "{app}\obs-virtualcam"; Flags: ignoreversion
Source: "..\drivers\obs-virtualcam\obs-virtualcam-module64.dll"; DestDir: "{app}\obs-virtualcam"; Flags: ignoreversion
Source: "..\drivers\obs-virtualcam\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}\obs-virtualcam"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
; Same "/i /s" regsvr32 invocation as OBS Studio's own virtualcam-install.bat
; (DllInstall + DllRegisterServer, silent). Registering an already-registered
; DLL is harmless, so this always runs rather than pre-checking the registry.
Filename: "{sys}\regsvr32.exe"; Parameters: "/i /s ""{app}\obs-virtualcam\obs-virtualcam-module32.dll"""; StatusMsg: "Registering virtual camera driver (32-bit)..."; Flags: runhidden
Filename: "{sys}\regsvr32.exe"; Parameters: "/i /s ""{app}\obs-virtualcam\obs-virtualcam-module64.dll"""; StatusMsg: "Registering virtual camera driver (64-bit)..."; Flags: runhidden
Filename: "{app}\{#MyAppExeName}"; Description: "Launch MediaFlow"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Mirrors OBS Studio's own virtualcam-uninstall.bat.
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\obs-virtualcam\obs-virtualcam-module32.dll"""; Flags: runhidden
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\obs-virtualcam\obs-virtualcam-module64.dll"""; Flags: runhidden
