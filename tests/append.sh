#!/bin/sh
# Streamed scenes end exactly like whole ones: every script, fed in chunks of 2, 3 and 5 lines through
# --stdin (as the chat streams Claude's beats), must read back identical to the script played at once.
#   make append
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
ok=0; bad=0
for n in 2 3 5; do
  for f in scripts/*.txt chat/exemplars/*.txt chat/examples/*.txt; do
    b=$(basename "$f" .txt)
    ./tutor "$f" --readback 2>/dev/null | grep -v '^# readback' > "$tmp/one.txt"
    head -4 "$f" > "$tmp/head.txt"
    tail -n +5 "$f" | awk -v n=$n 'NR%n==1 && NR>1 {print "#!chunk"} {print}' |
      ./tutor "$tmp/head.txt" --readback --stdin 2>/dev/null | grep -v '^# readback' > "$tmp/live.txt"
    if cmp -s "$tmp/one.txt" "$tmp/live.txt"; then ok=$((ok + 1)); else bad=$((bad + 1)); echo "DIFFERS: $f in chunks of $n"; fi
  done
done
rm -rf "$tmp"
echo "append equivalence: $ok identical, $bad different"
[ "$bad" -eq 0 ]
