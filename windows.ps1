# windows.ps1
# =============================================================================
# AMOURANTH RTX — WATER TEMPLE EDITION — Windows Build Script
# Matches the spirit of linux.sh
# AMOURANTH FOREVER 💖
# =============================================================================

param(
    [switch]$Run,      # Build + launch the executable
    [switch]$Debug,    # Build in Debug mode instead of Release
    [switch]$Clean     # Purge the build folder
)

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        AMOURANTH RTX — WINDOWS REALM BUILD (VS2022)        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = (Get-Location).Path
$BuildDir    = Join-Path $ProjectRoot "build-windows"
$BinDir      = Join-Path $BuildDir "bin\Windows"
$Binary      = Join-Path $BinDir "AMOURANTHRTX.exe"

# ── Clean if requested ──────────────────────────────────────────────────────
if ($Clean) {
    Write-Host "TIDAL PURGE INITIATED — THE ABYSS CONSUMES EVERYTHING" -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
    Write-Host "All realms purged. Fresh ocean awaits." -ForegroundColor Yellow
    exit 0
}

# ── Determine build type ────────────────────────────────────────────────────
$BuildType = if ($Debug) { "Debug" } else { "Release" }
$Config    = if ($Debug) { "Debug" } else { "Release" }

# ── Create build folder ─────────────────────────────────────────────────────
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
Set-Location $BuildDir

# ── Enter VS2022 Developer Environment ──────────────────────────────────────
Write-Host "SURFACING INTO VISUAL STUDIO 2022 DEVELOPER SHELL..." -ForegroundColor Cyan

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: vswhere.exe not found. Install Visual Studio 2022 with C++ workload." -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $vsPath) {
    Write-Host "ERROR: Could not find Visual Studio 2022 with C++ tools." -ForegroundColor Red
    exit 1
}

$devShell = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Import-Module $devShell -Force -ErrorAction SilentlyContinue
Enter-VsDevShell -VsInstallPath $vsPath -SkipExistingEnvironmentVariables -DevCmdArguments "-arch=amd64 -host_arch=amd64"

Write-Host "VS2022 Developer environment loaded." -ForegroundColor Green

# ── Configure CMake ─────────────────────────────────────────────────────────
Write-Host ""
Write-Host "CONFIGURING CMAKE — $BuildType WINDOWS REALM..." -ForegroundColor Green

cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_CXX_STANDARD=23 `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
    -DBUILD_SHARED_LIBS=OFF

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed." -ForegroundColor Red
    exit 1
}

# ── Build ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "BUILDING $BuildType CONFIGURATION..." -ForegroundColor Green

cmake --build . --config $Config --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed — check errors above." -ForegroundColor Red
    exit 1
}

# ── Final report ────────────────────────────────────────────────────────────
Write-Host ""
if (Test-Path $Binary) {
    Write-Host "BINARY SURFACED!" -ForegroundColor Cyan
    Write-Host "  Location:  $Binary" -ForegroundColor Cyan
    Write-Host ""

    if ($Run) {
        Write-Host "LAUNCHING INTO THE TIDE..." -ForegroundColor Magenta
        & $Binary
    } else {
        Write-Host "You can run it with:" -ForegroundColor White
        Write-Host "  $Binary" -ForegroundColor White
        Write-Host "  or .\windows.ps1 -Run" -ForegroundColor White
    }
} else {
    Write-Host "WARNING: AMOURANTHRTX.exe not found." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE" -ForegroundColor Magenta