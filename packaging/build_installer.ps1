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

& $windeployqt --qmldir "$PSScriptRoot\..\qml" --release (Join-Path $distDir "MediaFlow.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

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
