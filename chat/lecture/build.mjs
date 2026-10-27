// Building a planned lecture, one part at a time: write, check, look, fix.
//   1. Claude writes the part from the outline, the bible, the source and what earlier parts said.
//   2. Python blocks run; `tutor --check` lints go back to Claude until clean (at most CHECK_ROUNDS fixes).
//   3. Frames at moments through the part + the layout report go to a separate critic call; below a 4 it is
//      rewritten with the critic's problems (at most CRITIC_ROUNDS times). The best version is kept.
// Each finished part is a checkpoint (parts/NN.dan): a crash or a usage limit resumes at the next part.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { buildSystem } from '../system-prompt.mjs';
import { expandReply } from '../compute.mjs';
import { textOf } from '../ingest.mjs';
import { ask, fence } from './claude.mjs';
import { dirOf, readJson, writeJson, saveMeta, sourceFiles } from './store.mjs';
import { sourceBlocks } from './plan.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TUTOR = path.join(ROOT, 'tutor');
const CHECK_ROUNDS = 3, CRITIC_ROUNDS = 2, FRAMES = 6;
const WHOLE_SOURCE_WORDS = 40_000; // up to this, every part sees all the sources; above, its own pages

export const partFile = (m, n) => path.join(dirOf(m.id), 'parts', `${String(n).padStart(2, '0')}.dan`);
const workDir = (m, n) => path.join(dirOf(m.id), 'parts', 'work', String(n).padStart(2, '0'));
const says = (scene) => scene.split('\n').filter((l) => /^\s*Say:/.test(l)).map((l) => l.replace(/^\s*Say:\s*/, ''));

// The sources a part is written from: all of them, or for a big lecture the PDF pages its plan names (± 1).
function partSources(m, part) {
  if (m.sources.reduce((n, s) => n + s.words, 0) <= WHOLE_SOURCE_WORDS) return sourceBlocks(m);
  const blocks = [];
  for (const f of sourceFiles(m)) {
    if (!part.source?.includes(f.name) && m.sources.length > 1) continue;
    let text = textOf(f);
    const ranges = [...(part.source || '').matchAll(/(\d+)\s*(?:[-–]\s*(\d+))?/g)].map((r) => [Number(r[1]) - 1, Number(r[2] || r[1]) + 1]);
    if (f.kind === 'pdf' && ranges.length)
      text = text.split(/(?=\[page \d+\])/).filter((pg) => {
        const n = Number(/\[page (\d+)\]/.exec(pg)?.[1]);
        return ranges.some(([a, b]) => n >= a && n <= b);
      }).join('\n');
    blocks.push({ type: 'text', text: `<file name="${f.name}">\n${text.slice(0, 200_000)}\n</file>` });
  }
  return blocks;
}

function partContext(m, outline, bible, n) {
  const part = outline.parts[n - 1];
  const before = outline.parts.slice(0, n - 1).map((p, i) => {
    const f = partFile(m, i + 1);
    const said = fs.existsSync(f) ? says(fs.readFileSync(f, 'utf8')).join(' ') : '(not built)';
    return `## Part ${i + 1}: ${p.title}\n${said}`;
  });
  return [
    `<lecture title="${outline.title}" level="${outline.level}" parts="${outline.parts.length}">`,
    outline.parts.map((p, i) => `${i + 1}. ${p.title}: ${p.goal}`).join('\n'), '</lecture>',
    `<bible>\n${bible.trim()}\n</bible>`,
    before.length ? `<said_before>\nWhat earlier parts said (the learner has heard all of it):\n\n${before.join('\n\n')}\n</said_before>` : '',
    `<this_part n="${n}">\n${JSON.stringify(part, null, 2)}\n</this_part>`,
    `Write part ${n} of ${outline.parts.length}: "${part.title}". The sources it teaches from are attached.`,
  ].filter(Boolean).join('\n\n');
}

const tutor = (args) => spawnSync(TUTOR, args, { cwd: ROOT, encoding: 'utf8', maxBuffer: 1 << 26 });

