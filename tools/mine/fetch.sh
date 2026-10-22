#!/bin/sh
# 3Blue1Brown's scene code, pinned so the mined library can be rebuilt exactly.
#   sh tools/mine/fetch.sh
set -e
PIN=306a1346a356c7a7b097875ceb51c5cd67d0a56c   # 2026-09-29
cd "$(dirname "$0")"
mkdir -p work
[ -d work/videos/.git ] || git clone -q https://github.com/3b1b/videos.git work/videos
git -C work/videos fetch -q origin "$PIN" 2>/dev/null || true
git -C work/videos checkout -q "$PIN"
echo "3b1b/videos at $(git -C work/videos rev-parse --short HEAD)"
