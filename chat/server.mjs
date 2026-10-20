// tutor chat: the stage is the conversation. You type (on the stage window, or in the browser
// console); Claude answers by speaking and drawing on the stage. The browser shows a live transcript.
// Separate from the engine: it drives `tutor --stdin` and listens to its "@ev" events.
//
//   node chat/server.mjs [--port 4747] [--mute] [--voice Brian] [--model sonnet] [--effort low] [--new] [--no-open]
//
// A turn:
//   Claude (one persistent `claude -p` process per chat, no tools) answers with a hidden ```plan,
//   a ```scene, and optionally a ```memory block. Scene lines go to the stage while Claude is still
//   writing: set-up lines at once, then one beat (a Say and the lines after it) at a time. The engine
//   reports what it says word by word; the server relays that to the browser (SSE) as the transcript.
//   Typing while it speaks interrupts: the stage fast-forwards, and Claude is told what was heard.
//
// Memory, three layers:
//   sessions/<id>/messages.jsonl   the transcript (the page reloads it; survives restarts)
//   sessions/<id>/session.json     Claude's session id, so the conversation resumes with context
//   memory/learner.md              what Claude knows about the learner across chats
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '..');
const TUTOR = path.join(ROOT, 'tutor');
const LIBRARY = path.join(HERE, 'library');
const SESSIONS = path.join(HERE, 'sessions');
const MEMORY_DIR = path.join(HERE, 'memory');
const MEMORY = path.join(MEMORY_DIR, 'learner.md');
const EXEMPLARS = path.join(HERE, 'exemplars');

const argv = process.argv.slice(2);
const flag = (name, def) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : def; };
const PORT = Number(flag('--port', 4747));
let mute = argv.includes('--mute');
const VOICE = flag('--voice', process.env.TUTOR_VOICE || ''); // an Inworld voice id; empty: the engine's default
let defaultModel = flag('--model', 'sonnet');
// low effort: measured the same first-token time with no visible thinking lost, and it keeps turns short
const EFFORT = flag('--effort', 'low');
const PROMPT_FILE = flag('--prompt', path.join(HERE, 'prompt.md')); // for experiments

if (!fs.existsSync(TUTOR)) { console.error(`no ${TUTOR}: run make first`); process.exit(1); }
if (process.env.ANTHROPIC_API_KEY)
  console.warn('warning: ANTHROPIC_API_KEY is set, so claude will bill the API instead of your subscription');
for (const d of [LIBRARY, SESSIONS, MEMORY_DIR]) fs.mkdirSync(d, { recursive: true });
if (!fs.existsSync(MEMORY)) fs.writeFileSync(MEMORY, '# Learner\n\n(Nothing yet.)\n');

// A fixed system prompt (so it caches): the protocol, worked examples, and the language reference.
function buildSystem(promptFile) {
  const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
  let examples = '';
  if (fs.existsSync(EXEMPLARS))
    for (const f of fs.readdirSync(EXEMPLARS).filter((f) => f.endsWith('.txt')).sort())
      examples += `\n### ${f.slice(0, -4)}\n\n\`\`\`scene\n${fs.readFileSync(path.join(EXEMPLARS, f), 'utf8').trim()}\n\`\`\`\n`;
  return fs.readFileSync(promptFile, 'utf8').replace('{{EXAMPLES}}', examples) +
    readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
}
const SYSTEM = buildSystem(PROMPT_FILE);
const libraryNames = () => fs.readdirSync(LIBRARY).filter((f) => f.endsWith('.txt')).map((f) => f.slice(0, -4));

// ------------------------------------------------------------ live events to every open page (SSE)
const clients = new Set();
function broadcast(e) {
  const line = `data: ${JSON.stringify(e)}\n\n`;
  for (const res of clients) res.write(line);
}

// ------------------------------------------------------------ sessions
let S; // { id, dir, sid, turn, title, lints, busy, memSent, pending, cut, note }
const file = (n) => path.join(S.dir, n);
const read = (n) => { try { return fs.readFileSync(file(n), 'utf8'); } catch { return ''; } };
const saveMeta = () => fs.writeFileSync(file('session.json'),
  JSON.stringify({ sid: S.sid, turn: S.turn, title: S.title, lints: S.lints, updated: Date.now() }, null, 1));
