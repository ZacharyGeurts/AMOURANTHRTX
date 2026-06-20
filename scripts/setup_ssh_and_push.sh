#!/usr/bin/env bash
# Generate SSH key, add to GitHub, open Firefox fallback page, rebase + push main.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== SSH =="
mkdir -p ~/.ssh && chmod 700 ~/.ssh
if [[ ! -f ~/.ssh/id_ed25519 ]]; then
  ssh-keygen -t ed25519 -C "gzac5314@gmail.com" -f ~/.ssh/id_ed25519 -N ""
fi
chmod 600 ~/.ssh/id_ed25519
chmod 644 ~/.ssh/id_ed25519.pub

echo ""
echo "Public key (paste at https://github.com/settings/ssh/new if gh upload fails):"
cat ~/.ssh/id_ed25519.pub

if command -v gh >/dev/null 2>&1 && gh auth status -h github.com &>/dev/null; then
  echo ""
  echo "== Upload key via gh =="
  gh ssh-key add ~/.ssh/id_ed25519.pub -t "AMOURANTHRTX Linux" 2>/dev/null || true
fi

if command -v firefox >/dev/null 2>&1; then
  firefox "https://github.com/settings/ssh/new" >/dev/null 2>&1 &
elif command -v xdg-open >/dev/null 2>&1; then
  xdg-open "https://github.com/settings/ssh/new" >/dev/null 2>&1 &
fi

echo ""
echo "== Git =="
git remote set-url origin git@github.com:ZacharyGeurts/AMOURANTHRTX.git
git fetch origin
git status -sb

if ! git diff --cached --quiet 2>/dev/null; then
  git commit -m "Add read-only Copilot policy; sync local updates" || true
fi

if ! git pull --rebase origin main; then
  echo "Rebase conflict — keeping local code, remote copilot policy files"
  git checkout --theirs .github/copilot-instructions.md .github/instructions/readonly.instructions.md AGENTS.md 2>/dev/null || true
  git add -A
  git rebase --continue || { git rebase --abort; exit 1; }
fi

ssh -o StrictHostKeyChecking=accept-new -T git@github.com || true
git push origin main

echo ""
echo "Done. HEAD=$(git rev-parse HEAD) origin/main=$(git rev-parse origin/main)"