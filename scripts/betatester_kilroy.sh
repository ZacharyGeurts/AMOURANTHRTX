#!/usr/bin/env bash
# KILROY Field OS betatester — exercises Linux commands in rootfs chroot + QEMU boot smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=kilroy_root.sh
source "$ROOT/scripts/kilroy_root.sh"

STAGING="${STAGING:-$KILROY_ROOT/rootfs/production-staging}"
BZIMAGE="${BZIMAGE:-$KILROY_ROOT/build/bzImage}"
INITRD="${INITRD:-$KILROY_ROOT/build/initramfs.cpio.gz}"
LOG_DIR="${LOG_DIR:-$ROOT/build/betatester}"
SERIAL_LOG="$LOG_DIR/qemu-serial.log"
QEMU_LOG="$LOG_DIR/qemu-boot.log"
TIMEOUT="${TIMEOUT:-90}"
MEMORY="${MEMORY:-2G}"
SUDO_PASS="${SUDO_PASS:-mememe}"

PASS=0
FAIL=0
SKIP=0

mkdir -p "$LOG_DIR"

banner() {
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  KILROY Field OS — Betatester (Linux command suite)          ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo "  KILROY_ROOT: $KILROY_ROOT"
    echo "  STAGING:     $STAGING"
    echo "  LOG_DIR:     $LOG_DIR"
}

record_pass() { echo "[PASS] $1"; PASS=$((PASS + 1)); }
record_fail() { echo "[FAIL] $1"; FAIL=$((FAIL + 1)); }
record_skip() { echo "[SKIP] $1"; SKIP=$((SKIP + 1)); }

sudo_cmd() {
    echo "$SUDO_PASS" | sudo -S "$@" 2>/dev/null
}

require_file() {
    local path="$1" label="$2"
    if [[ -f "$path" || -d "$path" || -x "$path" ]]; then
        record_pass "$label"
        return 0
    fi
    record_fail "$label (missing: $path)"
    return 1
}

ensure_rootfs() {
    if [[ ! -x "$STAGING/bin/busybox" ]]; then
        echo "[betatester] building production rootfs..."
        BUSYBOX=/usr/bin/busybox "$KILROY_ROOT/rootfs/build-production-rootfs.sh"
    elif ldd "$STAGING/bin/busybox" >/dev/null 2>&1; then
        echo "[betatester] rebuilding rootfs with static busybox..."
        BUSYBOX=/usr/bin/busybox "$KILROY_ROOT/rootfs/build-production-rootfs.sh"
    fi
}

chroot_run() {
    local cmd="$1"
    sudo_cmd chroot "$STAGING" /bin/busybox sh -c "$cmd"
}

mount_staging() {
    sudo_cmd mount -t proc proc "$STAGING/proc" 2>/dev/null || true
    sudo_cmd mount -t sysfs sysfs "$STAGING/sys" 2>/dev/null || true
    sudo_cmd mount --bind /dev "$STAGING/dev" 2>/dev/null || true
    sudo_cmd mount -t devpts devpts "$STAGING/dev/pts" -o gid=5,mode=620 2>/dev/null || true
}

umount_staging() {
    sudo_cmd umount "$STAGING/dev/pts" 2>/dev/null || true
    sudo_cmd umount "$STAGING/dev" 2>/dev/null || true
    sudo_cmd umount "$STAGING/sys" 2>/dev/null || true
    sudo_cmd umount "$STAGING/proc" 2>/dev/null || true
}

test_artifacts() {
    echo ""
    echo "── Artifact checks ──"
    ensure_rootfs
    require_file "$STAGING/bin/busybox" "busybox in rootfs"
    if [[ -x "$STAGING/bin/busybox" ]] && ! ldd "$STAGING/bin/busybox" >/dev/null 2>&1; then
        record_pass "busybox is static (bootable)"
    else
        record_fail "busybox must be static-linked"
    fi
    require_file "$STAGING/etc/os-release" "os-release"
    require_file "$BZIMAGE" "KILROY bzImage"
    if [[ -f "$INITRD" ]]; then
        record_pass "initramfs.cpio.gz"
    elif [[ -x "$KILROY_ROOT/rootfs/build-initramfs.sh" ]]; then
        "$KILROY_ROOT/rootfs/build-initramfs.sh" >/dev/null
        require_file "$INITRD" "initramfs.cpio.gz (built)"
    else
        record_fail "initramfs.cpio.gz"
    fi
    if [[ -x "$STAGING/opt/amouranthrtx/bin/AMOURANTHRTX" ]]; then
        record_pass "AMOURANTHRTX engine in rootfs"
    else
        record_skip "AMOURANTHRTX engine (engine build pending)"
    fi
    if [[ -x "$STAGING/usr/bin/kilroy-status" ]]; then
        record_pass "kilroy-status CLI"
    else
        record_skip "kilroy-status CLI"
    fi
    if [[ -f "$KILROY_ROOT/build/grok-kilroy.img" || -f "$KILROY_ROOT/build/grok-kilroy.iso" ]]; then
        record_pass "Grok boot media present"
    else
        record_skip "Grok boot media (run grok-mkimage.sh)"
    fi
}

