// The tutor as tools for any Claude chat (Claude Code, the Claude desktop app): an MCP server over stdio.
// Thin on purpose: it talks HTTP to the running tutor (chat/server.mjs, started here if it isn't), and the
// tutor's own Claude writes every scene with the full prompt, examples and toolbox. The chat's Claude only
// writes a brief. Registered by `tutor mcp install`.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { z } from 'zod';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const PORT = Number(process.env.TUTOR_PORT || 4747);
const BASE = `http://localhost:${PORT}`;
const NOTES_DIR = path.join(os.homedir(), 'Documents', 'Tutor notes');

const text = (s) => ({ content: [{ type: 'text', text: String(s) }] });
const getJson = (p) => fetch(BASE + p).then((r) => r.json());
const postJson = (p, b) => fetch(BASE + p, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(b || {}) }).then((r) => r.json());
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// The tutor, running: started in the background when it isn't (the engine must be built: `tutor` once by hand).
async function ensureTutor() {
  try { return await getJson('/api/info'); } catch {}
  if (!fs.existsSync(path.join(ROOT, 'tutor'))) throw new Error('the engine is not built: run `tutor` once in a terminal');
  const log = path.join(ROOT, '.tutor_cache', 'server.log');
  fs.mkdirSync(path.dirname(log), { recursive: true });
  const out = fs.openSync(log, 'a');
  spawn(process.execPath, [path.join(ROOT, 'chat', 'server.mjs'), '--no-open'], { cwd: ROOT, detached: true, stdio: ['ignore', out, out] }).unref();
  for (let i = 0; i < 100; i++) {
    await sleep(300);
    try { return await getJson('/api/info'); } catch {}
  }
  throw new Error(`the tutor did not start (see ${log})`);
}

// Follows the tutor's event stream and collects what it says: until the turn is spoken (idle), or, with
// firstOnly, until it has started (clients time a tool call out after a minute; the stage keeps going).
async function spokenUntilIdle({ timeoutMs = 6 * 60_000, firstOnly = false } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  const said = []; let error = '';
  try {
    const res = await fetch(BASE + '/api/events', { signal: ctrl.signal });
    const reader = res.body.getReader();
    const dec = new TextDecoder();
    let buf = '', started = false;
    outer: for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += dec.decode(value, { stream: true });
      let i;
      while ((i = buf.indexOf('\n\n')) >= 0) {
        const chunk = buf.slice(0, i); buf = buf.slice(i + 2);
        const line = chunk.split('\n').find((l) => l.startsWith('data:'));
        if (!line) continue;
        let e; try { e = JSON.parse(line.slice(5)); } catch { continue; }
        if (e.t === 'say') { started = true; said.push(e.text); if (firstOnly) break outer; }
        else if (e.t === 'error') { error = e.text || e.message || 'error'; break outer; }
        else if (e.t === 'idle' && started) break outer;
      }
    }
  } catch (e) { if (e.name !== 'AbortError') throw e; }
  finally { clearTimeout(timer); ctrl.abort(); }
  return { said, error };
}

