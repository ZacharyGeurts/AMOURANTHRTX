#!/usr/bin/env bash
# FieldKilroy — KILROY 7.1.1 Field Die bridge (userspace QA + kernel build).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=kilroy_root.sh
source "$ROOT/scripts/kilroy_root.sh"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"

banner() {
    echo "=== FieldKilroy — KILROY Field Die + AMOURANTHRTX ==="
    echo "    Telemetry: /proc/kilroy_field/status"
    echo "    KILROY:    $KILROY_ROOT"
}

build_compat() {
    echo "[FieldKilroy] Building compat QA..."
    if ! command -v cmake >/dev/null 2>&1; then
        g++-14 -std=c++23 -fstack-protector-strong -D_FORTIFY_SOURCE=3 \
            -I Navigator/engine -I FieldKilroy -I include/uapi \
            -I AmmoOS -I AmmoOS/core -I AmmoOS/windowing -I dos -I AmmoOS/data \
            scripts/qa_field_kilroy_test.cpp FieldKilroy/FieldKilroyBridge.cpp \
            FieldKilroy/FieldKilroyKernel.cpp -o /tmp/qa_field_kilroy_test -lpthread
        echo "[FieldKilroy] qa_field_kilroy_test -> /tmp/qa_field_kilroy_test"
        return 0
    fi
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --target qa_field_kilroy_test -j"$(nproc)"
}

build_kernel() {
    chmod +x "$ROOT/scripts/build_rtx_kernel.sh"
    "$ROOT/scripts/build_rtx_kernel.sh"
}

run_qa() {
    build_compat
    if [[ -x /tmp/qa_field_kilroy_test ]]; then
        /tmp/qa_field_kilroy_test
    elif [[ -x "$BUILD_DIR/bin/Linux/qa_field_kilroy_test" ]]; then
        "$BUILD_DIR/bin/Linux/qa_field_kilroy_test"
    elif [[ -x "$ROOT/build/bin/Linux/qa_field_kilroy_test" ]]; then
        "$ROOT/build/bin/Linux/qa_field_kilroy_test"
    else
        echo "[FieldKilroy] qa binary not found"
        exit 1
    fi
}

case "${1:-help}" in
    build)   banner; build_compat; build_kernel ;;
    kernel)  banner; build_kernel ;;
    qa|test) banner; run_qa ;;
    os)
        chmod +x "$ROOT/scripts/build_kilroy_os.sh"
        "$ROOT/scripts/build_kilroy_os.sh" "${@:2}"
        ;;
    syscalls)
        if [[ -f "$KILROY_ROOT/scripts/kilroy-compat-path.sh" ]]; then
            # shellcheck source=/dev/null
            source "$KILROY_ROOT/scripts/kilroy-compat-path.sh"
            python3 "$ROOT/scripts/gen_kilroy_syscalls.py" "$KILROY_COMPAT_SRC" 2>/dev/null || true
        fi
        ;;
    help|*)
        banner
        cat <<'EOF'
  ./scripts/field_kilroy.sh build   — QA + KILROY Field Die bzImage
  ./scripts/field_kilroy.sh kernel  — build KILROY kernel only
  ./scripts/field_kilroy.sh qa      — userspace syscall router test
  ./scripts/field_kilroy.sh os      — full bootable OS (see build_kilroy_os.sh)
  ./scripts/field_kilroy.sh syscalls — KILROY 7.1.1 syscall table

  Boot: build/kernel-kilroy/bzImage — /proc/kilroy_field telemetry when running KILROY kernel.
EOF
        ;;
esac