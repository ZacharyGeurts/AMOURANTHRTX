#!/usr/bin/env bash
# Stage AMOURANTHRTX + assets into KILROY production rootfs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=kilroy_root.sh
source "$ROOT/scripts/kilroy_root.sh"

ENGINE_BIN="${ENGINE_BIN:-$ROOT/build-release/bin/Linux/AMOURANTHRTX}"
STAGING="${STAGING:-$KILROY_ROOT/rootfs/production-staging}"
PREFIX="${PREFIX:-$STAGING/opt/amouranthrtx}"

banner() {
    echo "=== KILROY rootfs — AMOURANTHRTX Field engine ==="
    echo "    engine:  $ENGINE_BIN"
    echo "    staging: $STAGING"
}

install_engine() {
    [[ -x "$ENGINE_BIN" ]] || {
        echo "[rootfs] engine missing — run: ./linux.sh release"
        exit 1
    }
    mkdir -p "$PREFIX/bin" "$PREFIX/assets"
    cp -f "$ENGINE_BIN" "$PREFIX/bin/AMOURANTHRTX"
    chmod 755 "$PREFIX/bin/AMOURANTHRTX"
    if [[ -d "$ROOT/build-release/bin/Linux/assets" ]]; then
        rsync -a "$ROOT/build-release/bin/Linux/assets/" "$PREFIX/assets/"
    elif [[ -d "$ROOT/assets" ]]; then
        rsync -a "$ROOT/assets/" "$PREFIX/assets/"
    fi
    echo "[rootfs] AMOURANTHRTX -> $PREFIX/bin/AMOURANTHRTX"
}

install_launchers() {
    mkdir -p "$STAGING/usr/bin" "$STAGING/etc/profile.d"
    cat >"$STAGING/usr/bin/amouranthrtx" <<'EOF'
#!/bin/sh
export AMOURANTHRTX_HOME=/opt/amouranthrtx
export LD_LIBRARY_PATH="${AMOURANTHRTX_HOME}/lib:${LD_LIBRARY_PATH:-}"
exec "$AMOURANTHRTX_HOME/bin/AMOURANTHRTX" "$@"
EOF
    chmod 755 "$STAGING/usr/bin/amouranthrtx"

    cat >"$STAGING/etc/profile.d/amouranthrtx.sh" <<'EOF'
# AMOURANTHRTX Field engine (KILROY Field OS)
export PATH="/opt/amouranthrtx/bin:/usr/bin:${PATH}"
export AMOURANTHRTX_HOME=/opt/amouranthrtx
if [ -c /dev/kilroy_field ] && [ -r /proc/kilroy_field/status ]; then
    echo "AMOURANTHRTX Field engine ready — run: amouranthrtx"
fi
EOF
    chmod 644 "$STAGING/etc/profile.d/amouranthrtx.sh"

    if [[ -f "$STAGING/etc/motd" ]]; then
        grep -q 'amouranthrtx' "$STAGING/etc/motd" 2>/dev/null || \
            echo "  amouranthrtx          — Field RTX engine (Vulkan)" >>"$STAGING/etc/motd"
    fi
}

case "${1:-install}" in
    install)
        banner
        install_engine
        install_launchers
        ;;
    help|*)
        banner
        cat <<'EOF'
  ./scripts/install_kilroy_rootfs.sh install
      Copy release AMOURANTHRTX + assets into KILROY production-staging.

  Prereq: ./linux.sh release && $KILROY_ROOT/rootfs/build-production-rootfs.sh
EOF
        ;;
esac