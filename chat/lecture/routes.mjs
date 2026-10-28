// Lecture mode in the browser (chat/lectures.html): upload files, read and edit the plan, build, play, print.
// The long steps (plan, build, notes) run as their own processes (`chat/lecture/cli.mjs`), so they outlive a server
// restart; each writes job.log and job.pid in the lecture's folder, and the page polls the state.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { create, list, find, dirOf, readJson, writeJson } from './store.mjs';
import { validOutline, targetMinutes } from './plan.mjs';
import { partFile } from './build.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const CLI = path.join(HERE, 'cli.mjs');

const alive = (pid) => { try { process.kill(pid, 0); return true; } catch { return false; } };

function job(m) {
  const dir = dirOf(m.id);
  const pid = Number(fs.existsSync(path.join(dir, 'job.pid')) && fs.readFileSync(path.join(dir, 'job.pid'), 'utf8'));
  const log = fs.existsSync(path.join(dir, 'job.log')) ? fs.readFileSync(path.join(dir, 'job.log'), 'utf8').split('\n').slice(-30).join('\n') : '';
  return { running: pid > 0 && alive(pid), log };
}

function start(m, cmd, extra = []) {
  if (job(m).running) return false;
  const dir = dirOf(m.id);
  const out = fs.openSync(path.join(dir, 'job.log'), 'a');
  fs.writeSync(out, `\n=== ${cmd} ${new Date().toLocaleString()} ===\n`);
  const p = spawn(process.execPath, [CLI, cmd, m.id, ...extra], { detached: true, stdio: ['ignore', out, out] });
  fs.writeFileSync(path.join(dir, 'job.pid'), String(p.pid));
  p.unref();
  return true;
}

function state(m) {
  const dir = dirOf(m.id);
  const outline = readJson(path.join(dir, 'outline.json'), null);
  const parts = outline ? outline.parts.map((_, i) => {
    const r = readJson(partFile(m, i + 1).replace(/\.dan$/, '.report.json'), null);
    return r && { seconds: r.seconds, critic: r.rounds.at(-1)?.review?.score ?? null, rounds: r.rounds.length };
  }) : [];
  return { ...m, target: targetMinutes(m), outline, parts, job: job(m),
    played: fs.existsSync(path.join(dir, 'lecture.dan')), notes: fs.existsSync(path.join(dir, 'notes.pdf')) };
}

// -> true when it answered the request
export async function lectureRoute(req, res, url, readBody, json) {
  if (req.method === 'GET' && url.pathname === '/lectures') {
    res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
    res.end(fs.readFileSync(path.join(HERE, '..', 'lectures.html')));
    return true;
  }
  if (req.method === 'GET' && url.pathname === '/api/lectures') { json(res, { lectures: list().map(state) }); return true; }
  if (req.method === 'GET' && url.pathname === '/api/lecture/file') { // notes.pdf / notes.md, to open or save
    const m = find(url.searchParams.get('id'));
    const name = url.searchParams.get('f');
    if (!m || !['notes.pdf', 'notes.md'].includes(name) || !fs.existsSync(path.join(dirOf(m.id), name))) { json(res, { error: 'not found' }, 404); return true; }
    res.writeHead(200, { 'content-type': name.endsWith('.pdf') ? 'application/pdf' : 'text/markdown; charset=utf-8' });
    res.end(fs.readFileSync(path.join(dirOf(m.id), name)));
    return true;
  }
  if (req.method !== 'POST' || !url.pathname.startsWith('/api/lecture/')) return false;
  const b = await readBody(req);
  const what = url.pathname.slice('/api/lecture/'.length);

  if (what === 'new') { // files arrive as base64; they are copied into the lecture and planned
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'lecture-'));
    const files = (Array.isArray(b.files) ? b.files : []).filter((f) => typeof f?.data === 'string').map((f) => {
      const to = path.join(tmp, path.basename(String(f.name || 'file')).replace(/[^\w.-]+/g, '_'));
      fs.writeFileSync(to, Buffer.from(f.data, 'base64'));
      return to;
    });
    if (!files.length) { json(res, { error: 'no files' }, 400); return true; }
    const m = create(files, { minutes: Number(b.minutes) || undefined, request: String(b.ask || ''), model: b.model || 'opus', effort: b.effort || 'high' });
    fs.rmSync(tmp, { recursive: true, force: true });
    start(m, 'plan');
    json(res, { id: m.id });
    return true;
  }
  const m = find(String(b.id || ''));
  if (!m || m.id !== b.id) { json(res, { error: 'no such lecture' }, 404); return true; }
  if (what === 'outline') { // the learner edited the plan before building
    const problems = validOutline(b.outline);
    if (problems.length) { json(res, { error: problems.join('; ') }, 400); return true; }
    writeJson(path.join(dirOf(m.id), 'outline.json'), b.outline);
    json(res, { ok: true });
    return true;
  }
  if (['plan', 'build', 'notes', 'play'].includes(what)) {
    const extra = what === 'build' && Array.isArray(b.parts) ? ['--parts', b.parts.join(',')] : [];
    json(res, start(m, what, extra) ? { ok: true } : { error: 'busy: a step is running' }, 200);
    return true;
  }
  if (what === 'stop') {
    const pid = Number(fs.readFileSync(path.join(dirOf(m.id), 'job.pid'), 'utf8'));
    // its own process group (started from the page): the claude calls under it stop too; else just the process
    if (alive(pid)) try { process.kill(-pid); } catch { process.kill(pid); }
    json(res, { ok: true });
    return true;
  }
  return false;
}
