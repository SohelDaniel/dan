// The same tools over HTTP, for claude.ai in the browser: Claude connects from Anthropic's servers, so the port
// is shared on the internet by Tailscale Funnel (`tutor mcp web on`), and the only path that answers carries a
// secret token (.tutor_cache/mcp-token, made once). Streamable HTTP, stateless: one transport per request.
//   node chat/mcp/http.mjs [--port 4748]
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
const port = Number(argv[argv.indexOf('--port') + 1]) || 4748;

export function token() {
  if (!fs.existsSync(TOKEN_FILE)) {
    fs.mkdirSync(path.dirname(TOKEN_FILE), { recursive: true });
    fs.writeFileSync(TOKEN_FILE, crypto.randomBytes(18).toString('base64url') + '\n', { mode: 0o600 });
  }
  return fs.readFileSync(TOKEN_FILE, 'utf8').trim();
}

const body = (req) => new Promise((ok) => { let s = ''; req.on('data', (d) => (s += d)); req.on('end', () => { try { ok(s ? JSON.parse(s) : undefined); } catch { ok(undefined); } }); });

const secret = token();
http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname !== `/mcp/${secret}`) { res.writeHead(404); return res.end(); }
  try {
    const transport = new StreamableHTTPServerTransport({ sessionIdGenerator: undefined });
    const server = makeServer();
    res.on('close', () => { transport.close(); server.close(); });
    await server.connect(transport);
    await transport.handleRequest(req, res, req.method === 'POST' ? await body(req) : undefined);
  } catch (e) {
    if (!res.headersSent) { res.writeHead(500, { 'content-type': 'application/json' }); res.end(JSON.stringify({ error: String(e.message || e) })); }
  }
}).listen(port, () => console.log(`tutor mcp over http: port ${port}, path /mcp/<token> (token in ${TOKEN_FILE})`));
