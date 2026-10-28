#!/bin/zsh
# tutor mcp web on|off|status: the tutor's tools for claude.ai in the browser.
# Claude connects from Anthropic's servers, so the HTTP tool server (chat/mcp/http.mjs, port 4749) is shared on
# the internet with Tailscale Funnel at this Mac's stable tailnet name, on a path only you know (the token).
# Add the printed URL once in claude.ai: Settings > Connectors > Add custom connector. `off` closes the door.
REPO=${0:A:h:h:h}
PORT=4749 # 4748 is tutor write
TS=/Applications/Tailscale.app/Contents/MacOS/Tailscale
PID=$REPO/.tutor_cache/mcp-http.pid
LOG=$REPO/.tutor_cache/mcp-http.log

up() { lsof -ti tcp:$PORT -sTCP:LISTEN >/dev/null 2>&1 }
url() {
  local host=$($TS status --json 2>/dev/null | python3 -c 'import json,sys; print(json.load(sys.stdin)["Self"]["DNSName"].rstrip("."))' 2>/dev/null)
  [[ -n "$host" ]] && echo "https://$host/mcp/$(cat "$REPO/.tutor_cache/mcp-token")"
}

case "$1" in
  on)
    [[ -x $TS ]] || { echo "Tailscale is not installed (tailscale.com/download)"; exit 1 }
    $TS status >/dev/null 2>&1 || { echo "Tailscale is stopped: open the Tailscale app and connect, then run this again"; exit 1 }
    if ! up; then
      mkdir -p "${LOG:h}"
      (cd "$REPO" && nohup node chat/mcp/http.mjs --port $PORT >"$LOG" 2>&1 & echo $! >"$PID")
      for _ in {1..30}; do up && break; sleep 0.2; done
      up || { echo "the tool server did not start:"; cat "$LOG"; exit 1 }
    fi
    # Funnel must be allowed for the tailnet once; Tailscale then prints a link and waits, which we don't
    out=$($TS funnel --bg $PORT 2>&1 </dev/null &; p=$!; sleep 6; kill $p 2>/dev/null)
    if print -r -- "$out" | grep -q "not enabled"; then
      echo "Funnel is not enabled on your tailnet yet. Open this link once, allow it, then run this again:"
      print -r -- "$out" | grep -o 'https://login.tailscale.com[^ ]*'
      exit 1
    fi
    $TS funnel status 2>/dev/null | grep -q "$PORT" || { echo "Funnel did not start:"; print -r -- "$out"; exit 1 }
    echo "on. In claude.ai: Settings > Connectors > Add custom connector, with this URL (keep it private):"
    url
    ;;
  off)
    [[ -x $TS ]] && $TS funnel --https=443 off >/dev/null 2>&1
    [[ -f $PID ]] && kill $(cat "$PID") 2>/dev/null; rm -f "$PID"
    up && kill $(lsof -ti tcp:$PORT -sTCP:LISTEN) 2>/dev/null
    echo "off: the tools are no longer reachable from the internet (the connector in claude.ai can stay; it just fails until on)"
    ;;
  status|"")
    up && echo "tool server: running on port $PORT" || echo "tool server: not running"
    [[ -x $TS ]] && { $TS funnel status 2>&1 | head -5; }
    [[ -f $REPO/.tutor_cache/mcp-token ]] && echo "url: $(url)"
    ;;
  *) echo "tutor mcp web on|off|status"; exit 1 ;;
esac
