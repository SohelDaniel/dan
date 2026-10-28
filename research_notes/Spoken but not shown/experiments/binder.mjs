// Experiment (no repo changes): a second model edits each beat before it would play, so the picture shows what
// the sentence claims. The spoken words are frozen (their audio is already being made); only visual lines,
// |cue| markers and [id] pointers may change. Reads EVAL/full/<id>, writes EVAL/<variant>/<id> in the same
// shape run.mjs writes, so judge.mjs can score it.
//
//   node binder.mjs --eval DIR --model haiku --variant binder-haiku [--jobs 6] [--only a,b]
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';

const ROOT = '<repo>';
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const EVAL = flag('--eval');
const model = flag('--model', 'haiku');
const variant = flag('--variant', 'binder-' + model);
const jobs = Number(flag('--jobs', 6));
const only = flag('--only', '');
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
const LANG = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
const prompt = fs.readFileSync(path.join(ROOT, 'chat', 'prompt.md'), 'utf8');
const SHOW = prompt.slice(prompt.indexOf('## Show what you say'), prompt.indexOf('**The 3Blue1Brown way**'));

const SYSTEM = `You are the binding editor of a live visual tutor. A narrator has written a scene in a markup language;
it plays beat by beat (a beat = one \`Say:\` line plus the lines after it). Each Say is spoken aloud. Your one job:
make the picture show what the sentence claims, AT THE MOMENT it is said.

You get the hidden plan, the scene so far (already on stage when this beat plays), and ONE beat. Return that beat,
edited, in a \`\`\`beat block and nothing else.

Hard rules:
- The spoken words are frozen. Never add, remove, reorder or change a word of a Say line. You MAY glue \`|cue|\`
  markers and \`[id]\` pointers onto existing words (\`|go|doubles\`, \`[w1]weight\`).
- Every \`|cue|\` you add needs an \`@cue\` line in this beat, and the reverse. A cue name is one word of letters.
- Only reference ids that exist in the scene so far or this beat, or that you create with \`mark\` in this beat.
- Keep every existing line unless it is broken. Prefer adding over rewriting.
- Never put the sentence itself on stage. Labels, numbers, values on links, \\val{}, colours, motion.
- Do nothing for metaphors, asides, questions to the learner, or claims the stage already shows. If the beat
  already binds, return it unchanged. Do not add clutter: one small change on the right word beats three.

What counts as a claim the picture must carry: a count or quantity (the number on the thing, as it is said), a
sameness/sharing (a shared colour on exactly those things), "each has its own" (distinct labels), a process or
action ("adds", "doubles", "moves", "splits": the change happening on that word), a comparison ("bigger",
"faster": both visible, the difference marked), "this/that X" (a pointer to X), a formula symbol (coloured like
its thing).

${SHOW}

${LANG}`;

const isSay = (l) => l.trim().startsWith('Say:');
const spoken = (l) => l.trim().slice(4).replace(/\|[^|\s]*\|/g, '').replace(/\[[^\]\s]*\]/g, '').replace(/\s+/g, ' ').trim();

function beats(scene) {
  const lines = scene.split('\n');
  const setup = [], out = [];
  for (const l of lines) {
    if (isSay(l)) out.push([l]);
    else if (!out.length) setup.push(l);
    else out.at(-1).push(l);
  }
  return { setup, beats: out };
}

function claude(ctx) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const c = spawn('claude', ['-p', ctx, '--output-format', 'json', '--tools', '', '--strict-mcp-config', '--model', model,
      '--effort', 'low', '--system-prompt', SYSTEM], { env: ENV });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let j = {};
      try { j = JSON.parse(out); } catch {}
      resolve({ text: j.result || out, wall: (Date.now() - t0) / 1000, api: (j.duration_api_ms || 0) / 1000 });
    });
  });
}

const checkOk = (file) => spawnSync(TUTOR, [file, '--check'], { cwd: ROOT, encoding: 'utf8' });

