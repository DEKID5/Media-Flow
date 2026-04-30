!macro customInstall
  DetailPrint "Registering MediaFlow Virtual Camera Drivers..."
  
  # Register UnityCapture filter if present
  IfFileExists "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll" 0 +3
    ExecWait 'regsvr32.exe /s "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll"'
    DetailPrint "UnityCapture Filter Registered."

  # Check for OBS Virtual Camera
  ReadRegStr $0 HKLM "SOFTWARE\OBS-Studio" ""
  IfErrors 0 +2
    DetailPrint "Note: OBS Studio not detected. OBS Virtual Camera may not work."

!macroend

!macro customUnInstall
  # Unregister UnityCapture filter
  IfFileExists "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll" 0 +2
    ExecWait 'regsvr32.exe /u /s "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll"'
!macroend
