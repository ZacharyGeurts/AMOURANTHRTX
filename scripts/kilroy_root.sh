#!/usr/bin/env bash
# Resolve KILROY Field OS tree (SG/KILROY canonical).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -n "${KILROY_ROOT:-}" && -f "${KILROY_ROOT}/scripts/build-kilroy.sh" ]]; then
    KILROY_RESOLVED="$(cd "$KILROY_ROOT" && pwd)"
elif [[ -f "$ROOT/../SG/KILROY/scripts/build-kilroy.sh" ]]; then
    KILROY_RESOLVED="$(cd "$ROOT/../SG/KILROY" && pwd)"
elif [[ -f "$ROOT/../KILROY/scripts/build-kilroy.sh" ]]; then
    KILROY_RESOLVED="$(cd "$ROOT/../KILROY" && pwd)"
else
    KILROY_RESOLVED="$ROOT/../SG/KILROY"
fi

export KILROY_ROOT="$KILROY_RESOLVED"

if [[ "${1:-}" == "--print" ]]; then
    echo "$KILROY_RESOLVED"
elif [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
    :
else
    echo "KILROY_ROOT=$KILROY_RESOLVED"
fi