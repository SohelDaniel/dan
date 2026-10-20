#!/bin/sh
# Your chats as an older commit drew them, next to how HEAD draws them (local only: sessions stay private).
#   tools/compare-commit.sh COMMIT [OUT_DIR] [extra audit flags, e.g. --last 13]
# Builds COMMIT in a throwaway git worktree, renders every turn's end frame with both engines and writes
#   OUT_DIR/before.png (COMMIT), OUT_DIR/after.png (HEAD's ./tutor) and the two audit tables.
set -e
commit=${1:?usage: tools/compare-commit.sh COMMIT [OUT_DIR] [audit flags]}
out=${2:-${TMPDIR:-/tmp}/tutor-compare}
shift; [ $# -gt 0 ] && shift
root=$(cd "$(dirname "$0")/.." && pwd)
wt="$out/worktree"
mkdir -p "$out"
git -C "$root" worktree add --detach "$wt" "$commit" >/dev/null
trap 'git -C "$root" worktree remove --force "$wt"' EXIT
make -C "$wt" tutor >/dev/null
node "$root/tools/audit-sessions.mjs" --tutor "$wt/tutor" --sheet "$out/before.png" "$@" > "$out/before.txt"
node "$root/tools/audit-sessions.mjs" --sheet "$out/after.png" "$@" > "$out/after.txt"
echo "before ($commit): $out/before.png  $out/before.txt"
echo "after (HEAD):     $out/after.png  $out/after.txt"
