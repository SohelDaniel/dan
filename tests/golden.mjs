// Layout regression: play every script headless, check its layout, compare with the saved baseline.
//   node tests/golden.mjs           report + compare (exit 1 if any script got worse)
//   node tests/golden.mjs --save    save the current results as the baseline
//   --placer old                    the same, for the old fixed-offset labels (their own baseline)
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const save = process.argv.includes('--save');
const placer = process.argv.includes('--placer') ? process.argv[process.argv.indexOf('--placer') + 1] : 'new';
const GOLDEN = path.join(ROOT, 'tests', 'golden', placer === 'old' ? 'layout-old.json' : 'layout-new.json'); // one baseline per placer
const TOL = 2; // score points of slack (sampling is time-based)

const files = [...fs.readdirSync(path.join(ROOT, 'scripts')).filter((f) => f.endsWith('.txt')).map((f) => 'scripts/' + f),
  ...fs.readdirSync(path.join(ROOT, 'chat', 'exemplars')).filter((f) => f.endsWith('.txt')).map((f) => 'chat/exemplars/' + f)].sort();
const results = {};
for (const f of files) {
  const args = [f, '--layout-report', '-', '--placer', placer];
  const r = spawnSync(path.join(ROOT, 'tutor'), args, { cwd: ROOT, encoding: 'utf8' });
  try { results[f] = JSON.parse(r.stdout.trim().split('\n').pop()); }
  catch { results[f] = { error: (r.stderr || r.stdout).slice(-300) }; }
}
const base = fs.existsSync(GOLDEN) ? JSON.parse(fs.readFileSync(GOLDEN, 'utf8')) : {};
let worse = 0;
const pad = (s, n) => String(s).padEnd(n);
console.log(pad('script', 34), pad('final', 7), pad('worst', 7), pad('mean', 7), pad('overl', 6), pad('flips', 6), 'vs baseline');
for (const f of files) {
  const r = results[f];
  if (r.error) { console.log(pad(f, 34), 'ERROR', r.error); worse++; continue; }
  const b = base[f];
  let cmp = '';
  if (b) {
    const d = r.mean - b.mean;
    cmp = d < -TOL || r.maxOverlaps > b.maxOverlaps ? `WORSE (${b.mean.toFixed(1)} -> ${r.mean.toFixed(1)})` : d > TOL ? `better (+${d.toFixed(1)})` : 'same';
    if (cmp.startsWith('WORSE')) worse++;
  }
  console.log(pad(f, 34), pad(r.final.toFixed(1), 7), pad(r.worst.toFixed(1), 7), pad(r.mean.toFixed(1), 7), pad(r.maxOverlaps, 6), pad(r.labelFlips ?? '-', 6), cmp);
  for (const m of r.issues.slice(0, 2)) console.log('   -', m);
  if (r.worst < r.final) for (const m of (r.worstIssues || []).slice(0, 2)) console.log(`   - at ${r.worstAt}s:`, m);
}
const mean = Object.values(results).filter((r) => !r.error).reduce((a, r) => a + r.mean, 0) / files.length;
console.log(`\nmean layout score ${mean.toFixed(1)} over ${files.length} scripts`);
if (save) {
  fs.mkdirSync(path.dirname(GOLDEN), { recursive: true });
  fs.writeFileSync(GOLDEN, JSON.stringify(Object.fromEntries(Object.entries(results).map(([k, v]) => [k, { mean: v.mean, worst: v.worst, final: v.final, maxOverlaps: v.maxOverlaps }])), null, 1));
  console.log('saved baseline', path.relative(ROOT, GOLDEN));
}
process.exit(!save && worse ? 1 : 0);
