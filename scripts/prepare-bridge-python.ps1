$ErrorActionPreference = "Stop"

$pythonVersion = "3.12.10"
$pythonShortVersion = "312"
$repoRoot = Split-Path -Parent $PSScriptRoot
$runtimeDir = Join-Path $repoRoot "resources\python"
$bridgeDir = Join-Path $repoRoot "resources\bridge"
$requirementsPath = Join-Path $bridgeDir "requirements.txt"
$pythonExe = Join-Path $runtimeDir "python.exe"
$pthPath = Join-Path $runtimeDir "python$pythonShortVersion._pth"
$cacheDir = Join-Path $repoRoot ".cache\python-bridge"
$zipPath = Join-Path $cacheDir "python-$pythonVersion-embed-amd64.zip"
$getPipPath = Join-Path $cacheDir "get-pip.py"
$pythonZipUrl = "https://www.python.org/ftp/python/$pythonVersion/python-$pythonVersion-embed-amd64.zip"
$getPipUrl = "https://bootstrap.pypa.io/get-pip.py"

New-Item -ItemType Directory -Force -Path $runtimeDir, $cacheDir | Out-Null

if (!(Test-Path $pythonExe)) {
  Write-Host "Downloading embedded Python $pythonVersion..."
  Invoke-WebRequest -Uri $pythonZipUrl -OutFile $zipPath

  Write-Host "Extracting embedded Python..."
  Get-ChildItem -LiteralPath $runtimeDir -Force |
    Where-Object { $_.Name -ne "README.md" } |
    Remove-Item -Recurse -Force

  Expand-Archive -LiteralPath $zipPath -DestinationPath $runtimeDir -Force
}

if (Test-Path $pthPath) {
  $pth = Get-Content -LiteralPath $pthPath
  if ($pth -notcontains "import site") {
    Write-Host "Enabling site-packages for embedded Python..."
    $pth | ForEach-Object {
      if ($_ -eq "#import site") { "import site" } else { $_ }
    } | Set-Content -LiteralPath $pthPath -Encoding ASCII
  }
}

if (!(Test-Path $getPipPath)) {
  Write-Host "Downloading pip bootstrap..."
  Invoke-WebRequest -Uri $getPipUrl -OutFile $getPipPath
}

Write-Host "Installing pip into embedded Python..."
& $pythonExe $getPipPath --no-warn-script-location

Write-Host "Installing bridge packages..."
& $pythonExe -m pip install --upgrade --no-warn-script-location pip
& $pythonExe -m pip install --upgrade --no-warn-script-location -r $requirementsPath

$pywin32PostInstall = Join-Path $runtimeDir "Scripts\pywin32_postinstall.py"
if (Test-Path $pywin32PostInstall) {
  Write-Host "Running pywin32 post-install setup..."
  & $pythonExe $pywin32PostInstall -install
}

Write-Host "Verifying embedded bridge runtime..."
& $pythonExe -c "import numpy, pyvirtualcam, win32gui, win32ui; print('embedded bridge runtime ok')"

Write-Host "Embedded Python bridge runtime is ready at $runtimeDir"
