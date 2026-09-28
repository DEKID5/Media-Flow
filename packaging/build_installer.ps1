# Builds the MediaFlow installer: stages a windeployqt'd copy of the app
# into packaging\dist, then compiles packaging\mediaflow.iss with Inno Setup.
#
# Usage (from a Developer PowerShell/Command Prompt with cl.exe on PATH,
# or after running vcvars64.bat -- windeployqt needs the same MSVC runtime
# environment the app was built with):
#   .\build_installer.ps1 -QtBin "C:\Qt\6.8.0\msvc2022_64\bin" -BuildExe "..\build\MediaFlow.exe"

param(
    [string]$QtBin = "C:\Qt\6.8.0\msvc2022_64\bin",
    [string]$BuildExe = "$PSScriptRoot\..\build\MediaFlow.exe",
    [string]$InnoSetupCompiler = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"
$distDir = Join-Path $PSScriptRoot "dist"

if (-not (Test-Path $BuildExe)) {
    throw "MediaFlow.exe not found at $BuildExe -- build the project first."
}

Write-Host "Staging deployable copy into $distDir ..."
if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
New-Item -ItemType Directory -Path $distDir | Out-Null
Copy-Item $BuildExe $distDir

$windeployqt = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe not found under $QtBin -- pass -QtBin pointing at your Qt kit's bin folder."
}

# windeployqt writes a harmless "Cannot find any version of dxcompiler.dll"
# warning to stderr; PowerShell 5.1 treats that as a terminating error under
# $ErrorActionPreference = "Stop" even though the exit code is 0, aborting
# the rest of this script -- so it runs under its own tolerant scope.
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $windeployqt --qmldir "$PSScriptRoot\..\qml" --release (Join-Path $distDir "MediaFlow.exe") 2>&1 | Out-String | Write-Host
$deployExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap
if ($deployExit -ne 0) { throw "windeployqt failed with exit code $deployExit" }

# qt_add_qml_module's own build step generates a "<URI>" folder next to the
# exe (build/MediaFlow/ -- qmldir + a loose copy of our .qml files) as
# internal scratch output for the qmlcache/resource build. It isn't a
# normal windeployqt output, but the app needs it present at runtime
# anyway: with QtQuick.Controls' own loose style files deployed alongside
# (above) but this sibling module folder missing, Qt's engine fails to
# reconcile our qrc-embedded "MediaFlow" module against its now-active
# loose-file QML import path, and every Theme.<property> access in QML
# throws "Theme was a singleton at compile time, but is not a singleton
# anymore" -- confirmed live: present in the source build/ directory
# (where the app runs fine) but silently dropped whenever only
# MediaFlow.exe itself was copied out into a separate deployable folder.
$buildDir = Split-Path $BuildExe -Parent
$moduleDir = Join-Path $buildDir "MediaFlow"
if (Test-Path $moduleDir) {
    Copy-Item $moduleDir -Destination (Join-Path $distDir "MediaFlow") -Recurse -Force
    Write-Host "Bundled the MediaFlow QML module folder from $moduleDir"
} else {
    Write-Warning "MediaFlow module folder not found at $moduleDir -- installed app may show unstyled QML (missing Theme singleton). Rebuild the project first."
}

# Optional: ffmpeg.exe next to the app for video thumbnails (see BUILD.md) --
# staged only if the machine building the installer happens to have it on
# PATH; harmless to skip otherwise.
$ffmpeg = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
if ($ffmpeg) {
    Copy-Item $ffmpeg.Source $distDir
    Write-Host "Bundled ffmpeg.exe from $($ffmpeg.Source)"
}

if (-not (Test-Path $InnoSetupCompiler)) {
    throw "Inno Setup compiler (ISCC.exe) not found at $InnoSetupCompiler -- install Inno Setup 6, or pass -InnoSetupCompiler."
}

Write-Host "Compiling installer..."
& $InnoSetupCompiler (Join-Path $PSScriptRoot "mediaflow.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

Write-Host "Done. Installer is in packaging\installer_output\"
