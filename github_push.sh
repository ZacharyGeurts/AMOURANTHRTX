#!/usr/bin/env bash
# Push AMOURANTHRTX to GitHub (https://github.com/ZacharyGeurts/AMOURANTHRTX)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

REMOTE="${GITHUB_REMOTE:-origin}"
BRANCH="${GITHUB_BRANCH:-main}"
URL="${GITHUB_URL:-https://github.com/ZacharyGeurts/AMOURANTHRTX.git}"

if [[ ! -d .git ]]; then
  echo "Initializing git repository..."
  git init -b "$BRANCH"
  git remote add "$REMOTE" "$URL" 2>/dev/null || git remote set-url "$REMOTE" "$URL"
fi

echo "Staging changes..."
git add -A
if git diff --cached --quiet; then
  echo "Nothing to commit."
else
  git commit -m "${1:-AMOURANTHRTX: SG/AmmoOS layout, single CANVAS shader, data assets}"
fi

echo "Pushing to $REMOTE/$BRANCH ..."
git push -u "$REMOTE" "$BRANCH"