#!/usr/bin/env bash
# Full KILROY Field OS build: AMOURANTHRTX engine + KILROY kernel + rootfs + Grok boot media.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=kilroy_root.sh
source "$ROOT/scripts/kilroy_root.sh"

JOBS="${JOBS:-$(nproc)}"
SKIP_ENGINE="${SKIP_ENGINE:-0}"
SKIP_KERNEL="${SKIP_KERNEL:-0}"
SKIP_ROOTFS="${SKIP_ROOTFS:-0}"
SKIP_IMAGE="${SKIP_IMAGE:-0}"
QEMU_BOOT="${QEMU_BOOT:-0}"

banner() {
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  KILROY Field OS — AMOURANTHRTX + SG/KILROY full stack       ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo "  KILROY_ROOT: $KILROY_ROOT"
    echo "  AMOURANTHRTX: $ROOT"
}

require_tools() {
    local missing=()
    for tool in cmake make rsync; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "[kilroy-os] missing: ${missing[*]}"
        exit 1
    fi
    if ! command -v g++-14 >/dev/null 2>&1; then
        echo "[kilroy-os] GCC 14+ required (g++-14)"
        exit 1
    fi
}

build_shaders() {
    echo "[kilroy-os] compiling shaders..."
    python3 "$ROOT/Navigator/shaders/compute/sync_canvas_shaders.py" 2>/dev/null || true
    make -C "$ROOT/Navigator/shaders" BUILD_TYPE=Release -j"$JOBS"
}

build_engine() {
    echo "[kilroy-os] building AMOURANTHRTX release..."
    build_shaders
    python3 "$ROOT/Navigator/shaders/compute/sync_canvas_shaders.py" 2>/dev/null || true
    make -C "$ROOT/Navigator/shaders" BUILD_TYPE=Release -j"$JOBS"
    cmake -S "$ROOT" -B "$ROOT/build-release" -DCMAKE_BUILD_TYPE=Release -DFIELD_DOS_EMBED_HD=OFF
    cmake --build "$ROOT/build-release" --target amouranth_engine -j"$JOBS"
    if [[ -x "$ROOT/build-release/bin/Linux/qa_field_kilroy_test" ]]; then
        "$ROOT/build-release/bin/Linux/qa_field_kilroy_test" || true
    fi
}

build_kernel() {
    echo "[kilroy-os] building KILROY Field Die kernel..."
    chmod +x "$ROOT/scripts/build_rtx_kernel.sh"
    GROK_IMAGE=0 OUT="$ROOT/build/kernel-kilroy" "$ROOT/scripts/build_rtx_kernel.sh"
}

build_rootfs() {
    echo "[kilroy-os] building KILROY production rootfs..."
    [[ -x "$KILROY_ROOT/rootfs/build-production-rootfs.sh" ]] || {
        echo "[kilroy-os] missing rootfs builder"
        exit 1
    }
    "$KILROY_ROOT/rootfs/build-production-rootfs.sh"
    chmod +x "$ROOT/scripts/install_kilroy_rootfs.sh"
    "$ROOT/scripts/install_kilroy_rootfs.sh" install
}

build_boot_image() {
    echo "[kilroy-os] building Grok boot media..."
    [[ -x "$KILROY_ROOT/scripts/grok-mkimage.sh" ]] || {
        echo "[kilroy-os] grok-mkimage.sh missing — kernel bzImage still at build/kernel-kilroy/"
        return 0
    }
    BZIMAGE="${BZIMAGE:-$KILROY_ROOT/build/bzImage}" \
        OUT="${OUT:-$KILROY_ROOT/build}" \
        "$KILROY_ROOT/scripts/grok-mkimage.sh"
    echo "[kilroy-os] boot media:"
    ls -lh "$KILROY_ROOT/build/grok-kilroy.img" "$KILROY_ROOT/build/grok-kilroy.iso" 2>/dev/null || true
}

qemu_boot() {
    [[ -x "$KILROY_ROOT/scripts/grok-boot-qemu.sh" ]] || return 0
    GRAPHICAL="${GRAPHICAL:-0}" "$KILROY_ROOT/scripts/grok-boot-qemu.sh"
}

full_build() {
    banner
    require_tools
    [[ "$SKIP_ENGINE" == "1" ]] || build_engine
    [[ "$SKIP_KERNEL" == "1" ]] || build_kernel
    [[ "$SKIP_ROOTFS" == "1" ]] || build_rootfs
    [[ "$SKIP_IMAGE" == "1" ]] || build_boot_image
    echo ""
    echo "[kilroy-os] build complete"
    echo "  Engine:  $ROOT/build-release/bin/Linux/AMOURANTHRTX"
    echo "  Kernel:  $ROOT/build/kernel-kilroy/bzImage"
    echo "  Rootfs:  $KILROY_ROOT/rootfs/production-staging"
    echo "  Boot:    $KILROY_ROOT/build/grok-kilroy.{img,iso}"
    echo ""
    echo "  Install to disk: KILROY_ROOT_DEVICE=/dev/sdX $KILROY_ROOT/rootfs/install-team-drive.sh"
    echo "  QEMU boot:       ./scripts/build_kilroy_os.sh qemu"
    echo "  Betatester:      ./scripts/build_kilroy_os.sh betatest"
    [[ "$QEMU_BOOT" == "1" ]] && qemu_boot
}

case "${1:-all}" in
    all|build)   full_build ;;
    engine)      banner; build_engine ;;
    kernel)      banner; build_kernel ;;
    rootfs)      banner; build_rootfs ;;
    image|iso)   banner; build_boot_image ;;
    qemu|boot)   banner; qemu_boot ;;
    qa)
        banner
        chmod +x "$ROOT/scripts/field_kilroy.sh"
        "$ROOT/scripts/field_kilroy.sh" qa
        ;;
    betatest|test)
        banner
        chmod +x "$ROOT/scripts/betatester_kilroy.sh"
        "$ROOT/scripts/betatester_kilroy.sh" all
        ;;
    help|*)
        banner
        cat <<'EOF'

  ./scripts/build_kilroy_os.sh all      Full stack (engine + kernel + rootfs + Grok image)
  ./scripts/build_kilroy_os.sh engine   AMOURANTHRTX release only
  ./scripts/build_kilroy_os.sh kernel   KILROY bzImage only
  ./scripts/build_kilroy_os.sh rootfs   Production rootfs + AMOURANTHRTX install
  ./scripts/build_kilroy_os.sh image    Grok boot ISO/disk image
  ./scripts/build_kilroy_os.sh qemu     Boot in QEMU (after image build)
  ./scripts/build_kilroy_os.sh qa       FieldKilroy userspace QA

  Env: SKIP_ENGINE=1 SKIP_KERNEL=1 SKIP_ROOTFS=1 SKIP_IMAGE=1 QEMU_BOOT=1
EOF
        ;;
esac