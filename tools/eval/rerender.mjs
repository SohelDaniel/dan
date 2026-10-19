// A/B the placer on scenes already generated: re-render each scene with --placer new into
// <variant>-newplacer, and write geometric layout reports for both.
//   node tools/eval/rerender.mjs --out DIR --variants full,full-opus
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const OUT = path.resolve(flag('--out', path.join(HERE, 'out')));
const variants = flag('--variants', 'full').split(',');

const report = (scene, extra) => {
  const r = spawnSync(TUTOR, [scene, '--layout-report', '-', ...extra], { cwd: ROOT, encoding: 'utf8' });
  try { return JSON.parse(r.stdout.trim().split('\n').pop()); } catch { return null; }
};
const rows = [];
for (const v of variants) {
  const src = path.join(OUT, v), dst = path.join(OUT, v + '-newplacer');
  for (const id of fs.readdirSync(src)) {
    const from = path.join(src, id);
    if (!fs.existsSync(path.join(from, 'scene.txt'))) continue;
    const to = path.join(dst, id);
    fs.mkdirSync(to, { recursive: true });
    for (const f of ['scene.txt', 'meta.json', 'reply.txt', 'plan.txt', 'check.txt']) if (fs.existsSync(path.join(from, f))) fs.copyFileSync(path.join(from, f), path.join(to, f));
    for (const f of fs.readdirSync(to)) if (f.startsWith('frame_')) fs.unlinkSync(path.join(to, f));
    spawnSync(TUTOR, [path.join(to, 'scene.txt'), '--placer', 'new', '--snapshot', path.join(to, 'frame'), '--at', '6,14,end'], { cwd: ROOT });
    const a = report(path.join(from, 'scene.txt'), []), b = report(path.join(to, 'scene.txt'), ['--placer', 'new']);
    if (a) fs.writeFileSync(path.join(from, 'layout.json'), JSON.stringify(a));
    if (b) fs.writeFileSync(path.join(to, 'layout.json'), JSON.stringify(b));
    rows.push({ variant: v, id, old: a?.mean ?? NaN, new: b?.mean ?? NaN, oldWorst: a?.worst ?? NaN, newWorst: b?.worst ?? NaN, flips: b?.labelFlips ?? 0 });
  }
}
console.table(rows.map((r) => ({ ...r, old: r.old.toFixed(1), new: r.new.toFixed(1), oldWorst: r.oldWorst.toFixed(1), newWorst: r.newWorst.toFixed(1) })));
const mean = (k) => (rows.reduce((s, r) => s + r[k], 0) / rows.length).toFixed(1);
console.log(`mean geometric layout score: old ${mean('old')} -> new ${mean('new')}  (worst moment: ${mean('oldWorst')} -> ${mean('newWorst')})`);
