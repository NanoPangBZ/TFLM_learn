#!/usr/bin/env pwsh
# One-command, self-contained environment setup for Windows.
# Everything lands inside this repo (.venv + third_party); nothing is installed
# system-wide, so the host machine stays clean.
#
# Usage (from the repo root, PowerShell):
#   powershell -ExecutionPolicy Bypass -File .\setup.ps1
$ErrorActionPreference = 'Stop'
Set-Location -Path $PSScriptRoot

Write-Host "==> TFLM_learn environment setup (Windows) =="

# ---- 1. Locate a compatible Python (3.10 - 3.13) ----
$pyExe = $null
$pyVer = $null
$candidates = @(
    @('py', '-3.12'),
    @('py', '-3.11'),
    @('py', ''),
    @('python', ''),
    @('python3', '')
)
foreach ($c in $candidates) {
    $exe = $c[0]; $arg = $c[1]
    if (-not (Get-Command $exe -ErrorAction SilentlyContinue)) { continue }
    $pyArgs = @(); if ($arg) { $pyArgs += $arg }
    try {
        $ver = (& $exe @pyArgs -c "import sys; print('%d.%d' % sys.version_info[:2])" 2>$null).Trim()
        if ($ver -match '^3\.(10|11|12|13)$') {
            $pyExe = (& $exe @pyArgs -c "import sys; print(sys.executable)" 2>$null).Trim()
            $pyVer = $ver
            break
        }
    } catch { }
}
if (-not $pyExe) {
    Write-Error "No compatible Python 3.10-3.13 found. Install one from https://www.python.org and re-run."
    exit 1
}
Write-Host "==> Using Python $pyVer ($pyExe)"

# ---- 2. Create the virtual environment inside the repo ----
$venvDir = Join-Path $PSScriptRoot '.venv'
if (-not (Test-Path (Join-Path $venvDir 'Scripts\python.exe'))) {
    & $pyExe -m venv $venvDir
}
$venvPy = Join-Path $venvDir 'Scripts\python.exe'
Write-Host "==> Virtual environment ready: $venvDir"

# ---- 3. Install Python dependencies into the venv ----
& $venvPy -m pip install --upgrade pip
& $venvPy -m pip install -r (Join-Path $PSScriptRoot 'requirements.txt')

# ---- 4. Vendor SDL3 / SDL3_ttf / 字体 into third_party (no system install) ----
& $venvPy (Join-Path $PSScriptRoot 'scripts\fetch_deps.py')

Write-Host ""
Write-Host "==> Environment ready. Installed:"
Write-Host "    Python deps -> .venv\"
Write-Host "    SDL3        -> third_party\SDL3\"
Write-Host ""
Write-Host "Verify with:"
Write-Host "    .venv\Scripts\python.exe verify\01_tensorflow_smoke.py"
Write-Host "    cmake -S verify\02_sdl3_smoke -B verify\02_sdl3_smoke\build && cmake --build verify\02_sdl3_smoke\build"
