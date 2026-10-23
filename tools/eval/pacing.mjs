// Pacing after a formula step: play each scene headless and read the engine's timeline (when sentences start and
// end, when formulas settle). Reports, per variant: formula changes per sentence, the rest the finished formula gets
// before the next sentence, and how often the next sentence reads it back.
//
//   node tools/eval/pacing.mjs --out DIR [--variants full] [--tutor ./tutor]
//
// Per scene: sentences, formula sentences (a Say whose beat creates or changes a tex), changes in each, the rest
// after each (next Say start minus the later of this Say's end and the last formula settling), and whether the next
// beat points at the formula ([id], {…}, indicate id, id[part], an annotation of=id[part]). Targets (research_notes/Pacing/): at most one change per
// formula sentence, rest >= 0.8 s after every one, 4-10% of all sentence gaps >= 2 s.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const OUT = path.resolve(flag('--out', path.join(HERE, 'out')));
const TUTOR = path.resolve(flag('--tutor', path.join(ROOT, 'tutor')));
const variants = flag('--variants', 'full').split(',');

function measure(dir) {
  const played = ['played.dan', 'played.txt', 'scene.dan', 'scene.txt'].map((f) => path.join(dir, f)).find((f) => fs.existsSync(f));
  if (!played) return null;
  const r = spawnSync(TUTOR, [played, '--layout-report', '-'], { cwd: ROOT, encoding: 'utf8' });
  let rep;
  try { rep = JSON.parse(r.stdout.trim().split('\n').pop()); } catch { return null; }
  const ev = (rep.timeline || []).map((l) => { const [t, kind, id] = l.split(' '); return { t: +t, kind, id }; });
  // beats: each Say with the lines that belong to it (cued lines, set-up for the next), as the engine plays them
  const beats = [];
  for (const l of fs.readFileSync(played, 'utf8').split('\n')) {
    if (l.trim().startsWith('Say:')) beats.push(l);
    else if (beats.length) beats[beats.length - 1] += '\n' + l;
  }
  const says = beats;
  const sents = []; // {start, end, texIds, lastTex, text}
  for (const e of ev) {
    if (e.kind === 'say') sents.push({ start: e.t, end: null, texIds: [], lastTex: -1, text: says[sents.length] || '' });
    else if (e.kind === 'end' && sents.length) sents.at(-1).end = e.t;
    else if ((e.kind === 'tex' || e.kind === 'texnew') && sents.length) {
      const s = sents.at(-1); s.texIds.push(e.id); s.lastTex = Math.max(s.lastTex, e.kind === 'texnew' ? e.t + 1 : e.t);
    }
  }
  const m = { sentences: sents.length, formulaSentences: 0, changes: [], rests: [], gaps: [], readBack: 0, readBackOf: 0 };
  for (let i = 0; i < sents.length; i++) {
    const s = sents[i], n = sents[i + 1];
    if (n && s.end != null) m.gaps.push(n.start - s.end);
    if (!s.texIds.length) continue;
    m.formulaSentences++;
    m.changes.push(s.texIds.length);
    if (n && s.end != null) {
      m.rests.push(n.start - Math.max(s.end, s.lastTex));
      const id = s.texIds.at(-1);
      // the next beat points at the formula or a part of it: [f] in the words, {f}, indicate f / f[part], an annotation of=f[part]
      const re = new RegExp(`\\[${id}\\]|\\{[^}]*\\b${id}\\b|\\b${id}\\[|indicate ${id}\\b|of=\\[?${id}\\b`);
      m.readBackOf++;
      if (re.test(n.text)) m.readBack++;
    }
  }
  return m;
}

const share = (a, f) => (a.length ? (100 * a.filter(f).length) / a.length : 0);
const mean = (a) => (a.length ? a.reduce((x, y) => x + y, 0) / a.length : 0);
for (const v of variants) {
  const vd = path.join(OUT, v);
  if (!fs.existsSync(vd)) { console.log(`${v}: no such dir`); continue; }
  const all = { scenes: 0, sentences: 0, formulaSentences: 0, changes: [], rests: [], gaps: [], readBack: 0, readBackOf: 0 };
  const per = {};
  for (const id of fs.readdirSync(vd).sort()) {
    const m = measure(path.join(vd, id));
    if (!m) continue;
    per[id] = m;
    all.scenes++; all.sentences += m.sentences; all.formulaSentences += m.formulaSentences;
    all.changes.push(...m.changes); all.rests.push(...m.rests); all.gaps.push(...m.gaps);
    all.readBack += m.readBack; all.readBackOf += m.readBackOf;
  }
  fs.writeFileSync(path.join(vd, 'pacing.json'), JSON.stringify({ all, per }, null, 1));
  console.log(`${v}: ${all.scenes} scenes, ${all.sentences} sentences, ${all.formulaSentences} change a formula (${(100 * all.formulaSentences / Math.max(1, all.sentences)).toFixed(0)}%)`);
  console.log(`  changes per formula sentence: mean ${mean(all.changes).toFixed(2)}, 2+ in ${share(all.changes, (c) => c >= 2).toFixed(0)}%`);
  console.log(`  rest after a formula sentence: median ${all.rests.length ? [...all.rests].sort((a, b) => a - b)[Math.floor(all.rests.length / 2)].toFixed(2) : '-'} s; >= 0.8 s in ${share(all.rests, (r) => r >= 0.8).toFixed(0)}%, >= 2 s in ${share(all.rests, (r) => r >= 2).toFixed(0)}%`);
  console.log(`  all sentence gaps: >= 1 s in ${share(all.gaps, (g) => g >= 1).toFixed(0)}%, >= 2 s in ${share(all.gaps, (g) => g >= 2).toFixed(1)}%, >= 3 s in ${share(all.gaps, (g) => g >= 3).toFixed(1)}%`);
  console.log(`  next sentence reads the formula back: ${all.readBack}/${all.readBackOf} (${(100 * all.readBack / Math.max(1, all.readBackOf)).toFixed(0)}%)`);
}
