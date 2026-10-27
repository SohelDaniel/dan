// A built lecture as one thing to play and one to print.
//   lecture.dan  the parts joined; each starts with its `scene "Part N: ..."`, so the stage starts fresh
//   index.json   each part's title, the line it starts on and its length (muted timing)
//   notes.md / notes.pdf  the companion on paper: written by Claude, printed by tools/notes/notes-pdf.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ask } from './claude.mjs';
import { dirOf, readJson, writeJson, saveMeta } from './store.mjs';
import { sourceBlocks } from './plan.mjs';
import { partFile } from './build.mjs';
import { notesPdf } from '../../tools/notes/notes-pdf.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));

// preview: join the parts built so far (from part 1 on, up to the first missing one), to watch while the rest builds
export function packageLecture(m, { preview = false } = {}) {
  const dir = dirOf(m.id);
  const outline = readJson(path.join(dir, 'outline.json'), null);
  const missing = outline.parts.map((_, i) => i + 1).filter((n) => !fs.existsSync(partFile(m, n)));
  if (missing.length && !preview) throw new Error(`parts not built yet: ${missing.join(', ')}`);
  if (missing[0] === 1) throw new Error('part 1 is not built yet');
  if (missing.length) outline.parts = outline.parts.slice(0, missing[0] - 1);
  const lines = [`# ${outline.title}`, `title "${outline.title.replace(/"/g, "'")}"`];
  const parts = outline.parts.map((p, i) => {
    const text = fs.readFileSync(partFile(m, i + 1), 'utf8').trimEnd();
    const report = readJson(partFile(m, i + 1).replace(/\.dan$/, '.report.json'), {});
    lines.push(`# --- part ${i + 1} ---`, ...text.split('\n'));
    const line = lines.length - text.split('\n').length + 1; // its first line (the scene line), counted from 1
    return { n: i + 1, title: p.title, line, seconds: report.seconds || 0, check: p.check };
  });
  fs.writeFileSync(path.join(dir, 'lecture.dan'), lines.join('\n') + '\n');
  writeJson(path.join(dir, 'index.json'), { title: outline.title, parts });
  if (!missing.length) { m.status = 'packaged'; saveMeta(m); }
  return parts;
}

const notesOf = (reply) => (reply.match(/<notes>\s*([\s\S]*?)\s*<\/notes>/) || [])[1] || '';

// Notes from the sources, the plan and what each part says; a diagram or formula that fails to print goes back
// for one fix.
export async function writeNotes(m, { log = console.log } = {}) {
  const dir = dirOf(m.id);
  const outline = readJson(path.join(dir, 'outline.json'), null);
  const bible = fs.readFileSync(path.join(dir, 'bible.md'), 'utf8');
  const said = outline.parts.map((p, i) => {
    const f = partFile(m, i + 1);
    const says = fs.existsSync(f) ? fs.readFileSync(f, 'utf8').split('\n').filter((l) => /^\s*Say:/.test(l)).map((l) => l.replace(/^\s*Say:\s*/, '')) : [];
    return `## Part ${i + 1}: ${p.title}\n${says.join(' ') || '(not built: write from the plan)'}`;
  }).join('\n\n');
  const content = [...sourceBlocks(m), { type: 'text', text:
    `<outline>\n${JSON.stringify(outline, null, 2)}\n</outline>\n\n<bible>\n${bible}\n</bible>\n\n<narration>\n${said}\n</narration>\n\nWrite the notes.` }];
  const system = fs.readFileSync(path.join(HERE, 'notes-prompt.md'), 'utf8');
  log('writing notes...');
  let r = await ask({ system, content, model: m.model, effort: m.effort, cwd: dir });
  if (r.error) throw Object.assign(new Error(r.error), { limit: r.limit });
  const md = path.join(dir, 'notes.md'), pdf = path.join(dir, 'notes.pdf');
  fs.writeFileSync(md, notesOf(r.text) + '\n');
  let { problems } = await notesPdf(md, pdf);
  if (problems.length) {
    log(`  ${problems.length} problems printing, one fix: ${problems.join('; ')}`);
    r = await ask({ system, content: [{ type: 'text', text: `Printing the notes found:\n${problems.join('\n')}\nReply with the whole corrected notes.` }], model: m.model, effort: m.effort, cwd: dir, resume: r.sid });
    if (!r.error && notesOf(r.text)) {
      fs.writeFileSync(md, notesOf(r.text) + '\n');
      ({ problems } = await notesPdf(md, pdf));
    }
  }
  m.notes = { problems }; saveMeta(m);
  return { md, pdf, problems };
}
