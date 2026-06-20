#!/usr/bin/env bash
# Wire Grok + GitHub Copilot MCP + gh CLI using your browser session.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== Grok (xAI) =="
if command -v grok >/dev/null 2>&1; then
  grok login --oauth || grok login
  grok mcp doctor grok_com_github || true
else
  echo "grok CLI not on PATH — skip (IDE session may already be authed)"
fi

echo "== GitHub CLI (browser) =="
if ! command -v gh >/dev/null 2>&1; then
  echo "Install gh: https://cli.github.com/"
  exit 1
fi
if ! gh auth status -h github.com &>/dev/null; then
  gh auth login -h github.com -p https -w --skip-ssh-key
fi
gh auth setup-git -h github.com
export GH_TOKEN="$(gh auth token -h github.com)"
export GITHUB_TOKEN="$GH_TOKEN"
echo "GH_TOKEN exported for this shell ($(printf '%s' "$GH_TOKEN" | wc -c) chars)"

echo "== Git remote =="
if [[ -d .git ]]; then
  git remote -v
  git status -sb
else
  echo "No .git yet — github_push.sh will init on first push"
fi

echo "== Smoke test =="
gh api user --jq '.login'
gh api repos/ZacharyGeurts/AMOURANTHRTX --jq '.full_name + " private=" + (.private|tostring)'

echo
echo "Partner stack ready. Sync with:"
echo "  python3 scripts/github_sync_batches.py"
echo "  python3 scripts/github_push_batches.py 0 27"
echo "  ./github_push.sh \"Sync AMOURANTHRTX\""