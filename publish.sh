#!/usr/bin/env bash
# Builds every program and uploads BarnixRepo to
# github.com/ArtemShamaev/BarnixRepo, where brm finds it by default.
# Once, before the first run: create an empty public repository named
# BarnixRepo on github.com (no README, no license).
# Usage: ./publish.sh ["what changed"]
set -euo pipefail
cd "$(dirname "$0")"
message=${1:-Update BarnixRepo}
url=https://github.com/ArtemShamaev/BarnixRepo.git

./build.sh
[[ -d .git ]] || git init -q -b main
git remote get-url origin >/dev/null 2>&1 || git remote add origin "$url"
git add -A
if git diff --cached --quiet; then
    echo "Nothing changed since the last upload."
else
    git commit -q -m "$message"
fi
git push -u origin main
echo
echo "Done. In Barnix: sudo brm -Sy, then sudo brm -S gsnake (or brm -Ss to see everything)."
