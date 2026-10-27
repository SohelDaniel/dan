// Planning a lecture: one call reads every source and writes the outline (parts, each with a goal, the frame, the
// beats, a check) and the bible (what all parts must agree on). The learner reads and approves it before the build.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildSystem } from '../system-prompt.mjs';
import { blocksFor, textOf, suggestMinutes } from '../ingest.mjs';
import { ask, fence } from './claude.mjs';
import { dirOf, saveMeta, sourceFiles, writeJson } from './store.mjs';

const PROMPT = path.join(path.dirname(fileURLToPath(import.meta.url)), 'plan-prompt.md');
const PDF_PAGES = 100; // above this Claude reads the PDF's text (with page marks) instead of its pages

export function targetMinutes(m) {
  return m.minutes || suggestMinutes(m.sources.reduce((n, s) => n + s.words, 0), m.sources.filter((s) => s.kind === 'image').length);
}

export function sourceBlocks(m) {
  const blocks = [];
  for (const f of sourceFiles(m)) {
    blocks.push({ type: 'text', text: `Source: ${f.name}${f.pages ? ` (${f.pages} pages)` : ''}` });
    blocks.push(...(f.kind === 'pdf' && f.pages > PDF_PAGES
      ? [{ type: 'text', text: `<file name="${f.name}">\n${textOf(f)}\n</file>` }]
      : blocksFor(f)));
  }
  return blocks;
}

// A plan the build can rely on: parts, each with a title, a goal, minutes and a check.
export function validOutline(o) {
  const bad = [];
  if (!o || !Array.isArray(o.parts) || !o.parts.length) return ['no parts'];
  o.parts.forEach((p, i) => {
    for (const k of ['title', 'goal', 'frame']) if (!p[k]) bad.push(`part ${i + 1}: no ${k}`);
    if (!(p.minutes > 0)) bad.push(`part ${i + 1}: no minutes`);
    if (!p.check?.question || !p.check?.answer) bad.push(`part ${i + 1}: no check question and answer`);
    for (const n of p.needs || []) if (!(n >= 1 && n <= i)) bad.push(`part ${i + 1} needs part ${n}, which does not come before it`);
  });
  return bad;
}

export async function planLecture(m, { log = console.log } = {}) {
  const dir = dirOf(m.id);
  const minutes = targetMinutes(m);
  const system = buildSystem() + fs.readFileSync(PROMPT, 'utf8')
    .replaceAll('{{MINUTES}}', String(minutes))
    .replace('{{REQUEST}}', m.request ? `What the learner asked for: ${m.request}` : '');
  let content = [...sourceBlocks(m), { type: 'text', text: `Plan the lecture (about ${minutes} minutes) from these sources.` }];
  m.status = 'planning'; saveMeta(m);
  for (let attempt = 1; attempt <= 2; attempt++) {
    log(`planning (${minutes} min target, ${m.model}/${m.effort}, attempt ${attempt})...`);
    const r = await ask({ system, content, model: m.model, effort: m.effort, cwd: dir });
    if (r.error) { m.status = r.limit ? 'limit' : 'failed'; m.error = r.error; saveMeta(m); throw new Error(r.error); }
    fs.writeFileSync(path.join(dir, 'plan-reply.txt'), r.text);
    let outline = null, problems;
    try { outline = JSON.parse(fence(r.text, 'outline')); problems = validOutline(outline); }
    catch (e) { problems = [`the outline is not valid JSON: ${e.message}`]; }
    const bible = fence(r.text, 'bible');
    if (!bible) problems.push('no bible');
    if (!problems.length) {
      writeJson(path.join(dir, 'outline.json'), outline);
      fs.writeFileSync(path.join(dir, 'bible.md'), bible.trim() + '\n');
      Object.assign(m, { status: 'planned', planSecs: Math.round(r.secs), error: undefined });
      saveMeta(m);
      return outline;
    }
    log(`  plan problems: ${problems.join('; ')}`);
    content = [...content, { type: 'text', text: `Your previous reply had problems: ${problems.join('; ')}. Here it was:\n\n${r.text}\n\nReply again, complete and in the format.` }];
  }
  m.status = 'failed'; m.error = 'no valid plan after 2 attempts'; saveMeta(m);
  throw new Error(m.error);
}

// The outline as the learner reads it.
export function outlineText(o) {
  const total = o.parts.reduce((n, p) => n + p.minutes, 0);
  const lines = [`${o.title}  (${o.parts.length} parts, ${total} min, level: ${o.level})`];
  if (o.note) lines.push(`note: ${o.note}`);
  o.parts.forEach((p, i) => {
    lines.push('', `${i + 1}. ${p.title}  [${p.minutes} min]${p.needs?.length ? `  needs ${p.needs.join(', ')}` : ''}`,
      `   goal:  ${p.goal}`, `   frame: ${p.frame}`, `   from:  ${p.source || '-'}`, `   check: ${p.check.question}  -> ${p.check.answer}`);
  });
  return lines.join('\n');
}