// Play-free measures of a draft: lints, the layout report, frames at moments spread over the narration.
async function evaluate(reply, dir) {
  fs.mkdirSync(dir, { recursive: true });
  const { scene, problems } = await expandReply(reply);
  const file = path.join(dir, 'scene.dan');
  fs.writeFileSync(file, scene);
  const chk = tutor([file, '--check']);
  const out = (chk.stdout || '') + (chk.stderr || '');
  const lints = [...new Set(out.split('\n').filter((l) => l.startsWith('  ') || l.startsWith('parse:')).map((l) => l.trim()))];
  let layout = null;
  try { layout = JSON.parse(tutor([file, '--layout-report', '-']).stdout.trim().split('\n').pop()); } catch {}
  const timeline = (layout?.timeline || []).map((t) => t.split(' ')).map(([s, kind, line]) => ({ t: Number(s), kind, line: Number(line) }));
  const starts = timeline.filter((e) => e.kind === 'say');
  const at = starts.length <= FRAMES ? starts.map((e) => e.t + 1.5)
    : Array.from({ length: FRAMES - 1 }, (_, i) => starts[Math.round(((i + 1) * (starts.length - 1)) / FRAMES)].t + 1.5);
  for (const f of fs.readdirSync(dir)) if (f.startsWith('frame_')) fs.rmSync(path.join(dir, f));
  tutor([file, '--snapshot', path.join(dir, 'frame'), '--at', [...at.map((t) => t.toFixed(1)), 'end'].join(','), '--mute']);
  const frames = fs.readdirSync(dir).filter((f) => /^frame_.*\.png$/.test(f))
    .map((f) => ({ file: path.join(dir, f), at: f.slice(6, -4) }))
    .sort((a, b) => (a.at === 'end') - (b.at === 'end') || Number(a.at) - Number(b.at));
  const lines = scene.split('\n'); // the timeline counts lines from 0
  const narration = starts.map((e) => `${e.t.toFixed(1)}s  ${(lines[e.line] || '').trim()}`);
  const seconds = timeline.length ? timeline.at(-1).t : 0;
  return { scene, lints, computeProblems: problems, decodable: chk.status === 0, layout, frames, narration, seconds, says: says(scene).length };
}

const png = (file) => ({ type: 'image', source: { type: 'base64', media_type: 'image/png', data: fs.readFileSync(file).toString('base64') } });

async function review(m, part, ev, sources) {
  const findings = ev.layout ? [`layout score ${ev.layout.mean} (worst ${ev.layout.worst} at ${ev.layout.worstAt}s)`, ...(ev.layout.worstIssues || []).slice(0, 8)] : [];
  const content = [
    { type: 'text', text: `<plan_entry>\n${JSON.stringify(part, null, 2)}\n</plan_entry>\n\n<narration>\n${ev.narration.join('\n')}\n</narration>\n\n` +
      `<layout_checker>\n${findings.join('\n') || '(nothing)'}\n</layout_checker>\n\n<lints>\n${ev.lints.join('\n') || '(none)'}\n</lints>\n\nThe source:` },
    ...sources,
    { type: 'text', text: 'The frames:' },
  ];
  for (const f of ev.frames) content.push({ type: 'text', text: `frame at ${f.at === 'end' ? 'the end' : f.at + ' s'}` }, png(f.file));
  const r = await ask({ system: fs.readFileSync(path.join(HERE, 'critic-prompt.md'), 'utf8'), content, model: m.model, effort: m.criticEffort || 'medium', cwd: dirOf(m.id) });
  if (r.error) return { error: r.error, limit: r.limit };
  try { return JSON.parse(fence(r.text, 'review')); } catch { return { score: 0, problems: ['(the review did not parse)'], raw: r.text }; }
}

// How good a version is: the critic first, then fewer lints, then the layout.
const rank = (v) => (v.review?.score || 0) * 1000 - v.ev.lints.length * 10 + (v.ev.layout?.mean || 0) / 10;