// Files for the tutor, in the shape the browser sends: name, type, base64. Text files go as text/plain.
const MIME = { '.pdf': 'application/pdf', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif', '.webp': 'image/webp' };
function fileBlocks(paths = []) {
  return paths.map((p) => {
    const f = path.resolve(p);
    if (!fs.existsSync(f)) throw new Error(`no such file: ${p}`);
    return { name: path.basename(f), type: MIME[path.extname(f).toLowerCase()] || 'text/plain', data: fs.readFileSync(f).toString('base64') };
  });
}

// The tools, on a fresh McpServer: the stdio entry below and the HTTP server (http.mjs) both use it.
export function makeServer() {
const server = new McpServer({ name: 'tutor', version: '1.0.0' });

const BRIEF = `The brief is what the tutor is told, as if the learner asked it: write it in the learner's words, from their side.
Put in it: the question or thing to explain; what the conversation already established (facts, notation, examples
used, where the learner got stuck); the learner's level; what to show first if you have a view. Plain prose, 3-10
sentences. Never markup, drawing instructions, colours or positions: the tutor decides how to draw.`;

server.registerTool('show', {
  title: 'Explain on the stage',
  description: `Hand the current topic to the visual tutor: it explains it aloud while drawing on its stage window (a 3Blue1Brown-style animated explanation), using its own prompt and examples. Use when the learner asks to see something, to "use the engine/tutor/stage", or when a picture would teach better than text. Returns as soon as the tutor starts speaking (under a minute), with its first words; the explanation goes on by itself for a few minutes. Then tell the learner to look at the stage window; the stage tool later shows what was drawn. The learner can keep asking there, or here.\n\n${BRIEF}`,
  inputSchema: {
    brief: z.string().describe('what to explain, as the learner would ask it, with the context from this conversation'),
    files: z.array(z.string()).optional().describe('local paths of files to show the tutor with the brief (PDF, images, text, code)'),
    new_chat: z.boolean().optional().describe('start a fresh stage and tutor conversation first (default: continue the current one)'),
  },
}, async ({ brief, files, new_chat }) => {
  await ensureTutor();
  if (new_chat) await postJson('/api/new');
  const waiting = spokenUntilIdle({ firstOnly: true, timeoutMs: 50_000 });
  await sleep(100);
  const r = await postJson('/api/send', { text: brief, handoff: true, files: fileBlocks(files) });
  if (r.error) return text(`the tutor refused: ${r.error}`);
  const { said, error } = await waiting;
  const info = await getJson('/api/info');
  return text([error ? `The tutor stopped with an error: ${error}` : said.length ? 'The tutor is explaining it on the stage now.' : 'The tutor is still thinking; it will start on the stage shortly.',
    said.length ? `It began: "${said[0]}"` : '', `Chat: ${info.session}`].filter(Boolean).join('\n\n'));
});

server.registerTool('ask', {
  title: 'Ask about the paused stage',
  description: 'A side question about what is on the stage right now ("why is that 6?"): the tutor pauses, answers in a second window from that exact moment, and the lesson stays where it was. Returns the answer\'s words.',
  inputSchema: { question: z.string() },
}, async ({ question }) => {
  await ensureTutor();
  const ctrl = new AbortController();
  const res = await fetch(BASE + '/api/events', { signal: ctrl.signal });
  await postJson('/api/explain', { text: question });
  const reader = res.body.getReader(); const dec = new TextDecoder();
  let buf = '', spoken = null, state = '';
  const timer = setTimeout(() => ctrl.abort(), 4 * 60_000);
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += dec.decode(value, { stream: true });
      let i;
      while ((i = buf.indexOf('\n\n')) >= 0) {
        const chunk = buf.slice(0, i); buf = buf.slice(i + 2);
        const line = chunk.split('\n').find((l) => l.startsWith('data:'));
        let e; try { e = JSON.parse(line?.slice(5) || ''); } catch { continue; }
        if (e.t === 'explain' && (e.state === 'playing' || e.state === 'failed')) { spoken = e.spoken || []; state = e.state; break; }
      }
      if (state) break;
    }
  } catch (e) { if (e.name !== 'AbortError') throw e; }
  finally { clearTimeout(timer); ctrl.abort(); }
  return text(state === 'failed' ? 'The tutor could not answer that.' : spoken ? `The answer plays beside the lesson. It says:\n${spoken.join('\n')}` : 'No answer arrived in time.');
});

server.registerTool('stage', {
  title: 'What is on the stage',
  description: 'The tutor\'s current scene as its markup (what is drawn now), the chat id, and whether it is busy. Use to see what the learner is looking at before adding to it.',
  inputSchema: {},
}, async () => {
  const info = await ensureTutor();
  const frame = path.join(ROOT, 'chat', 'sessions', info.session, 'frame_end.png');
  return text(`Chat ${info.session} (${info.busy ? 'busy' : 'idle'}).\n\nStage:\n${info.state?.trim() || '(empty)'}\n\n${fs.existsSync(frame) ? `Last frame: ${frame}` : ''}`);
});

