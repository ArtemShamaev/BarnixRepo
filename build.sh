#!/bin/sh
# Builds all programs and the repository index (see tools/build.py).
# ./build.sh            everything
# ./build.sh gsnake     only these programs (the index is always rewritten)
# BDK=/path/to/sdk ./build.sh   when Barnix is not in ~/Barnix
set -e
cd "$(dirname "$0")"
exec python3 tools/build.py "$@"
