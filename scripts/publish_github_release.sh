#!/usr/bin/env bash
# Publish AMOURANTHRTX GitHub release — tag, notes, optional engine tarball.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(python3 -c "import sys; sys.path.insert(0,'scripts'); from ammo_platform import AMOURANTHRTX_VERSION; print(AMOURANTHRTX_VERSION)")"
TAG="v${VERSION}"
ENGINE="$ROOT/build-release/bin/Linux/AMOURANTHRTX"
NOTES="$ROOT/build/release_notes_${VERSION}.md"

echo "METRIC amouranthrtx_version=${VERSION}"
echo "=== Build release binary ==="
./linux.sh release

echo "=== Release QA checklist ==="
./linux.sh release-2.0

echo "=== OCR click toolkit ==="
python3 scripts/qa_ocr_click_test.py

NOTES_TMP="$(mktemp)"
cat > "$NOTES_TMP" <<EOF
# AMOURANTHRTX ${VERSION}

## Highlights
- Keen LZEXE EGA path — GpuLaunch titleForcePaint + ip_progress probe (KEEN4E.EXE)
- 4K default viewport (3840×2160) — live taskbar clock, 40px tab hits, OCR alignment
- FieldStorage wave resonance persist — powered-off hold + infinite 12 GiB SDF density
- CHIPS expansion (PS1/N64/DC/PS2/Xbox360/Amiga) — full real, MAME superiority benches
- JetBrains Mono SDF font + interactive test browser (\`./seetests.sh\`)

## QA
- \`./linux.sh release-2.0\` — GREEN ALL (includes qa_keen_host_test)
- \`./build/qa_keen_host_test\` — keen_title_paint=1 keen_ip_progress=1
- \`./seetests.sh aos\` — AOS + OCR visual suite

## Run
\`\`\`bash
./linux.sh
./seetests.sh
\`\`\`

Built: $(date -u '+%Y-%m-%d %H:%M UTC')
EOF
mv "$NOTES_TMP" "$NOTES"

if ! command -v gh >/dev/null 2>&1; then
    echo "WARN: gh CLI missing — skipping GitHub release upload"
    echo "Notes written: $NOTES"
    exit 0
fi

if ! gh auth status -h github.com &>/dev/null; then
    echo "WARN: gh not authenticated — run: gh auth login"
    echo "Notes written: $NOTES"
    exit 0
fi

git push origin main 2>/dev/null || true

if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Tag $TAG exists — deleting for re-release"
    git tag -d "$TAG" 2>/dev/null || true
    git push origin ":refs/tags/$TAG" 2>/dev/null || true
fi

git tag -a "$TAG" -m "AMOURANTHRTX ${VERSION}"
git push origin "$TAG"

ASSETS=()
if [[ -x "$ENGINE" ]]; then
    TARBALL="$ROOT/build/AMOURANTHRTX-${VERSION}-linux-x86_64.tar.gz"
    tar -czf "$TARBALL" -C "$(dirname "$ENGINE")" "$(basename "$ENGINE")" 2>/dev/null || true
    if [[ -f "$TARBALL" ]]; then
        ASSETS+=("$TARBALL")
    fi
fi

RELEASE_ARGS=(release create "$TAG" --title "AMOURANTHRTX ${VERSION}" --notes-file "$NOTES")
if [[ ${#ASSETS[@]} -gt 0 ]]; then
    for a in "${ASSETS[@]}"; do
        RELEASE_ARGS+=(--attach="$a")
    done
fi

if gh release view "$TAG" &>/dev/null; then
    gh release upload "$TAG" "${ASSETS[@]}" --clobber 2>/dev/null || true
    gh release edit "$TAG" --notes-file "$NOTES"
    echo "OK updated GitHub release $TAG"
else
    gh "${RELEASE_ARGS[@]}"
    echo "OK created GitHub release $TAG"
fi

echo "GREEN ALL publish_github_release ${VERSION}"