async function bind(id) {
  const src = path.join(EVAL, 'full', id), dst = path.join(EVAL, variant, id);
  fs.mkdirSync(dst, { recursive: true });
  const meta0 = JSON.parse(fs.readFileSync(path.join(src, 'meta.json'), 'utf8'));
  const plan = fs.readFileSync(path.join(src, 'plan.txt'), 'utf8');
  const scene = fs.readFileSync(path.join(src, 'scene.txt'), 'utf8');
  const played0 = fs.readFileSync(path.join(src, 'played.txt'), 'utf8');
  const prior = played0.slice(0, played0.length - scene.length); // a follow-up's starting stage
  const { setup, beats: bs } = beats(scene);
  let sofar = [...setup];
  const log = [];
  for (const b of bs) {
    const ctx = `<plan>\n${plan}</plan>\n\n<scene_so_far>\n${prior}${sofar.join('\n')}\n</scene_so_far>\n\n<beat>\n${b.join('\n')}\n</beat>`;
    const r = await claude(ctx);
    const m = r.text.match(/```beat\s*\n([\s\S]*?)```/);
    let nb = m ? m[1].replace(/\n+$/, '').split('\n') : null, why = m ? 'ok' : 'no block';
    if (nb) {
      const s0 = b.filter(isSay).map(spoken), s1 = nb.filter(isSay).map(spoken);
      if (JSON.stringify(s0) !== JSON.stringify(s1)) { nb = null; why = 'words changed'; }
    }
    if (nb) { // must still parse and check as well as before
      const tmp = path.join(dst, '.try.txt');
      fs.writeFileSync(tmp, prior + [...sofar, ...nb].join('\n') + '\n');
      const was = path.join(dst, '.was.txt');
      fs.writeFileSync(was, prior + [...sofar, ...b].join('\n') + '\n');
      const a = checkOk(tmp), w = checkOk(was);
      // other lints (cues never spoken, pointers to nothing, parse errors) must not grow
      const other = (r) => ((r.stderr || '') + (r.stdout || '')).split('\n').filter((l) => /^lint:|error/i.test(l) && !/binding:|layout:|zero:/.test(l)).length;
      if ((a.status !== 0 && w.status === 0) || other(a) > other(w)) { nb = null; why = 'new lints'; }
    }
    const changed = nb && nb.join('\n') !== b.join('\n');
    log.push({ wall: r.wall, api: r.api, why, changed, before: b, after: nb || b });
    sofar.push(...(nb || b));
  }
  for (const f of ['.try.txt', '.was.txt']) fs.rmSync(path.join(dst, f), { force: true });
  const newScene = sofar.join('\n');
  const played = prior + newScene;
  fs.writeFileSync(path.join(dst, 'scene.txt'), newScene);
  fs.writeFileSync(path.join(dst, 'played.txt'), played);
  fs.writeFileSync(path.join(dst, 'plan.txt'), plan);
  fs.writeFileSync(path.join(dst, 'binder.json'), JSON.stringify(log, null, 1));
  const chk = checkOk(path.join(dst, 'played.txt'));
  fs.writeFileSync(path.join(dst, 'check.txt'), (chk.stdout || '') + (chk.stderr || ''));
  for (let a = 0; a < 3 && !fs.readdirSync(dst).some((f) => f.startsWith('frame_')); a++)
    spawnSync(TUTOR, [path.join(dst, 'played.txt'), '--snapshot', path.join(dst, 'frame'), '--at', '6,14,end'], { cwd: ROOT });
  const lr = spawnSync(TUTOR, [path.join(dst, 'played.txt'), '--layout-report', '-'], { cwd: ROOT, encoding: 'utf8' });
  let layout = null;
  try { layout = JSON.parse(lr.stdout.trim().split('\n').pop()); fs.writeFileSync(path.join(dst, 'layout.json'), JSON.stringify(layout)); } catch {}
  const meta = { ...meta0, variant, ok: chk.status === 0, layoutGeom: layout ? layout.mean : null,
    binding: ((chk.stdout || '').match(/^  binding:/gm) || []).length, lines: newScene.split('\n').filter((l) => l.trim()).length,
    binder: { beats: log.length, changed: log.filter((l) => l.changed).length, rejected: log.filter((l) => l.why !== 'ok').map((l) => l.why),
      wallMedian: log.map((l) => l.wall).sort((a, b) => a - b)[log.length >> 1], apiMedian: log.map((l) => l.api).sort((a, b) => a - b)[log.length >> 1] } };
  fs.writeFileSync(path.join(dst, 'meta.json'), JSON.stringify(meta, null, 1));
  console.log(`${id}: ${meta.binder.changed}/${log.length} beats changed, rejected ${meta.binder.rejected.length}, check ${meta.ok ? 'ok' : 'FAIL'}, lints ${meta0.binding} -> ${meta.binding}, ${meta.binder.wallMedian?.toFixed(1)} s/beat`);
}

let ids = fs.readdirSync(path.join(EVAL, 'full')).filter((d) => fs.existsSync(path.join(EVAL, 'full', d, 'meta.json')));
if (only) ids = ids.filter((i) => only.split(',').includes(i));
const q = [...ids];
await Promise.all(Array.from({ length: jobs }, async () => { while (q.length) await bind(q.shift()); }));
