// tutor write: a playground for the Dan language. An editor in the browser, the stage in its window: what you type
// plays as you type it, and a panel shows the engine's check (lints, score) and the readback (what is on stage).
//
//   node tools/write.mjs [file.dan] [--voice] [--port 4748]      (bin/tutor: `tutor write [file.dan]`)
//
// How the stage follows the editor (the chat server's own live mode, `tutor --stdin`):
//   - lines added at the end are appended to the running scene and play live, as Claude's beats do;
//   - an edit earlier in the file reloads the unchanged prefix, fast-forwards it (#!interrupt), then plays the
//     rest live, so the stage shows the state just before the edit and animates what changed;
//   - only whole lines are sent (the line being typed waits for its newline; Cmd+Enter sends it anyway).
// The file on disk is always the editor's full text.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..');
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : undefined; };
const PORT = Number(flag('--port') || 4748);
const voice = argv.includes('--voice');
const FILE = path.resolve(argv.find((a) => !a.startsWith('--') && a !== flag('--port')) || path.join(ROOT, 'playground', 'scratch.dan'));
const PREFIX = path.join(path.dirname(FILE), '.' + path.basename(FILE, '.dan') + '.prefix.dan'); // the unchanged part, for a reload

if (!fs.existsSync(TUTOR)) { console.error(`no ${TUTOR}: run make first`); process.exit(1); }
fs.mkdirSync(path.dirname(FILE), { recursive: true });
if (!fs.existsSync(FILE)) fs.writeFileSync(FILE, STARTER());

// ------------------------------------------------------------ the stage
let stage = null, played = '';
function startStage() {
  const args = [FILE, '--stdin', '--window-pos', '660,60'];
  if (!voice) args.push('--mute');
  stage = spawn(TUTOR, args, { cwd: ROOT, stdio: ['pipe', 'pipe', 'inherit'] });
  let buf = '';
  stage.stdout.on('data', (d) => {
    buf += d;
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i); buf = buf.slice(i + 1);
      if (!line.startsWith('@ev ')) continue;
      const [, kind, ...rest] = line.split(' ');
      if (kind === 'say') { try { broadcast({ t: 'say', text: JSON.parse(rest.slice(1).join(' ')) }); } catch {} }
      else if (kind === 'end') broadcast({ t: 'end' });
      else if (kind === 'idle') broadcast({ t: 'idle' });
    }
  });
  stage.on('exit', () => { stage = null; broadcast({ t: 'stage', alive: false }); });
  played = fs.readFileSync(FILE, 'utf8'); // the stage plays the file from the top
}
const send = (s) => { if (stage) stage.stdin.write(s.endsWith('\n') ? s : s + '\n'); };

// whole lines only: the line being typed is not a line yet
const whole = (text) => (text.endsWith('\n') || !text.includes('\n') ? (text.endsWith('\n') ? text : '') : text.slice(0, text.lastIndexOf('\n') + 1));

function apply(text, force) {
  fs.writeFileSync(FILE, text);
  const complete = force ? (text.endsWith('\n') ? text : text + '\n') : whole(text);
  if (complete === played) return 'none';
  if (complete.startsWith(played)) { // typed at the end: continue the scene
    send(complete.slice(played.length));
    send('#!chunk');
    played = complete;
    return 'append';
  }
  // edited earlier: the unchanged lines are reloaded and finished at once, the rest plays
  const a = played.split('\n'), b = complete.split('\n');
  let i = 0;
  while (i < a.length && i < b.length && a[i] === b[i]) i++;
  const prefix = b.slice(0, i).join('\n') + (i ? '\n' : '');
  fs.writeFileSync(PREFIX, prefix);
  send('#!load ' + PREFIX);
  send('#!interrupt');
  const rest = complete.slice(prefix.length);
  if (rest) send(rest);
  send('#!chunk');
  played = complete;
  return 'reload';
}

// ------------------------------------------------------------ the check: lints, score, readback
let checkTimer = null;
function scheduleCheck() {
  clearTimeout(checkTimer);
  checkTimer = setTimeout(runCheck, 600);
}
function runCheck() {
  const c = spawnSync(TUTOR, [FILE, '--check'], { cwd: ROOT, encoding: 'utf8' });
  const out = (c.stdout || '') + (c.stderr || '');
  const lints = out.split('\n').filter((l) => /^(lint|parse|error):/.test(l));
  const score = (out.match(/layout score: (\d+)/) || [])[1];
  const decodable = /decodable: yes/.test(out);
  const r = spawnSync(TUTOR, [FILE, '--readback'], { cwd: ROOT, encoding: 'utf8' });
  const readback = (r.stdout || '').split('\n').filter((l) => !l.startsWith('# readback')).join('\n').trim();
  broadcast({ t: 'check', ok: c.status === 0, lints, score, decodable, readback });
}

