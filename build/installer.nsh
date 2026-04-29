
!macro customInstall
  DetailPrint "Registering Mediaflow Virtual Camera Driver..."
  ExecWait 'regsvr32.exe /s "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll"'
  
  # Rename to Mediaflow in registry
  WriteRegStr HKCU "Software\Classes\CLSID\{8E145426-DB61-4972-A6D3-F67EF983A006}" "" "Mediaflow"
!macroend

!macro customUnInstall
  DetailPrint "Unregistering Mediaflow Virtual Camera Driver..."
  ExecWait 'regsvr32.exe /u /s "$INSTDIR\resources\drivers\UnityCaptureFilter64.dll"'
  DeleteRegKey HKCU "Software\Classes\CLSID\{8E145426-DB61-4972-A6D3-F67EF983A006}"
!macroend
