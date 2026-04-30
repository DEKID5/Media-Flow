!macro customHeader
  !define MUI_ABORTWARNING
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_RIGHT

  !ifdef BUILD_UNINSTALLER
    !define MUI_WELCOMEPAGE_TITLE "Remove Media Flow Broadcast"
    !define MUI_WELCOMEPAGE_TEXT "This wizard will remove Media Flow Broadcast from this computer.$\r$\n$\r$\nYour Windows shortcuts and installed app files will be removed. Broadcast media and JW Library data outside the app folder are left untouched."
    !define MUI_FINISHPAGE_TITLE "Media Flow Broadcast was removed"
    !define MUI_FINISHPAGE_TEXT "Uninstall is complete. You can reinstall Media Flow Broadcast any time from the latest installer."
  !else
    !define MUI_WELCOMEPAGE_TITLE "Install Media Flow Broadcast"
    !define MUI_WELCOMEPAGE_TEXT "Set up the broadcast control suite for media playback, audience display routing, and Zoom/OBS virtual camera output.$\r$\n$\r$\nThe installer will add app shortcuts, bundle the bridge runtime, and check for OBS Studio support during setup."
    !define MUI_FINISHPAGE_TITLE "Media Flow Broadcast is ready"
    !define MUI_FINISHPAGE_TEXT "Setup finished successfully. Launch the app to configure displays, media, and the virtual camera bridge."
  !endif
!macroend

!macro customWelcomePage
  !insertmacro MUI_PAGE_WELCOME
!macroend

!macro customFinishPage
  Function StartApp
    ${if} ${isUpdated}
      StrCpy $1 "--updated"
    ${else}
      StrCpy $1 ""
    ${endif}
    ${StdUtils.ExecShellAsUser} $0 "$launchLink" "open" "$1"
  FunctionEnd

  !ifndef HIDE_RUN_AFTER_FINISH
    !define MUI_FINISHPAGE_RUN
    !define MUI_FINISHPAGE_RUN_FUNCTION "StartApp"
    !define MUI_FINISHPAGE_RUN_TEXT "Launch Media Flow Broadcast"
  !endif
  !insertmacro MUI_PAGE_FINISH
!macroend

!macro customUnWelcomePage
  !insertmacro MUI_UNPAGE_WELCOME
!macroend

!macro customInstall
  DetailPrint "Preparing Media Flow Broadcast..."
  DetailPrint "Installing desktop shortcuts and bundled bridge runtime..."
  DetailPrint "Checking OBS Studio availability for virtual camera output..."

  ReadRegStr $0 HKLM "SOFTWARE\OBS-Studio" ""
  IfErrors 0 +2
    DetailPrint "OBS Studio was not detected. Install OBS Studio to use the virtual camera bridge."

  IfErrors +2 0
    DetailPrint "OBS Studio detected. Virtual camera bridge support is ready."

!macroend

!macro customUnInstall
  DetailPrint "Removing Media Flow Broadcast application files..."
  DetailPrint "Removing app shortcuts and installer registry entries..."
!macroend