// ------------------------------------------------------------ examples to load
function examples() {
  const list = [];
  for (const d of ['chat/examples', 'scripts']) {
    const dir = path.join(ROOT, d);
    if (fs.existsSync(dir)) for (const f of fs.readdirSync(dir).filter((f) => f.endsWith('.dan')).sort()) list.push({ name: `${d}/${f}`, path: path.join(dir, f) });
  }
  const toolbox = path.join(ROOT, 'chat', 'toolbox.md');
  if (fs.existsSync(toolbox))
    for (const move of fs.readFileSync(toolbox, 'utf8').split(/^### /m).slice(1)) {
      const title = move.split('\n')[0], code = (move.match(/```\n([\s\S]*?)```/) || [])[1];
      if (code) list.push({ name: `toolbox: ${title}`, text: `title "${title}"\n${code}` });
    }
  return list;
}

// ------------------------------------------------------------ the page and its events
const clients = new Set();
function broadcast(e) { const line = `data: ${JSON.stringify(e)}\n\n`; for (const res of clients) res.write(line); }
const json = (res, o, code = 200) => { res.writeHead(code, { 'content-type': 'application/json' }); res.end(JSON.stringify(o)); };
const body = (req) => new Promise((r) => { let s = ''; req.on('data', (d) => (s += d)); req.on('end', () => { try { r(JSON.parse(s || '{}')); } catch { r({}); } }); });

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (req.method === 'GET' && url.pathname === '/') {
    res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
    return res.end(fs.readFileSync(path.join(HERE, 'write.html')));
  }
  if (url.pathname === '/api/events') {
    res.writeHead(200, { 'content-type': 'text/event-stream', 'cache-control': 'no-cache', connection: 'keep-alive' });
    res.write('\n');
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }
  if (url.pathname === '/api/state') return json(res, { file: FILE, text: fs.readFileSync(FILE, 'utf8'), voice, alive: !!stage, examples: examples().map((e) => e.name) });
  const b = await body(req);
  if (url.pathname === '/api/text') { const how = apply(String(b.text ?? ''), !!b.force); scheduleCheck(); return json(res, { how }); }
  if (url.pathname === '/api/load') {
    const e = examples().find((x) => x.name === b.name);
    if (!e) return json(res, { error: 'no such example' }, 404);
    const text = e.text ?? fs.readFileSync(e.path, 'utf8');
    fs.writeFileSync(FILE, text);
    played = text;
    send('#!load ' + FILE);
    scheduleCheck();
    return json(res, { text });
  }
  if (url.pathname === '/api/replay') { played = fs.readFileSync(FILE, 'utf8'); send('#!load ' + FILE); return json(res, { ok: true }); }
  if (url.pathname === '/api/skip') { send('#!interrupt'); return json(res, { ok: true }); }
  if (url.pathname === '/api/stage') { if (!stage) startStage(); return json(res, { alive: !!stage }); }
  json(res, { error: 'not found' }, 404);
});

server.listen(PORT, () => {
  startStage();
  scheduleCheck();
  console.log(`tutor write: http://localhost:${PORT}  (${path.relative(process.cwd(), FILE) || FILE}${voice ? ', voice on' : ', muted'})`);
  if (!argv.includes('--no-open')) spawn('open', [`http://localhost:${PORT}`], { stdio: 'ignore' });
});
process.on('SIGINT', () => { if (stage) stage.kill(); process.exit(0); });
process.on('SIGTERM', () => { if (stage) stage.kill(); process.exit(0); });

function STARTER() {
  return `title "A transfer"
mark alice bar account balance=100 label=Alice unit=$
mark bob   bar account balance=20  label=Bob   unit=$
Say: "Alice has [alice]a hundred dollars and Bob has [bob]twenty."
Say: "She sends him |go|thirty."
@go link pay arrow alice -> bob amount=30
@go morph alice.balance -> 70 1.5s
with morph bob.balance -> 50 1.5s
Say: "Now she has {alice.balance} and he has {bob.balance}: the total never changed."
mark f tex tex="\\textcolor{alice}{70} + \\textcolor{bob}{50} = 120"
`;
}
