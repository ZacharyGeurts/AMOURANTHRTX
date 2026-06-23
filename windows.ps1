# windows.ps1
# =============================================================================
# AMOURANTH RTX — WATER TEMPLE EDITION — Windows Build Script
# Matches the spirit of linux.sh
# AMOURANTH FOREVER 💖
# =============================================================================

param(
    [switch]$Run,           # Build + launch the executable
    [switch]$Debug,         # Build in Debug mode instead of Release
    [switch]$Clean,         # Purge the build folder
    [switch]$Dual,          # Alias: --dual (same as -DualHost)
    [switch]$DualHost,      # Enable Linux+Windows simultaneous guest shim
    [switch]$TeamDrive,     # TEAM empty nvme2n1 harness (non-destructive)
    [switch]$FieldStorageV2 # FieldStorage v2 multi-FS + physics Bo layer
    [switch]$Infinite       # Infinite SDF wave logical capacity
    [switch]$Chips          # CHIPS expansion tier
    [switch]$ChipsPs1       # CHIPS PS1 GPU die wave
    [switch]$FieldEmulator  # Field emulator dispatch
    [switch]$ChipsAll      # All CHIPS expansion tier
    [switch]$AmigaLove     # Amiga Love of EVERYTHING canvas
    [switch]$Xbox360       # Xbox 360 Xenos die wave
    [switch]$AllBreakthroughs # Hyper physics breakthroughs 1-4
    [switch]$ExtendedField  # Extended non-point wave dispatch
    [switch]$EndGame       # END OF EVERYTHING — all flags + full QA matrix
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

    if ($Dual -or $DualHost) { $env:AMOURANTHRTX_DUAL_HOST = "1" }
    if ($TeamDrive) { $env:AMOURANTHRTX_TEAM_DRIVE = "1" }
    if ($FieldStorageV2) { $env:AMOURANTHRTX_FIELD_STORAGE_V2 = "1" }
    if ($Infinite) { $env:AMOURANTHRTX_INFINITE = "1" }
    if ($Chips) { $env:AMOURANTHRTX_CHIPS = "1" }
    if ($ChipsPs1) { $env:AMOURANTHRTX_CHIPS_PS1 = "1"; $env:AMOURANTHRTX_CHIPS = "1" }
    if ($FieldEmulator) { $env:AMOURANTHRTX_FIELD_EMULATOR = "1" }
    if ($ChipsAll) { $env:AMOURANTHRTX_CHIPS_ALL = "1"; $env:AMOURANTHRTX_CHIPS = "1" }
    if ($AmigaLove) { $env:AMOURANTHRTX_AMIGA_LOVE = "1" }
    if ($Xbox360) { $env:AMOURANTHRTX_XBOX360 = "1" }
    if ($AllBreakthroughs) { $env:AMOURANTHRTX_ALL_BREAKTHROUGHS = "1"; $env:AMOURANTHRTX_EXTENDED_FIELD = "1" }
    if ($ExtendedField) { $env:AMOURANTHRTX_EXTENDED_FIELD = "1" }
    if ($Infinite) { $env:AMOURANTHRTX_INFINITE = "1" }
    if ($EndGame) {
        $env:AMOURANTHRTX_END_GAME = "1"
        $env:AMOURANTHRTX_INFINITE = "1"
        $env:AMOURANTHRTX_DUAL_HOST = "1"
        $env:AMOURANTHRTX_TEAM_DRIVE = "1"
        $env:AMOURANTHRTX_FIELD_STORAGE_V2 = "1"
        $env:AMOURANTHRTX_CHIPS = "1"
        $env:AMOURANTHRTX_CHIPS_PS1 = "1"
        $env:AMOURANTHRTX_CHIPS_ALL = "1"
        $env:AMOURANTHRTX_AMIGA_LOVE = "1"
        $env:AMOURANTHRTX_XBOX360 = "1"
        $env:AMOURANTHRTX_ALL_BREAKTHROUGHS = "1"
        $env:AMOURANTHRTX_EXTENDED_FIELD = "1"
        $env:AMOURANTHRTX_CHIPS_FABRIC = "1"
        $env:AMOURANTHRTX_FIELD_EMULATOR = "1"
    }

    if ($EndGame) {
        Write-Host "End-game QA matrix (fast)..." -ForegroundColor Cyan
        python "$ProjectRoot\scripts\end_game_audit.py"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    if ($TeamDrive) {
        Write-Host "TEAM drive harness (non-destructive)..." -ForegroundColor Cyan
        python "$ProjectRoot\scripts\team_drive_test.py"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    if ($FieldStorageV2) {
        Write-Host "FieldStorage v2 bench..." -ForegroundColor Cyan
        python "$ProjectRoot\scripts\bench_storage.py"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

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