const logMsg = (m) => fs.appendFileSync(file('messages.jsonl'), JSON.stringify({ ...m, ts: Date.now() }) + '\n');
const sessionOf = (dir, meta) => ({ id: path.basename(dir), dir, sid: meta.sid || null, turn: meta.turn || 0,
  title: meta.title || '', lints: meta.lints || [], busy: false, memSent: null, pending: null, cut: false, note: '' });

function newSession() {
  const id = new Date().toISOString().replace(/[-:]/g, '').replace('T', '-').slice(0, 15);
  const dir = path.join(SESSIONS, id);
  fs.mkdirSync(path.join(dir, 'turns'), { recursive: true });
  for (const f of ['state.txt', 'turn.txt', 'messages.jsonl']) fs.writeFileSync(path.join(dir, f), '');
  switchTo(sessionOf(dir, {}));
  saveMeta();
  loadStage('title "Ask me anything"\n');
}

// sessions from the first version of this server had no session.json (and no transcript): adopt them
function adopt(dir) {
  if (fs.existsSync(path.join(dir, 'session.json')) || !fs.existsSync(path.join(dir, 'state.txt'))) return;
  const turns = fs.existsSync(path.join(dir, 'turns')) ? fs.readdirSync(path.join(dir, 'turns')).length : 0;
  const title = (fs.readFileSync(path.join(dir, 'state.txt'), 'utf8').match(/^title "([^"]*)"/m) || [])[1] || 'Earlier chat';
  fs.writeFileSync(path.join(dir, 'session.json'),
    JSON.stringify({ sid: null, turn: turns, title, updated: fs.statSync(path.join(dir, 'state.txt')).mtimeMs }, null, 1));
  if (!fs.existsSync(path.join(dir, 'messages.jsonl'))) fs.writeFileSync(path.join(dir, 'messages.jsonl'), '');
}

function openSession(id) {
  const dir = path.join(SESSIONS, path.basename(id));
  adopt(dir);
  if (!fs.existsSync(path.join(dir, 'session.json'))) return false;
  switchTo(sessionOf(dir, JSON.parse(fs.readFileSync(path.join(dir, 'session.json'), 'utf8'))));
  const state = read('state.txt');
  loadStage(state.trim() ? state : 'title "Ask me anything"\n');
  return true;
}

function switchTo(next) {
  stopClaude();
  S = next;
  claude = startClaude(defaultModel); // warm: the first message doesn't wait for start-up
}

function listSessions() {
  for (const d of fs.readdirSync(SESSIONS)) adopt(path.join(SESSIONS, d));
  return fs.readdirSync(SESSIONS).filter((d) => fs.existsSync(path.join(SESSIONS, d, 'session.json'))).map((d) => {
    const m = JSON.parse(fs.readFileSync(path.join(SESSIONS, d, 'session.json'), 'utf8'));
    return { id: d, title: m.title || 'New chat', turns: m.turn || 0, updated: m.updated || 0 };
  }).sort((a, b) => b.updated - a.updated);
}