export async function buildPart(m, outline, bible, n, { log = console.log } = {}) {
  const part = outline.parts[n - 1];
  const dir = workDir(m, n);
  const sources = partSources(m, part);
  const system = buildSystem() + fs.readFileSync(path.join(HERE, 'part-prompt.md'), 'utf8');
  const report = { part: n, title: part.title, rounds: [], started: new Date().toISOString() };
  let r = await ask({ system, content: [{ type: 'text', text: partContext(m, outline, bible, n) }, ...sources], model: m.model, effort: m.effort, cwd: dirOf(m.id) });
  const versions = [];
  let checkFixes = 0, criticFixes = 0;
  for (let round = 1; ; round++) {
    if (r.error) return { error: r.error, limit: r.limit };
    const reply = r.text, sid = r.sid;
    const ev = await evaluate(reply, path.join(dir, `v${round}`));
    fs.writeFileSync(path.join(dir, `v${round}`, 'reply.txt'), reply);
    const v = { round, reply, ev };
    versions.push(v);
    const blocking = [...ev.computeProblems, ...ev.lints];
    const note = `v${round}: ${ev.says} Say, ~${Math.round(ev.seconds)} s, ${ev.lints.length} lints, layout ${ev.layout?.mean ?? '-'}`;
    let ask_ = null;
    if (!ev.says) ask_ = 'The reply had no scene with Say lines. Write the whole part in the format.';
    else if (blocking.length && checkFixes < CHECK_ROUNDS) {
      checkFixes++;
      ask_ = `\`tutor --check\` on your part says:\n${blocking.join('\n')}\n\nFix every one and reply with the whole corrected part.`;
    } else {
      v.review = await review(m, part, ev, sources);
      if (v.review.error) return { error: v.review.error, limit: v.review.limit };
      writeJson(path.join(dir, `v${round}`, 'review.json'), v.review);
      log(`  ${note}, critic ${v.review.score}`);
      report.rounds.push({ ...summary(v) });
      if (v.review.score >= 4 || criticFixes >= CRITIC_ROUNDS) break;
      criticFixes++;
      ask_ = `A reviewer watched your part (frames and narration) and scored it ${v.review.score}/5.\nProblems:\n- ${(v.review.problems || []).join('\n- ')}\n` +
        `Keep:\n- ${(v.review.keep || []).join('\n- ') || '(nothing named)'}\n${ev.lints.length ? `\nLints still open:\n${ev.lints.join('\n')}\n` : ''}\nRewrite the whole part fixing these.`;
    }
    if (!v.review) { log(`  ${note}, fixing lints`); report.rounds.push(summary(v)); }
    r = await ask({ system, content: [{ type: 'text', text: ask_ }], model: m.model, effort: m.effort, cwd: dirOf(m.id), resume: sid });
  }
  const best = versions.filter((v) => v.ev.says).sort((a, b) => rank(b) - rank(a))[0];
  fs.writeFileSync(partFile(m, n), best.ev.scene.trim() + '\n');
  Object.assign(report, { kept: best.round, seconds: Math.round(best.ev.seconds), finished: new Date().toISOString() });
  writeJson(partFile(m, n).replace(/\.dan$/, '.report.json'), report);
  return { report };
}

const summary = (v) => ({ v: v.round, says: v.ev.says, seconds: Math.round(v.ev.seconds), lints: v.ev.lints, decodable: v.ev.decodable,
  layout: v.ev.layout && { mean: v.ev.layout.mean, worst: v.ev.layout.worst }, review: v.review && { score: v.review.score, problems: v.review.problems } });

// Build every part not built yet, in order. only: (re)build just these part numbers.
export async function buildLecture(m, { only, log = console.log } = {}) {
  const dir = dirOf(m.id);
  const outline = readJson(path.join(dir, 'outline.json'), null);
  const bible = fs.readFileSync(path.join(dir, 'bible.md'), 'utf8');
  if (!outline) throw new Error('no outline: plan it first');
  fs.mkdirSync(path.join(dir, 'parts'), { recursive: true });
  if (only) for (const n of only) fs.rmSync(partFile(m, n), { force: true });
  m.status = 'building'; m.error = undefined; saveMeta(m);
  for (let n = 1; n <= outline.parts.length; n++) {
    if (only ? !only.includes(n) : fs.existsSync(partFile(m, n))) continue;
    const t0 = Date.now();
    log(`part ${n}/${outline.parts.length}: ${outline.parts[n - 1].title}`);
    const r = await buildPart(m, outline, bible, n, { log });
    if (r.error) { m.status = r.limit ? 'limit' : 'failed'; m.error = `part ${n}: ${r.error}`; saveMeta(m); throw new Error(m.error); }
    log(`  kept v${r.report.kept}, ~${Math.round(r.report.seconds / 60 * 10) / 10} min, ${Math.round((Date.now() - t0) / 60000)} min to build`);
  }
  const done = outline.parts.every((_, i) => fs.existsSync(partFile(m, i + 1)));
  m.status = done ? 'built' : 'planned'; saveMeta(m);
  return done;
}
