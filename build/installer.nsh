!macro customInstall
  DetailPrint "Checking OBS Virtual Camera availability..."

  # Check for OBS Virtual Camera
  ReadRegStr $0 HKLM "SOFTWARE\OBS-Studio" ""
  IfErrors 0 +2
    DetailPrint "Note: OBS Studio not detected. OBS Virtual Camera may not work."

!macroend

!macro customUnInstall
!macroend
