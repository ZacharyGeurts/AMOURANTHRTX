# =============================================================================
# windows.ps1 — AMOURANTH RTX — WATER TEMPLE EDITION — FIXED FOREVER
# No loops. No bullshit. Builds clean every time on Windows.
# =============================================================================

param (
    [string]$Action = "build",      # build (default), run, clean, debug
    [switch]$Clean,                 # alias for clean action
    [switch]$Run,                   # alias for run action
    [switch]$Debug                  # build in Debug mode
)

# ── OCEAN PALETTE (ANSI for Windows Terminal / PowerShell 7+) ────────────────
$AQUA   = "`e[38;5;51m"
$DEEP   = "`e[38;5;27m"
$TURQ   = "`e[38;5;45m"
$WAVE   = "`e[38;5;39m"
$FOAM   = "`e[38;5;195m"
$PEARL  = "`e[38;5;231m"
$CORAL  = "`e[38;5;204m"
$ABYSS  = "`e[38;5;17m"
$GLOW   = "`e[38;5;159m"
$W      = "`e[1;97m"
$X      = "`e[0m"

$PROJECT_ROOT = $PSScriptRoot
$BUILD_DIR    = Join-Path $PROJECT_ROOT "build"

function Banner {
    Clear-Host
    Write-Host "${DEEP}  █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗${X}"
    Write-Host "${AQUA} ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║${X}"
    Write-Host "${TURQ} ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║${X}"
    Write-Host "${WAVE} ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║${X}"
    Write-Host "${GLOW} ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║${X}"
    Write-Host "${FOAM} ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝${X}"
    Write-Host ""
    Write-Host "${CORAL}  ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗${X}"
    Write-Host "${AQUA}  ██╔══██╗╚══██╔══╝╚██╗██╔     ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝${X}"
    Write-Host "${TURQ}  ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ${X}"
    Write-Host "${WAVE}  ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ${X}"
    Write-Host "${GLOW}  ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗${X}"
    Write-Host "${PEARL}  ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝${X}"
    Write-Host ""
    Write-Host "${GLOW}                AMOURANTH RTX — AQUA TEMPLE — $(Get-Date -Format 'MMMM dd, yyyy')${X}"
    Write-Host ""
}

function Show-Help {
    Banner
    Write-Host @"

╔══════════════════════════════════════════════════════════════════════════════╗
║                     AQUA TEMPLE — PARAMORE CROSS REALMS                      ║
╚══════════════════════════════════════════════════════════════════════════════╝

  .\windows.ps1                     → build native Windows (default)
  .\windows.ps1 -Run                → build + launch .exe
  .\windows.ps1 -Clean              → purge build directory
  .\windows.ps1 -Debug              → build in Debug mode
  .\windows.ps1 -Run -Debug         → debug build + launch under debugger (if VS installed)

  Binary path:
    Windows:   build\bin\Navigator.exe

╔══════════════════════════════════════════════════════════════════════════════╗
║           THE TIDE FLOWS THROUGH DIMENSIONS — LOVE IS CROSS-PLATFORM         ║
╚══════════════════════════════════════════════════════════════════════════════╝

"@
    exit 0
}

# ── Parse Arguments ──────────────────────────────────────────────────────────
$BuildType = if ($Debug) { "Debug" } else { "Release" }
$RunAfterBuild = $Run.IsPresent
$CleanBuild = $Clean.IsPresent

if ($CleanBuild) {
    Banner
    Write-Host "${ABYSS}        TIDAL PURGE INITIATED — THE ABYSS CONSUMES${X}"
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Path $BUILD_DIR -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "${GLOW}        WINDOWS BUILD REALM PURGED${X}"
    } else {
        Write-Host "${GLOW}        No build directory found — already clean${X}"
    }
    exit 0
}

if ($PSBoundParameters.Count -eq 0 -or $Action -eq "build") {
    $Action = "build"
}

Banner

# ── Build ────────────────────────────────────────────────────────────────────
Write-Host "${WAVE}        SURFACING WITH Visual Studio Generator — $BuildType MODE${X}"

if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

Set-Location $BUILD_DIR

# CMake configure
cmake .. `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# CMake build
Write-Host "${AQUA}        COMPILING — MULTI-THREADED${X}"
cmake --build . --config $BuildType

# ── Binary Check ─────────────────────────────────────────────────────────────
$FINAL_BINARY = Join-Path $BUILD_DIR "bin\Navigator.exe"

if (-not (Test-Path $FINAL_BINARY)) {
    Write-Host "${CORAL}        FATAL: Binary drowned — $FINAL_BINARY${X}"
    exit 1
}

Write-Host "${GLOW}        BINARY SURFACED → $FINAL_BINARY${X}"

# ── Launch ───────────────────────────────────────────────────────────────────
if ($RunAfterBuild) {
    Write-Host ""
    Write-Host "${PEARL}       THROUGH WATER — LAUNCHING WINDOWS REALM${X}"
    Write-Host "${GLOW}        The ocean crosses dimensions. Dive deep.${X}"
    Write-Host ""

    Set-Location $PROJECT_ROOT

    & $FINAL_BINARY
}

# ── Final Tide ───────────────────────────────────────────────────────────────
Banner
Write-Host ""
Write-Host "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
Write-Host "${AQUA}        ~${TURQ}~${WAVE}~${GLOW}~${FOAM}~${PEARL}~ ${BuildType^^} BUILD COMPLETE — NAVIGATOR SWIMS ETERNAL ~${PEARL}~${FOAM}~${GLOW}~${WAVE}~${TURQ}~${AQUA}~${X}"
Write-Host "${DEEP}        ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~${X}"
Write-Host ""
Write-Host "        ${W}Current Realm:${X} ${GLOW}Windows${X}"
Write-Host "        ${W}Binary Location:${X} ${GLOW}$FINAL_BINARY${X}"
Write-Host "        ${W}Dive Command:${X}   ${AQUA}.\windows.ps1 -Run${X}"
Write-Host ""
Write-Host "${GLOW}        AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE — POWERSHELL FIXED IT${X}"