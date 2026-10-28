// The tutor's MCP tools (chat/mcp/server.mjs) over a stub of the tutor's HTTP API: each tool answers, show returns
// as soon as the stage speaks, a brief is sent as a handoff, lectures are listed.
//   node tests/mcp.mjs        (cd chat/mcp && npm install once)
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SDK = path.join(ROOT, 'chat/mcp/node_modules/@modelcontextprotocol/sdk/dist/esm');
const { Client } = await import(path.join(SDK, 'client/index.js'));
const { StdioClientTransport } = await import(path.join(SDK, 'client/stdio.js'));

let failed = 0;
const check = (name, ok, got) => { if (!ok) failed++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok ? '' : '  got ' + JSON.stringify(got)}`); };

// a tutor that answers the routes the tools use; a send is "spoken" on the event stream a moment later
const got = { sends: [], explains: [] };
const clients = new Set();
const emit = (e) => { for (const c of clients) c.write(`data: ${JSON.stringify(e)}\n\n`); };
const stub = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const json = (o) => { res.writeHead(200, { 'content-type': 'application/json' }); res.end(JSON.stringify(o)); };
  let body = '';
  req.on('data', (d) => (body += d));
  req.on('end', () => {
    const b = body ? JSON.parse(body) : {};
    if (url.pathname === '/api/events') {
      res.writeHead(200, { 'content-type': 'text/event-stream' }); res.write(': hello\n\n'); clients.add(res); res.on('close', () => clients.delete(res));
    } else if (url.pathname === '/api/info') json({ session: 'stub-chat', busy: false, state: 'mark a dot v=1\n', lints: ['layout: a overlaps b'] });
    else if (url.pathname === '/api/send') { got.sends.push(b); json({ ok: true }); setTimeout(() => { emit({ t: 'say', uid: 1, text: 'First words.' }); emit({ t: 'say', uid: 2, text: 'More.' }); emit({ t: 'idle' }); }, 200); }
    else if (url.pathname === '/api/explain') { got.explains.push(b); json({ ok: true }); setTimeout(() => emit({ t: 'explain', state: 'playing', q: b.text, spoken: ['Because six.'] }), 200); }
    else if (url.pathname === '/api/lectures') json({ lectures: [{ id: '20261027-x', status: 'packaged', sources: [{ name: 'a.pdf' }], parts: [{}], notes: true, played: true, job: { running: false, log: '' }, target: 20,
      outline: { title: 'T', level: 'new', parts: [{ title: 'P1', minutes: 3, goal: 'g', check: { question: 'q?' } }] } }] });
    else if (url.pathname === '/api/lecture/play') json({ ok: true });
    else json({ error: 'not found' });
  });
});
await new Promise((r) => stub.listen(0, r));
const port = stub.address().port;

const client = new Client({ name: 'test', version: '0' });
await client.connect(new StdioClientTransport({ command: process.execPath, args: [path.join(ROOT, 'chat/mcp/server.mjs')], env: { ...process.env, TUTOR_PORT: String(port) } }));
const call = async (name, args = {}) => (await client.callTool({ name, arguments: args })).content[0].text;

const { tools } = await client.listTools();
check('eight tools', tools.length === 8 && tools.some((t) => t.name === 'show'), tools.map((t) => t.name));
let t0 = Date.now();
let r = await call('show', { brief: 'Explain x.' });
check('show sends the brief as a handoff', got.sends[0]?.text === 'Explain x.' && got.sends[0]?.handoff === true, got.sends[0]);
check('show returns once the stage speaks, with the first words', r.includes('explaining it on the stage now') && r.includes('First words.') && Date.now() - t0 < 5000, r);
r = await call('ask', { question: 'why 6?' });
check('ask returns the side answer', got.explains[0]?.text === 'why 6?' && r.includes('Because six.'), r);
r = await call('stage');
check('stage shows the scene and the chat', r.includes('mark a dot v=1') && r.includes('stub-chat'), r);
r = await call('lecture_status');
check('lecture_status lists the newest lecture with its outline', r.includes('20261027-x') && r.includes('1. P1 [3 min]'), r);
r = await call('lecture_play');
check('lecture_play plays it', r.includes('Playing 20261027-x'), r);

await client.close();
stub.close();
process.exit(failed ? 1 : 0);