// ------------------------------------------------------------ the stage (tutor --stdin) and what it says
let stage = null;
const speech = { lines: [], current: null, speaking: false, lastText: '' }; // what the learner has heard this turn
const stageAlive = () => stage && stage.exitCode === null && !stage.killed;
function stageSend(line) {
  if (process.env.TUTOR_DEBUG) console.log('stage<', stageAlive(), line);
  if (stageAlive()) stage.stdin.write(line + '\n');
}
function spawnStage() {
  const args = [file('live.txt'), '--stdin'];
  if (mute) args.push('--mute');
  if (VOICE) args.push('--voice-id', VOICE);
  stage = spawn(TUTOR, args, { cwd: ROOT, stdio: ['pipe', 'pipe', 'pipe'] }); // cwd ROOT: .inworld_key + caches
  stage.stdin.on('error', () => {});
  stage.stderr.on('data', (d) => fs.appendFileSync(file('stage.log'), d)); // parse errors, voice failures
  let buf = '';
  stage.stdout.on('data', (d) => {
    buf += d;
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i); buf = buf.slice(i + 1);
      if (process.env.TUTOR_DEBUG) console.log('stage>', line);
      if (line.startsWith('@ev ')) onStageEvent(line.slice(4));
    }
  });
}
function onStageEvent(ev) {
  const sp = ev.indexOf(' ');
  const kind = sp < 0 ? ev : ev.slice(0, sp), rest = sp < 0 ? '' : ev.slice(sp + 1);
  if (kind === 'say') {
    const sp2 = rest.indexOf(' ');
    const uid = +rest.slice(0, sp2), text = JSON.parse(rest.slice(sp2 + 1));
    speech.current = { uid, text, word: -1 };
    speech.speaking = true;
    speech.lastText = text;
    if (S.turnTimes && S.turnTimes.voice === null) S.turnTimes.voice = S.turnAt();
    stageSend('#!state idle'); // it's talking now
    broadcast({ t: 'say', uid, text });
  } else if (kind === 'word') {
    const [uid, w] = rest.split(' ').map(Number);
    if (speech.current && speech.current.uid === uid) speech.current.word = w;
    broadcast({ t: 'word', uid, i: w });
  } else if (kind === 'end') {
    const uid = +rest;
    if (speech.current && speech.current.uid === uid) { speech.lines.push(speech.current.text); speech.current = null; }
    broadcast({ t: 'end', uid });
  } else if (kind === 'idle') {
    speech.speaking = false;
    // it asked something: the learner's turn
    if (!S.busy && /\?\s*$/.test(speech.lastText)) { stageSend('#!state listening'); broadcast({ t: 'state', v: 'listening' }); }
    broadcast({ t: 'idle' });
  } else if (kind === 'input') {
    submit(JSON.parse(rest), 'stage');
  }
}
// show this text from scratch (session open, replay)
function loadStage(text) {
  fs.writeFileSync(file('live.txt'), text);
  if (stageAlive()) stageSend(`#!load ${file('live.txt')}`);
  else spawnStage();
}
// continue the running stage
function appendStage(text) {
  fs.appendFileSync(file('live.txt'), text);
  if (process.env.TUTOR_DEBUG) console.log('stage<<', stageAlive(), JSON.stringify(text).slice(0, 80));
  if (stageAlive()) stage.stdin.write(text);
  else spawnStage(); // the window was closed: reopen it with everything so far
}
function restartStage() { if (stageAlive()) stage.kill(); stage = null; spawnStage(); }

function readback(text) {
  fs.writeFileSync(file('.fold.txt'), text);
  const rb = spawnSync(TUTOR, [file('.fold.txt'), '--readback'], { encoding: 'utf8', cwd: ROOT }).stdout || '';
  return rb.split('\n').filter((l) => l.trim() && !l.startsWith('#')).join('\n') + '\n'; // the model, no derived comments
}
// the stage as the learner saw it at the end of the turn, rendered offscreen in the background; it goes
// to Claude with the next message, so it sees its own picture (crowding, cut-off text, a bad composition)
function snapshotFrame(text) {
  fs.writeFileSync(file('.frame.txt'), text);
  S.frame = null;
  const p = spawn(TUTOR, [file('.frame.txt'), '--snapshot', file('frame'), '--at', 'end', '--mute'], { cwd: ROOT, stdio: 'ignore' });
  p.on('close', (code) => { if (code === 0 && fs.existsSync(file('frame_end.png'))) S.frame = file('frame_end.png'); });
}

function check(text) {
  fs.writeFileSync(file('.fold.txt'), text);
  const r = spawnSync(TUTOR, [file('.fold.txt'), '--check'], { encoding: 'utf8', cwd: ROOT });
  const out = (r.stdout || '') + (r.stderr || '');
  const lints = [...new Set(out.split('\n').filter((l) => l.startsWith('  ') || l.startsWith('parse:')).map((l) => l.trim()))];
  return { ok: r.status === 0, lints };
}

// ------------------------------------------------------------ Claude: one persistent process per chat
const CLEAN_ENV = Object.fromEntries(Object.entries(process.env)
  .filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));
let claude = null; // { p, model, onEvent, err }

