// Binding, sentence by sentence: a frame at the end of every Say (just before the next one starts), and a Claude
// judge that says, for each sentence, whether that frame shows what the sentence claims. judge.mjs sees three
// frames per scene, so it can't tell whether a claim was shown when it was said; this can.
//
//   node tools/eval/sentences.mjs --out DIR [--variants full] [--model sonnet] [--jobs 4] [--only a,b]
//
// Writes DIR/<variant>/<id>/sent/f_<t>.png and sentences.json, and prints per variant the share of claims shown
// (yes / partly / no), overall and by kind of claim.
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const OUT = path.resolve(flag('--out', path.join(HERE, 'out')));
const variants = flag('--variants', 'full').split(',');
const model = flag('--model', 'sonnet');
const jobs = Number(flag('--jobs', 4));
const only = flag('--only', '');
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const RUBRIC = `You judge one thing about an AI visual tutor: does the picture show what each sentence says, at the
moment it is said? Below are the spoken sentences in order. For each sentence there is a frame of the stage
taken just as that sentence ends. Read every frame with the Read tool.

For each sentence, decide:
- claim: the visual claim the sentence makes, in a few words: a number/quantity, a count, a step or action
  ("add them", "it doubles", "splits"), a sameness or difference, a comparison, a thing pointed at ("this curve").
  Use "none" for metaphors, transitions, questions to the learner and asides that need no picture.
- shown: "yes" if the frame (or the change into it) visibly carries that claim, "partly", or "no". Use "na" when
  claim is "none". A number counts as shown only if it is readable on stage or is a count you can see. A step
  counts as shown if its result is visibly there (a sum appears, a thing moved, a colour marks the group).

Reply with ONLY JSON: {"sentences":[{"i":1,"claim":"...","shown":"yes|partly|no|na"}, ...]}`;

// this turn's Says with the time each starts, from a muted verbose run; a follow-up starts from an earlier stage
function says(dir) {
  const played = path.join(dir, 'played.txt');
  const v = spawnSync(TUTOR, [played, '--snapshot', path.join(dir, 'sent', 'x'), '--at', 'end', '--verbose'], { cwd: ROOT, encoding: 'utf8' });
  const first = fs.readFileSync(played, 'utf8').split('\n').length - fs.readFileSync(path.join(dir, 'scene.txt'), 'utf8').split('\n').length + 1;
  const all = [...(v.stderr || '').matchAll(/^\[\s*([\d.]+)s\] line (\d+): Say:\s*"?(.*?)"?\s*$/gm)]
    .map((m) => ({ t: +m[1], line: +m[2], text: m[3].replace(/\|[^|\s]*\|/g, '').replace(/\[[^\]\s]*\]/g, '') }));
  return all.map((s, i) => ({ ...s, at: all[i + 1] ? Math.max(s.t + 0.5, all[i + 1].t - 0.15).toFixed(2) : 'end' }))
    .filter((s) => s.line >= first);
}

function frames(dir) {
  const out = path.join(dir, 'sent');
  fs.rmSync(out, { recursive: true, force: true });
  fs.mkdirSync(out);
  const ss = says(dir);
  if (!ss.length) return [];
  // rendering opens a hidden window, which can fail when many start at once: retry
  for (let attempt = 0; attempt < 3 && !fs.readdirSync(out).some((f) => f.startsWith('f_')); attempt++)
    spawnSync(TUTOR, [path.join(dir, 'played.txt'), '--snapshot', path.join(out, 'f'), '--at', ss.map((s) => s.at).join(',')], { cwd: ROOT });
  return ss.map((s) => ({ ...s, frame: path.join(out, `f_${s.at}.png`) }));
}

function judge(dir) {
  return new Promise((resolve) => {
    const ss = frames(dir);
    const name = `${path.basename(path.dirname(dir))}/${path.basename(dir)}`;
    if (!ss.length || !ss.every((s) => fs.existsSync(s.frame))) {
      console.warn(`${name}: frames failed to render`);
      return resolve();
    }
    const body = ss.map((s, i) => `${i + 1}. "${s.text}"\n   frame: ${s.frame}`).join('\n');
    const c = spawn('claude', ['-p', `${RUBRIC}\n\n${body}`, '--output-format', 'json', '--model', model, '--effort', 'low',
      '--strict-mcp-config', '--tools', 'Read', '--allowedTools', 'Read', '--add-dir', dir], { cwd: dir, env: ENV });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let res = '';
      try { res = JSON.parse(out).result || ''; } catch { res = out; }
      let j;
      try { j = JSON.parse(res.match(/\{[\s\S]*\}/)[0]); } catch { j = { error: res.slice(0, 300) }; }
      fs.writeFileSync(path.join(dir, 'sentences.json'), JSON.stringify(j, null, 1));
      const claims = (j.sentences || []).filter((x) => x.shown !== 'na');
      const n = (k) => claims.filter((x) => x.shown === k).length;
      console.log(`${name}: ${claims.length} claims, yes ${n('yes')}, partly ${n('partly')}, no ${n('no')}`);
      resolve();
    });
  });
}

// the kind of claim, from the judge's words: numbers are checked by construction, the rest by judgement
function kind(claim) {
  const c = claim.toLowerCase();
  if (/\d|\b(one|two|three|four|five|six|seven|eight|nine|ten|hundred|thousand|million|half|percent|twice)\b/.test(c)) return 'number';
  if (/add|multipl|sum|split|move|double|turn|becom|cancel|shrink|grow|flow|rotat|shift|swap|chang|step|pass|travel|jump|fall|rise|push|pull|cross|bend|combin|appear/.test(c)) return 'process';
  if (/same|share|differ|each|own|equal|match/.test(c)) return 'same/different';
  if (/bigger|smaller|faster|slower|more|less|steeper|higher|lower|than|larger/.test(c)) return 'comparison';
  return 'other';
}

const dirs = [];
for (const v of variants) {
  const vd = path.join(OUT, v);
  if (!fs.existsSync(vd)) continue;
  for (const id of fs.readdirSync(vd))
    if (fs.existsSync(path.join(vd, id, 'played.txt')) && (!only || only.split(',').includes(id))) dirs.push(path.join(vd, id));
}
const queue = [...dirs];
await Promise.all(Array.from({ length: jobs }, async () => { while (queue.length) await judge(queue.shift()); }));

// ---- summary
const pct = (a, b) => (b ? Math.round((100 * a) / b) + '%' : '-');
const rows = [];
for (const v of variants) {
  const vd = path.join(OUT, v);
  if (!fs.existsSync(vd)) continue;
  const claims = [];
  for (const id of fs.readdirSync(vd)) {
    try { claims.push(...(JSON.parse(fs.readFileSync(path.join(vd, id, 'sentences.json'), 'utf8')).sentences || []).filter((x) => x.shown !== 'na')); } catch {}
  }
  for (const k of ['all', 'number', 'process', 'same/different', 'comparison', 'other']) {
    const cs = k === 'all' ? claims : claims.filter((c) => kind(c.claim || '') === k);
    const n = (s) => cs.filter((c) => c.shown === s).length;
    rows.push({ variant: v, kind: k, claims: cs.length, yes: pct(n('yes'), cs.length), partly: pct(n('partly'), cs.length), no: pct(n('no'), cs.length) });
  }
}
console.table(rows);
fs.writeFileSync(path.join(OUT, 'sentences-summary.json'), JSON.stringify(rows, null, 1));
