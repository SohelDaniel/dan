// Replay your recorded chats turn by turn and report layout problems (local only: sessions are personal and
// never committed). For each session and each turn, the turns so far are played as the stage played them:
// the layout report samples every half second (overlaps, worst moment, labels that flipped side).
//
//   node tools/audit-sessions.mjs [--last 8] [--only SESSION] [--frames DIR] [--sheet OUT.png] [--tutor BIN]
//     --frames  snapshots at each turn's worst moment
//     --sheet   every turn's end frame on one contact sheet, captioned "session/turn score"
//     --tutor   another build of the engine (an older commit, to compare); it still runs from this folder
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const TUTOR = path.resolve(flag('--tutor', path.join(ROOT, 'tutor')));
const only = flag('--only', '');
const sheet = flag('--sheet', '');
const tiles = [];
const last = Number(flag('--last', 8));
const frames = flag('--frames', '');
const SESS = path.join(ROOT, 'chat', 'sessions');
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'tutor-audit-'));

const sessions = fs.readdirSync(SESS).filter((s) => fs.existsSync(path.join(SESS, s, 'turns')) && (!only || s === only)).sort().slice(-last);
const rows = [];
for (const s of sessions) {
  const turns = fs.readdirSync(path.join(SESS, s, 'turns')).filter((f) => f.endsWith('.txt')).sort();
  let text = '';
  for (const t of turns) {
    text += fs.readFileSync(path.join(SESS, s, 'turns', t), 'utf8') + '\n';
    const f = path.join(tmp, `${s}-${t}`);
    fs.writeFileSync(f, text);
    const r = spawnSync(TUTOR, [f, '--layout-report', '-'], { cwd: ROOT, encoding: 'utf8' });
    let j;
    try { j = JSON.parse(r.stdout.trim().split('\n').pop()); } catch { rows.push({ session: s, turn: t, error: 'no report' }); continue; }
    const bad = j.samples.filter((x) => x[2] > 0).length;
    rows.push({ session: s, turn: t, worst: j.worst, mean: j.mean, bad: `${bad}/${j.samples.length}`, flips: j.labelFlips,
      at: j.worstAt, issue: (j.worstIssues[0] || '').slice(0, 70) });
    if (frames && j.worst < 95) {
      fs.mkdirSync(frames, { recursive: true });
      spawnSync(TUTOR, [f, '--snapshot', path.join(frames, `${s}-${t.replace('.txt', '')}`), '--at', `${j.worstAt},end`], { cwd: ROOT });
    }
    if (sheet) {
      const shot = path.join(tmp, `${s}-${t.replace('.txt', '')}`);
      spawnSync(TUTOR, [f, '--snapshot', shot, '--at', 'end'], { cwd: ROOT });
      if (fs.existsSync(`${shot}_end.png`)) tiles.push(['-label', `${s}/${t.replace('.txt', '')}  ${j.worst}`, `${shot}_end.png`]);
    }
  }
}
console.table(rows);
if (sheet && tiles.length) {
  const r = spawnSync('montage', ['-font', '/System/Library/Fonts/Supplemental/Arial.ttf', '-pointsize', '18', ...tiles.flat(),
    '-tile', '3x', '-geometry', '640x360+4+4', '-background', '#202020', '-fill', '#dddddd', sheet], { encoding: 'utf8' });
  console.log(r.status === 0 ? `sheet: ${sheet}` : `montage failed: ${r.stderr}`);
}
fs.rmSync(tmp, { recursive: true, force: true });