function startClaude(model) {
  const args = ['-p', '--input-format', 'stream-json', '--output-format', 'stream-json', '--verbose',
    '--include-partial-messages', '--tools', '', '--strict-mcp-config', '--model', model, '--effort', EFFORT,
    '--append-system-prompt', SYSTEM];
  if (S.sid) args.push('--resume', S.sid);
  const p = spawn('claude', args, { cwd: S.dir, env: CLEAN_ENV });
  const c = { p, model, onEvent: null, err: '' };
  let buf = '';
  p.stdout.on('data', (d) => {
    buf += d;
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i); buf = buf.slice(i + 1);
      let e; try { e = JSON.parse(line); } catch { continue; }
      if (e.session_id) S.sid = e.session_id;
      c.onEvent?.(e);
    }
  });
  p.stderr.on('data', (d) => { c.err = (c.err + d).slice(-2000); });
  p.stdin.on('error', () => {});
  p.on('close', () => {
    if (claude === c) claude = null;
    c.onEvent?.({ type: 'closed', err: c.err });
  });
  S.memSent = null; // a new process has not seen the memory yet
  return c;
}
function stopClaude() { if (claude) { claude.onEvent = null; claude.p.kill(); claude = null; } }

// ------------------------------------------------------------ input: from the stage or the browser
function heardSoFar() {
  const parts = [...speech.lines];
  if (speech.current) parts.push(speech.current.text.split(' ').slice(0, speech.current.word + 1).join(' ') + ' …');
  return parts.join(' ');
}

function submit(text, source) {
  text = String(text || '').trim();
  if (!text) return;
  broadcast({ t: 'user', text, source });
  stageSend(`#!state thinking ${text.replace(/\n/g, ' ')}`);
  broadcast({ t: 'state', v: 'thinking' });
  if (S.busy) { // still writing: cut it here, answer this next
    S.cut = true;
    S.pending = { text, source };
    S.note = `(The learner interrupted you. They had heard: "${heardSoFar()}". The rest of your last answer was not shown.)`;
    stageSend('#!interrupt');
    return;
  }
  if (speech.speaking) { // finished writing, still talking: cut the talk
    S.note = `(The learner interrupted you. They had heard: "${heardSoFar()}". The rest of that answer was skipped.)`;
    stageSend('#!interrupt');
  }
  runTurn(text, source);
}

function contextFor(text) {
  const parts = [];
  const mem = fs.readFileSync(MEMORY, 'utf8');
  if (mem !== S.memSent) { parts.push(`<memory>\n${mem.trim()}\n</memory>`); S.memSent = mem; }
  const lib = libraryNames();
  if (lib.length) parts.push(`<library>${lib.join(', ')}</library>`);
  const said = text.toLowerCase();
  for (const n of lib)
    if (said.includes(n) || said.includes(n.replace(/-/g, ' ')))
      parts.push(`<library_scene name="${n}">\n${fs.readFileSync(path.join(LIBRARY, n + '.txt'), 'utf8').trim()}\n</library_scene>`);
  if (S.lints.length) parts.push(`<last_scene_issues>\n${S.lints.join('\n')}\n</last_scene_issues>`);
  if (S.note) { parts.push(S.note); S.note = ''; }
  if (/\b(show me (what|everything)|what (you|u) can do|display|tour|demo|showcase|longer|bigger|more (aesthetic|beautiful|visual|detail)|full (display|show))\b/i.test(text))
    parts.push('<request_note>The learner explicitly asked for more: a showcase, longer, bigger or more beautiful. Give exactly that, at showcase scale (several full scenes), not the default length.</request_note>');
  if (S.frame) parts.push('<last_frame> (attached image): the stage as the learner saw it at the end of your last turn. If it is crowded, cut off or hard to read, keep that in mind as you build on it.');
  parts.push(`<stage>\n${read('state.txt').trim() || '(empty)'}\n</stage>`);
  return parts.join('\n') + `\n\nLearner: ${text}`;
}

