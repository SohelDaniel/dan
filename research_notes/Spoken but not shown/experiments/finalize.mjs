// Re-applies the binder's saved per-beat edits (binder.json) with the guard working (absolute paths this time):
// an edit is kept only if the scene still checks and no new non-binding lints appear. Then check, frames,
// layout report and meta, as run.mjs writes them.
//   node finalize.mjs --eval /abs/eval --variant binder-sonnet
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const ROOT = '/Users/soheldanial/code/server/testing/visulaizations/tutor';
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const EVAL = path.resolve(flag('--eval')), variant = flag('--variant');
const run = (f, ...a) => spawnSync(TUTOR, [f, ...a], { cwd: ROOT, encoding: 'utf8' });
const other = (r) => ((r.stderr || '') + (r.stdout || '')).split('\n')
  .filter((l) => /^lint:|error/i.test(l) && !/binding:|layout:|zero:/.test(l)).length;

for (const id of fs.readdirSync(path.join(EVAL, variant))) {
  const dst = path.join(EVAL, variant, id), src = path.join(EVAL, 'full', id);
  if (!fs.existsSync(path.join(dst, 'binder.json'))) continue;
  const log = JSON.parse(fs.readFileSync(path.join(dst, 'binder.json'), 'utf8'));
  const scene0 = fs.readFileSync(path.join(src, 'scene.txt'), 'utf8');
  const played0 = fs.readFileSync(path.join(src, 'played.txt'), 'utf8');
  const prior = played0.slice(0, played0.length - scene0.length);
  const lines = scene0.split('\n');
  const sofar = lines.slice(0, lines.findIndex((l) => l.trim().startsWith('Say:')));
  let kept = 0, guarded = 0;
  const tmp = path.join(dst, '.try.txt'), was = path.join(dst, '.was.txt');
  for (const b of log) {
    let use = b.before;
    if (b.changed && b.why === 'ok') {
      fs.writeFileSync(tmp, prior + [...sofar, ...b.after].join('\n') + '\n');
      fs.writeFileSync(was, prior + [...sofar, ...b.before].join('\n') + '\n');
      const a = run(tmp, '--check'), w = run(was, '--check');
      if ((a.status !== 0 && w.status === 0) || other(a) > other(w)) guarded++;
      else { use = b.after; kept++; }
    }
    sofar.push(...use);
  }
  fs.rmSync(tmp, { force: true }); fs.rmSync(was, { force: true });
  const scene = sofar.join('\n'), played = prior + scene;
  fs.writeFileSync(path.join(dst, 'scene.txt'), scene);
  fs.writeFileSync(path.join(dst, 'played.txt'), played);
  for (const f of fs.readdirSync(dst)) if (f.startsWith('frame_') || f === 'score.json') fs.rmSync(path.join(dst, f));
  const chk = run(path.join(dst, 'played.txt'), '--check');
  fs.writeFileSync(path.join(dst, 'check.txt'), (chk.stdout || '') + (chk.stderr || ''));
  for (let a = 0; a < 3 && !fs.readdirSync(dst).some((f) => f.startsWith('frame_')); a++)
    run(path.join(dst, 'played.txt'), '--snapshot', path.join(dst, 'frame'), '--at', '6,14,end');
  const lr = run(path.join(dst, 'played.txt'), '--layout-report', '-');
  let layout = null;
  try { layout = JSON.parse(lr.stdout.trim().split('\n').pop()); fs.writeFileSync(path.join(dst, 'layout.json'), JSON.stringify(layout)); } catch {}
  const meta = JSON.parse(fs.readFileSync(path.join(dst, 'meta.json'), 'utf8'));
  Object.assign(meta, { ok: chk.status === 0, layoutGeom: layout ? layout.mean : null,
    binding: ((chk.stdout || '').match(/^  binding:/gm) || []).length, lines: scene.split('\n').filter((l) => l.trim()).length });
  meta.binder = { ...meta.binder, kept, guarded };
  fs.writeFileSync(path.join(dst, 'meta.json'), JSON.stringify(meta, null, 1));
  const frames = fs.readdirSync(dst).filter((f) => f.startsWith('frame_')).length;
  console.log(`${variant}/${id}: kept ${kept}, guarded ${guarded}, lints ${meta.binding}, check ${meta.ok ? 'ok' : 'FAIL'}, frames ${frames}`);
}
