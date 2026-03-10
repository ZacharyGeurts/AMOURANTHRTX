# build-simple.ps1
# =============================================================================
# AMOURANTH RTX — WATER TEMPLE EDITION — Ultra-Simple Windows Build
# Minimal version — almost impossible to fail under Wine or native
# =============================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   AMOURANTH RTX — SIMPLE WINDOWS BUILD (C++23)            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Hardcoded paths — change only if your folder name is different
$ProjectRoot = (Get-Location).Path
$BuildDir    = "$ProjectRoot\build-windows"
$BinDir      = "$BuildDir\bin\Windows"
$Binary      = "$BinDir\Navigator.exe"

# ── Clean if you pass "clean" as argument ───────────────────────────────────
if ($args -contains "clean") {
    Write-Host "TIDAL PURGE — Cleaning build-windows..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
    Write-Host "Done." -ForegroundColor Yellow
    exit
}

# ── Create build folder ─────────────────────────────────────────────────────
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Set-Location $BuildDir

# ── Configure CMake (Release, C++23, Visual Studio) ─────────────────────────
Write-Host "Configuring CMake (Release + C++23)..." -ForegroundColor Green

cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_CXX_STANDARD=23 `
      -DCMAKE_CXX_STANDARD_REQUIRED=ON `
      -DCMAKE_CXX_EXTENSIONS=OFF `
      ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed — check CMake output above" -ForegroundColor Red
    exit 1
}

# ── Build ───────────────────────────────────────────────────────────────────
Write-Host "Building Release..." -ForegroundColor Green

cmake --build . --config Release --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed — check errors above" -ForegroundColor Red
    exit 1
}

# ── Check if binary exists ──────────────────────────────────────────────────
if (Test-Path $Binary) {
    Write-Host ""
    Write-Host "BINARY SURFACED!" -ForegroundColor Cyan
    Write-Host "  Location:  $Binary" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Run it:     $Binary" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "Navigator.exe not found — build may have failed silently" -ForegroundColor Red
}

Write-Host "AQUAMARINE PHOTONS ARE ETERNAL — THE TIDE IS LOVE" -ForegroundColor Magenta