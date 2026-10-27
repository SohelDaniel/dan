// A lecture on disk: chat/lectures/<id>/ (the learner's own, gitignored).
//   lecture.json     sources, target minutes, model, status of each step
//   sources/         copies of the files it teaches from
//   outline.json     the plan: parts, each with goal, frame, beats, check   (plan.mjs)
//   bible.md         notation, names, running examples, colour meanings     (plan.mjs)
//   parts/NN.dan     each part as built, NN.report.json how it was checked  (build.mjs)
//   lecture.dan      the parts joined, index.json where each starts         (package.mjs)
//   notes.md, notes.pdf  the companion to follow on paper                   (notes.mjs)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { kindOf, textOf, pageCount, wordCount } from '../ingest.mjs';

export const LECTURES = process.env.TUTOR_LECTURES || path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', 'lectures'); // tests use their own

const slug = (s) => s.toLowerCase().replace(/\.[a-z0-9]+$/, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 40);
const stamp = () => new Date().toISOString().replace(/[-:]/g, '').replace('T', '-').slice(0, 15);

export const dirOf = (id) => path.join(LECTURES, id);
export const readJson = (f, dflt) => { try { return JSON.parse(fs.readFileSync(f, 'utf8')); } catch { return dflt; } };
export const writeJson = (f, v) => fs.writeFileSync(f, JSON.stringify(v, null, 2) + '\n');
export const meta = (id) => readJson(path.join(dirOf(id), 'lecture.json'), null);
export const saveMeta = (m) => writeJson(path.join(dirOf(m.id), 'lecture.json'), m);

// A new lecture from files: copied in, measured (words, pages) for the length estimate.
export function create(files, { minutes, request = '', model = 'opus', effort = 'high' } = {}) {
  const id = `${stamp()}-${slug(path.basename(files[0])) || 'lecture'}`;
  const dir = dirOf(id);
  fs.mkdirSync(path.join(dir, 'sources'), { recursive: true });
  const sources = files.map((f) => {
    const name = path.basename(f);
    const to = path.join(dir, 'sources', name);
    fs.copyFileSync(f, to);
    const file = { name, path: to };
    const kind = kindOf(name);
    return { name, kind, words: kind === 'image' ? 0 : wordCount(textOf(file)), pages: kind === 'pdf' ? pageCount(file) : 0 };
  });
  const m = { id, created: new Date().toISOString(), sources, minutes, request, model, effort, status: 'new' };
  saveMeta(m);
  return m;
}

export const sourceFiles = (m) => m.sources.map((s) => ({ ...s, path: path.join(dirOf(m.id), 'sources', s.name) }));

export function list() {
  if (!fs.existsSync(LECTURES)) return [];
  return fs.readdirSync(LECTURES).sort().reverse().map(meta).filter(Boolean);
}

// An id, or the start of one ("20261027"), or a slug fragment: the newest match.
export function find(q) {
  const all = list();
  return (q ? all.find((m) => m.id === q) || all.find((m) => m.id.includes(q)) : all[0]) || null;
}