function runTurn(text, source, model = defaultModel) {
  S.busy = true;
  S.cut = false;
  speech.lines = []; speech.current = null;
  if (!S.title) S.title = text.replace(/\s+/g, ' ').trim().slice(0, 60);
  logMsg({ role: 'user', text, source, frame: !!S.frame }); // frame: the last picture went with it
  const t0 = Date.now();
  S.turnAt = () => +((Date.now() - t0) / 1000).toFixed(1);
  const times = S.turnTimes = { first: null, stage: null, voice: null, done: null }; // seconds after send
  const msg = { role: 'assistant', spoken: [], plan: '', memory: false, stage: null, meta: null, error: null, cut: false };

  // ---- the reply stream: a fence parser (plan / scene / memory), then beats to the stage
  const stateBefore = read('state.txt');
  fs.writeFileSync(file('turn.txt'), '');
  fs.appendFileSync(file('live.txt'), `# --- turn ${S.turn + 1} ---\n`);
  let mode = 'none', partial = '', memText = '', beat = [], seenSay = false;
  const toStage = (lines) => {
    if (!lines.length || S.cut) return; // after an interrupt, nothing more is shown
    if (times.stage === null) { times.stage = S.turnAt(); broadcast({ t: 'drawing' }); }
    const chunk = lines.join('\n') + '\n';
    fs.appendFileSync(file('turn.txt'), chunk); // the record is what was actually shown
    for (const l of lines) if (l.trim().startsWith('Say:')) msg.spoken.push(l.trim().slice(4).trim().replace(/^"|"$/g, '').replace(/\|[^|\s]*\|/g, ''));
    appendStage(chunk);
  };
  const sceneLine = (line) => {
    if (line.trim().startsWith('Say:')) {
      toStage(beat); // the previous beat is complete
      beat = [line];
      seenSay = true;
    } else if (!seenSay) toStage([line]); // set-up before any speech: show it now
    else beat.push(line);
  };
  const endScene = () => { toStage(beat); beat = []; };
  const fullLine = (line) => {
    const t = line.trim();
    if (mode === 'none') {
      const m = t.match(/^```\s*(scene|memory|plan)\b/);
      if (m) mode = m[1];
      return; // anything outside the blocks isn't shown: the stage is the only voice
    }
    if (t.startsWith('```')) { if (mode === 'scene') endScene(); mode = 'none'; return; }
    if (mode === 'scene') sceneLine(line);
    else if (mode === 'memory') memText += line + '\n';
    else if (mode === 'plan') msg.plan += line + '\n';
  };
  const onText = (v) => {
    if (times.first === null) times.first = S.turnAt();
    for (const ch of v.split(/(\n)/)) {
      if (ch === '\n') { fullLine(partial); partial = ''; continue; }
      partial += ch;
    }
  };

  const finish = (error) => {
    if (partial) { fullLine(partial); partial = ''; }
    if (mode === 'scene') endScene();
    if (error) { msg.error = error; broadcast({ t: 'error', v: error }); }
    times.done = S.turnAt();
    msg.cut = S.cut;
    const turn = read('turn.txt');
    if (turn.trim()) {
      S.turn++;
      fs.writeFileSync(path.join(S.dir, 'turns', String(S.turn).padStart(3, '0') + '.txt'), turn);
      const live = stateBefore + (stateBefore.endsWith('\n') || !stateBefore ? '' : '\n') + turn;
      const c = check(live);
      S.lints = c.lints;
      fs.writeFileSync(file('state.txt'), readback(live));
      snapshotFrame(live);
      msg.stage = { ok: c.ok, lints: c.lints, turn, state: read('state.txt') };
      broadcast({ t: 'stage', ...msg.stage });
    } else S.lints = [];
    if (memText.trim()) {
      fs.writeFileSync(MEMORY, memText.trim() + '\n');
      msg.memory = true;
      broadcast({ t: 'memory' });
    }
    msg.meta = { ...times };
    broadcast({ t: 'meta', ...times, plan: msg.plan.trim() });
    logMsg(msg);
    saveMeta();
    S.busy = false;
    S.turnTimes = null;
    if (S.pending) { const p = S.pending; S.pending = null; runTurn(p.text, p.source); }
  };

  if (!claude || claude.model !== model) { stopClaude(); claude = startClaude(model); }
  claude.onEvent = (e) => {
    if (e.type === 'stream_event') {
      const ev = e.event;
      if (ev?.type === 'content_block_delta' && ev.delta?.type === 'text_delta') onText(ev.delta.text);
    } else if (e.type === 'result') {
      claude.onEvent = null;
      finish(e.is_error ? String(e.result || e.subtype) : null);
    } else if (e.type === 'closed') {
      finish('Claude stopped: ' + (e.err.trim().split('\n').pop() || 'process exited'));
    }
  };
  const content = [{ type: 'text', text: contextFor(text) }];
  if (S.frame) { // what the learner last saw, as an image
    try { content.push({ type: 'image', source: { type: 'base64', media_type: 'image/png', data: fs.readFileSync(S.frame).toString('base64') } }); } catch {}
    S.frame = null;
  }
  claude.p.stdin.write(JSON.stringify({ type: 'user', message: { role: 'user', content } }) + '\n');
}

// ------------------------------------------------------------ http
const json = (res, obj, code = 200) => { res.writeHead(code, { 'content-type': 'application/json' }); res.end(JSON.stringify(obj)); };
const body = (req) => new Promise((r) => { let s = ''; req.on('data', (d) => (s += d)); req.on('end', () => { try { r(JSON.parse(s || '{}')); } catch { r({}); } }); });
const slug = (s) => String(s || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 40);
const messages = () => read('messages.jsonl').split('\n').filter(Boolean).map((l) => { try { return JSON.parse(l); } catch { return null; } }).filter(Boolean);
const turnFiles = () => fs.readdirSync(path.join(S.dir, 'turns')).sort().map((f) => fs.readFileSync(path.join(S.dir, 'turns', f), 'utf8'));

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (req.method === 'GET' && url.pathname === '/') {
    res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
    return res.end(fs.readFileSync(path.join(HERE, 'index.html')));
  }
  if (url.pathname === '/api/events') {
    res.writeHead(200, { 'content-type': 'text/event-stream', 'cache-control': 'no-cache', connection: 'keep-alive' });
    res.write(': hello\n\n');
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }
  if (url.pathname === '/api/info') {
    return json(res, { session: S.id, title: S.title, state: read('state.txt'), turn: S.turn, mute, model: defaultModel,
      busy: S.busy, messages: messages(), sessions: listSessions(), library: libraryNames() });
  }
  if (req.method === 'GET' && url.pathname === '/api/memory') return json(res, { text: fs.readFileSync(MEMORY, 'utf8'), path: MEMORY });
  if (req.method !== 'POST') return json(res, { error: 'not found' }, 404);
  const b = await body(req);

  if (url.pathname === '/api/send') {
    if (b.model) defaultModel = b.model;
    submit(b.text, 'browser');
    return json(res, { ok: true });
  }
  if (S.busy) return json(res, { error: 'busy' }, 409);
  if (url.pathname === '/api/replay-turn') {
    // the stage as it was before the last turn, then the last turn plays again
    const turns = turnFiles();
    loadStage(turns.length ? readback(turns.slice(0, -1).join('\n')) + turns.at(-1) : read('state.txt'));
    return json(res, { ok: true });
  }
  if (url.pathname === '/api/replay-all') {
    // the turns are deltas from an empty stage, so in order they are the whole lesson
    const all = turnFiles().map((t, i) => `# --- turn ${i + 1} ---\n` + t).join('\n');
    fs.writeFileSync(file('lesson.txt'), all);
    loadStage(all || read('state.txt'));
    return json(res, { ok: true, path: file('lesson.txt') });
  }
  if (url.pathname === '/api/new') { newSession(); return json(res, { ok: true }); }
  if (url.pathname === '/api/open') return json(res, { ok: openSession(String(b.id || '')) });
  if (url.pathname === '/api/voice') {
    mute = !b.on;
    fs.writeFileSync(file('live.txt'), read('state.txt'));
    restartStage();
    return json(res, { mute });
  }
  if (url.pathname === '/api/memory') { fs.writeFileSync(MEMORY, String(b.text ?? '')); return json(res, { ok: true }); }
  if (url.pathname === '/api/save') {
    const name = slug(b.name);
    if (!name || !read('state.txt').trim()) return json(res, { error: 'nothing to save' }, 400);
    fs.writeFileSync(path.join(LIBRARY, name + '.txt'), read('state.txt'));
    return json(res, { ok: true, name });
  }
  json(res, { error: 'not found' }, 404);
});

// pick up where we left off, unless asked for a fresh start
const recent = listSessions()[0];
if (argv.includes('--new') || !recent || !openSession(recent.id)) newSession();
server.listen(PORT, '127.0.0.1', () => {
  console.log(`tutor: http://localhost:${PORT}   (session ${S.id}${S.title ? ': ' + S.title : ''})`);
  console.log('type on the stage window, or in the browser console');
  if (!argv.includes('--no-open')) spawn('open', [`http://localhost:${PORT}`], { stdio: 'ignore' });
});
const quit = () => { stopClaude(); if (stageAlive()) stage.kill(); process.exit(0); };
process.on('SIGINT', quit);
process.on('SIGTERM', quit);