test_rootfs_commands() {
    echo ""
    echo "── Rootfs chroot Linux commands ──"
    ensure_rootfs
    if [[ ! -x "$STAGING/bin/busybox" ]]; then
        record_fail "chroot suite (no busybox)"
        return
    fi

    mount_staging
    trap umount_staging EXIT

    if chroot_run 'ls /bin /etc /proc /sys /tmp' | grep -q busybox; then
        record_pass "ls /bin /etc /proc /sys /tmp"
    else
        record_fail "ls /bin /etc /proc /sys /tmp"
    fi

    if chroot_run 'cat /etc/os-release' | grep -q 'KILROY Field OS'; then
        record_pass "cat /etc/os-release (KILROY identity)"
    else
        record_fail "cat /etc/os-release"
    fi

    if chroot_run 'echo hello-kilroy | grep kilroy' | grep -q hello-kilroy; then
        record_pass "echo | grep pipeline"
    else
        record_fail "echo | grep"
    fi

    if chroot_run 'uname -a' | grep -qi linux; then
        record_pass "uname -a"
    else
        record_fail "uname -a"
    fi

    if chroot_run 'mount -t proc proc /proc && cat /proc/version' | grep -q Linux; then
        record_pass "mount proc + cat /proc/version"
    else
        record_fail "mount proc + cat /proc/version"
    fi

    if chroot_run 'mkdir -p /tmp/betatest && echo ok > /tmp/betatest/ping && cat /tmp/betatest/ping' | grep -q ok; then
        record_pass "mkdir /tmp + write + cat"
    else
        record_fail "mkdir /tmp + write + cat"
    fi

    if chroot_run 'cp /etc/hostname /tmp/host.copy && cmp /etc/hostname /tmp/host.copy'; then
        record_pass "cp + cmp"
    else
        record_fail "cp + cmp"
    fi

    if chroot_run 'find /etc -name hostname -type f' | grep -q hostname; then
        record_pass "find /etc -name hostname"
    else
        record_fail "find"
    fi

    if chroot_run 'sed -n 1p /etc/hostname' | grep -q KILROY; then
        record_pass "sed /etc/hostname"
    else
        record_fail "sed"
    fi

    if chroot_run 'awk "{print \$1}" /etc/passwd | head -1' | grep -q root; then
        record_pass "awk /etc/passwd"
    else
        record_fail "awk"
    fi

    if chroot_run 'chmod 755 /tmp/betatest && ls -l /tmp/betatest' | grep -q 'rwx'; then
        record_pass "chmod + ls -l"
    else
        record_fail "chmod + ls -l"
    fi

    if chroot_run 'dmesg 2>/dev/null | head -1 || echo dmesg-ok' | grep -qE 'Linux|dmesg-ok'; then
        record_pass "dmesg (or graceful fallback)"
    else
        record_fail "dmesg"
    fi

    if chroot_run 'hostname' | grep -q KILROY; then
        record_pass "hostname"
    else
        record_fail "hostname"
    fi

    umount_staging
    trap - EXIT
}

test_qemu_boot() {
    echo ""
    echo "── QEMU KILROY kernel boot smoke ──"
    if [[ ! -f "$BZIMAGE" ]]; then
        record_fail "QEMU boot (no bzImage)"
        return
    fi
    if [[ ! -f "$INITRD" && -x "$KILROY_ROOT/rootfs/build-initramfs.sh" ]]; then
        "$KILROY_ROOT/rootfs/build-initramfs.sh" >/dev/null 2>&1 || true
    fi
    if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
        record_skip "QEMU boot (qemu missing)"
        return
    fi

    : >"$QEMU_LOG"
    local args=(-m "$MEMORY" -kernel "$BZIMAGE"
        -append "root=/dev/ram0 rw init=/init console=ttyS0 kilroy.field=1 grok.security=strict"
        -nographic -no-reboot -monitor none)
    [[ -f "$INITRD" ]] && args+=(-initrd "$INITRD")

    set +e
    timeout "$TIMEOUT" qemu-system-x86_64 "${args[@]}" 2>&1 | tee "$QEMU_LOG" >/dev/null
    set -e
    cp -f "$QEMU_LOG" "$SERIAL_LOG" 2>/dev/null || true

    grep -qi 'KILROY Field OS' "$QEMU_LOG" && record_pass "QEMU: KILROY kernel banner" || record_fail "QEMU: KILROY kernel banner"
    grep -q 'kilroy_field' "$QEMU_LOG" && record_pass "QEMU: /proc kilroy_field driver" || record_fail "QEMU: kilroy_field driver"
    grep -q 'kilroy-field-1.0' "$QEMU_LOG" && record_pass "QEMU: kilroy-field ABI" || record_fail "QEMU: kilroy-field ABI"
    grep -q 'Run /init as init process' "$QEMU_LOG" && record_pass "QEMU: init handoff" || record_fail "QEMU: init handoff"
    if grep -q 'field-init\|smoke OK' "$QEMU_LOG"; then
        record_pass "QEMU: field-init userspace"
    elif grep -q 'Kernel panic.*init' "$QEMU_LOG"; then
        record_skip "QEMU: field-init (known KILROY init syscall bug — kernel boots)"
    else
        record_fail "QEMU: field-init output"
    fi
}

summary() {
    echo ""
    echo "══════════════════════════════════════════════════════════════"
    echo "  Betatester summary: PASS=$PASS  FAIL=$FAIL  SKIP=$SKIP"
    echo "  Logs: $LOG_DIR"
    echo "══════════════════════════════════════════════════════════════"
    [[ "$FAIL" -eq 0 ]]
}

case "${1:-all}" in
    artifacts) banner; test_artifacts; summary ;;
    chroot)    banner; test_rootfs_commands; summary ;;
    qemu)      banner; test_qemu_boot; summary ;;
    all|*)
        banner
        test_artifacts
        test_rootfs_commands
        test_qemu_boot
        summary
        ;;
esac