server.registerTool('notes', {
  title: 'Notes to PDF',
  description: 'Turn markdown into a printed PDF to follow on paper: LaTeX math ($...$, $$...$$), mermaid diagrams (```mermaid), tables, code. Saved in ~/Documents/Tutor notes/ and opened. Write the notes yourself (summary, definitions, worked examples, exercises with answers), then call this.',
  inputSchema: { markdown: z.string(), title: z.string().optional().describe('file name; else the first heading') },
}, async ({ markdown, title }) => {
  const { notesPdf } = await import(path.join(ROOT, 'tools', 'notes', 'notes-pdf.mjs'));
  fs.mkdirSync(NOTES_DIR, { recursive: true });
  const name = (title || (markdown.match(/^#\s+(.+)$/m) || [])[1] || 'notes').replace(/[^\w .-]+/g, '_').slice(0, 80);
  const md = path.join(NOTES_DIR, name + '.md'), pdf = path.join(NOTES_DIR, name + '.pdf');
  fs.writeFileSync(md, markdown);
  const { problems } = await notesPdf(md, pdf);
  spawn('open', [pdf], { stdio: 'ignore' }).unref();
  return text(`Saved and opened ${pdf}${problems.length ? `\nPrinting problems (fix and call again): ${problems.join('; ')}` : ''}`);
});

// ---- lectures: the long form, prepared in the background (chat/lecture/)
const lectureLine = (l) => {
  const o = l.outline;
  const parts = o ? `${o.parts.length} parts, ${o.parts.reduce((n, p) => n + p.minutes, 0)} min planned, ${l.parts.filter(Boolean).length} built` : `${l.target} min target`;
  return `${l.id}  [${l.job?.running ? 'working: ' + l.status : l.status}]  ${l.sources.map((s) => s.name).join(', ')}  ${parts}${l.notes ? ', notes ready' : ''}${l.error ? `  error: ${l.error}` : ''}`;
};
const outlineText = (o) => [`${o.title}  (level ${o.level})`, o.note ? `note: ${o.note}` : '',
  ...o.parts.map((p, i) => `${i + 1}. ${p.title} [${p.minutes} min]: ${p.goal}\n   check: ${p.check?.question}`)].filter(Boolean).join('\n');
async function lectureById(id) {
  const { lectures } = await getJson('/api/lectures');
  return (id ? lectures.find((l) => l.id === id || l.id.includes(id)) : lectures[0]) || null;
}

server.registerTool('lecture', {
  title: 'Prepare a lecture',
  description: 'Start a long lecture (15 min to hours) prepared from files in the background: the tutor plans it in parts of a few minutes with a question closing each, then builds, reviews and fixes each part, fetches the voice, and writes notes to print. Planning takes ~3 minutes and this returns the outline for the learner to approve; building takes ~4-7 minutes per lecture minute. Give files as local paths; put what was discussed and what to focus on in `ask`.',
  inputSchema: {
    files: z.array(z.string()).min(1).describe('local paths: PDFs, markdown, JSON, code, images'),
    ask: z.string().optional().describe('who it is for, what to focus on or skip, what this conversation established'),
    minutes: z.number().optional().describe('target length; else suggested from the material'),
  },
}, async ({ files, ask, minutes }) => {
  await ensureTutor();
  const r = await postJson('/api/lecture/new', { files: fileBlocks(files), ask, minutes });
  if (r.error) return text(`could not start: ${r.error}`);
  for (let i = 0; i < 60; i++) { // the plan: one call, about 3 minutes
    await sleep(5000);
    const l = await lectureById(r.id);
    if (l?.outline) return text(`Planned (${l.id}). Show this outline to the learner; when they approve, call lecture_build.\n\n${outlineText(l.outline)}`);
    if (l && !l.job?.running) return text(`Planning stopped: ${l.error || l.status}`);
  }
  return text(`Still planning (${r.id}); call lecture_status later.`);
});

server.registerTool('lecture_status', {
  title: 'Lecture status',
  description: 'Where a lecture stands (newest if no id): planned, building (parts done), built, with the outline and the notes path.',
  inputSchema: { id: z.string().optional() },
}, async ({ id }) => {
  await ensureTutor();
  const l = await lectureById(id);
  if (!l) return text(id ? `no lecture matches ${id}` : 'no lectures yet');
  const dir = path.join(ROOT, 'chat', 'lectures', l.id);
  return text([lectureLine(l), l.outline ? outlineText(l.outline) : '', l.notes ? `Notes: ${path.join(dir, 'notes.pdf')}` : '',
    l.played ? `Play: tutor lecture play ${l.id} (or lecture_play)` : '', l.job?.log ? `Log:\n${l.job.log.split('\n').slice(-6).join('\n')}` : ''].filter(Boolean).join('\n\n'));
});

server.registerTool('lecture_build', {
  title: 'Build a lecture',
  description: 'After the learner approved the outline: build every part in the background (hours; a usage limit pauses it, calling again resumes). Returns at once; lecture_status follows it.',
  inputSchema: { id: z.string().optional(), parts: z.array(z.number()).optional().describe('rebuild only these part numbers') },
}, async ({ id, parts }) => {
  await ensureTutor();
  const l = await lectureById(id);
  if (!l?.outline) return text('no planned lecture to build');
  const r = await postJson('/api/lecture/build', { id: l.id, parts });
  return text(r.error ? r.error : `Building ${l.id} in the background. Check with lecture_status; the Lectures page is ${BASE}/lectures.`);
});

server.registerTool('lecture_play', {
  title: 'Play a lecture',
  description: 'Open the stage window and play a built lecture (or the parts built so far). The learner can pause, go back a sentence, jump parts, type questions, and answer each part\'s closing question.',
  inputSchema: { id: z.string().optional() },
}, async ({ id }) => {
  await ensureTutor();
  const l = await lectureById(id);
  if (!l) return text('no lecture');
  const r = await postJson('/api/lecture/play', { id: l.id });
  return text(r.error ? r.error : `Playing ${l.id} on the stage. Tell the learner: space pauses, ← goes back a sentence, shift ←/→ jump parts, typing asks.`);
});

return server;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  await makeServer().connect(new StdioServerTransport());
