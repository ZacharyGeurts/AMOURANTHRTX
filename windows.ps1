# build-simple.ps1
# =============================================================================
# AMOURANTH RTX — WATER TEMPLE EDITION — Ultra-Simple Windows Build
# Minimal version — almost impossible to fail under native Windows
# Untested. Best wishes.
# =============================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   AMOURANTH RTX — SIMPLE WINDOWS BUILD (C++23 / VS2022)   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ── Configuration ───────────────────────────────────────────────────────────
$ProjectRoot = (Get-Location).Path
$BuildDir    = Join-Path $ProjectRoot "build-windows"
$BinDir      = Join-Path $BuildDir "bin\Windows"
$Binary      = Join-Path $BinDir "Navigator.exe"

# ── Clean if requested ──────────────────────────────────────────────────────
if ($args -contains "clean") {
    Write-Host "TIDAL PURGE — Cleaning build-windows..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
    Write-Host "Done." -ForegroundColor Yellow
    exit 0
}

# ── Create build folder ─────────────────────────────────────────────────────
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
Set-Location $BuildDir

# ── Try to locate & run Developer PowerShell for VS 2022 ────────────────────
Write-Host "Looking for Visual Studio 2022 Developer environment..." -ForegroundColor Cyan

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: vswhere.exe not found → Visual Studio 2022 appears not to be installed" -ForegroundColor Red
    Write-Host "       Install VS2022 with 'Desktop development with C++' workload." -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $vsPath) {
    Write-Host "ERROR: Could not find Visual Studio 2022 installation with C++ tools" -ForegroundColor Red
    exit 1
}

$devShell = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

if (-not (Test-Path $devShell)) {
    Write-Host "ERROR: Microsoft.VisualStudio.DevShell.dll not found" -ForegroundColor Red
    exit 1
}

Write-Host "Found VS2022 at: $vsPath" -ForegroundColor Green

Import-Module $devShell -Force
Enter-VsDevShell -VsInstallPath $vsPath -SkipExistingEnvironmentVariables -DevCmdArguments "-arch=amd64 -host_arch=amd64"

# Quick sanity check
if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    Write-Host "WARNING: cl.exe still not found after DevShell — build will probably fail" -ForegroundColor Yellow
}

# ── Configure CMake ─────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Configuring CMake (Visual Studio 17 2022, Release, static runtime)..." -ForegroundColor Green

cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_STANDARD=23 `
    -DCMAKE_CXX_STANDARD_REQUIRED=ON `
    -DCMAKE_CXX_EXTENSIONS=OFF `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
    -DBUILD_SHARED_LIBS=OFF

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed — see errors above" -ForegroundColor Red
    Write-Host "(Common causes: missing Vulkan SDK, Git not in PATH, antivirus blocking downloads)" -ForegroundColor Yellow
    exit 1
}

# ── Build ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Building Release configuration..." -ForegroundColor Green

cmake --build . --config Release --parallel 8

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed — check errors above" -ForegroundColor Red
    exit 1
}

# ── Final report ────────────────────────────────────────────────────────────
Write-Host ""
if (Test-Path $Binary) {
    Write-Host "BINARY SURFACED!" -ForegroundColor Cyan
    Write-Host "  Location:  $Binary" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Run it directly (double-click or):" -ForegroundColor Cyan
    Write-Host "  $Binary" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "Navigator.exe not found in $BinDir" -ForegroundColor Red
    Write-Host "Build may have failed or target name is different" -ForegroundColor Yellow
}

Write-Host "AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE" -ForegroundColor Magenta