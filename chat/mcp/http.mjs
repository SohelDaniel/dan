// The same tools over HTTP, for claude.ai in the browser: Claude connects from Anthropic's servers, so the port
// is shared on the internet by Tailscale Funnel (`tutor mcp web on`), and the only path that answers carries a
// secret token (.tutor_cache/mcp-token, made once). Streamable HTTP with sessions, as claude.ai expects: an
// initialize starts one, later requests name it in mcp-session-id; a session that is quiet for an hour is dropped.
//   node chat/mcp/http.mjs [--port 4749]
import crypto from 'node:crypto';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import { makeServer } from './server.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const TOKEN_FILE = path.join(ROOT, '.tutor_cache', 'mcp-token');
const argv = process.argv.slice(2);
const port = Number(argv[argv.indexOf('--port') + 1]) || 4749;

export function token() {
  if (!fs.existsSync(TOKEN_FILE)) {
    fs.mkdirSync(path.dirname(TOKEN_FILE), { recursive: true });
    fs.writeFileSync(TOKEN_FILE, crypto.randomBytes(18).toString('base64url') + '\n', { mode: 0o600 });
  }
  return fs.readFileSync(TOKEN_FILE, 'utf8').trim();
}

const body = (req) => new Promise((ok) => { let s = ''; req.on('data', (d) => (s += d)); req.on('end', () => { try { ok(s ? JSON.parse(s) : undefined); } catch { ok(undefined); } }); });

const secret = token();
const sessions = new Map(); // mcp-session-id -> { transport, server, used }
const IDLE = 60 * 60_000;
setInterval(() => {
  for (const [id, s] of sessions) if (Date.now() - s.used > IDLE) { s.transport.close(); s.server.close(); sessions.delete(id); }
}, 60_000).unref();

http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname !== `/mcp/${secret}`) { res.writeHead(404); return res.end(); }
  try {
    const parsed = req.method === 'POST' ? await body(req) : undefined;
    const id = req.headers['mcp-session-id'];
    let s = id && sessions.get(id);
    if (!s) {
      if (id || req.method !== 'POST' || parsed?.method !== 'initialize') { res.writeHead(400, { 'content-type': 'application/json' }); return res.end(JSON.stringify({ jsonrpc: '2.0', error: { code: -32000, message: 'no such session: initialize first' }, id: null })); }
      const transport = new StreamableHTTPServerTransport({
        sessionIdGenerator: () => crypto.randomUUID(),
        onsessioninitialized: (sid) => sessions.set(sid, s),
      });
      const server = makeServer();
      s = { transport, server, used: Date.now() };
      transport.onclose = () => { if (transport.sessionId) sessions.delete(transport.sessionId); };
      await server.connect(transport);
    }
    s.used = Date.now();
    await s.transport.handleRequest(req, res, parsed);
  } catch (e) {
    if (!res.headersSent) { res.writeHead(500, { 'content-type': 'application/json' }); res.end(JSON.stringify({ error: String(e.message || e) })); }
  }
}).listen(port, () => console.log(`tutor mcp over http: port ${port}, path /mcp/<token> (token in ${TOKEN_FILE})`));
