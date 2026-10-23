// Mining 3Blue1Brown's step comments into an idiom library for the tutor (see tools/mine/README.md).
//
//   node tools/mine/mine.mjs taxonomy     propose the kinds of visual move from two samples of comments, then merge
//   node tools/mine/mine.mjs label        label every usable block with one kind (batches; cached);
//                                        --model opus --out labels-opus.jsonl gives the second, independent labeller
//   node tools/mine/mine.mjs audit        a second, independent labelling of a sample: agreement per kind
//   node tools/mine/mine.mjs select       3-5 varied blocks per kept kind
//   node tools/mine/mine.mjs translate    each selected block -> an idiom in our markup, checked and repaired once
//   node tools/mine/mine.mjs judge        render each idiom and score it with a vision judge
//   node tools/mine/mine.mjs assemble     the best idiom per kind -> chat/idioms.md
//
// Every model call is cached in work/cache by a hash of its input, so a rerun is free and gives the same result.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const WORK = path.join(HERE, 'work');
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const stage = argv[0];
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const jobs = Number(flag('--jobs', 4));
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));
fs.mkdirSync(path.join(WORK, 'cache'), { recursive: true });

// ---------------------------------------------------------------- shared
const readJsonl = (f) => fs.readFileSync(f, 'utf8').split('\n').filter(Boolean).map((l) => JSON.parse(l));
const writeJsonl = (f, rows) => fs.writeFileSync(f, rows.map((r) => JSON.stringify(r)).join('\n') + '\n');
const writeJson = (f, v) => fs.writeFileSync(f, JSON.stringify(v, null, 1));
const readJson = (f) => JSON.parse(fs.readFileSync(f, 'utf8'));

// a seeded shuffle, so samples are the same on every run
function shuffled(xs, seed = 1) {
  const a = [...xs];
  let s = seed;
  const rnd = () => ((s = (s * 1103515245 + 12345) % 2147483648) / 2147483648);
  for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(rnd() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; }
  return a;
}

// one Claude call, cached by (model, system, user, tools)
function claude({ model = 'sonnet', system = '', user, tools = '', addDir = null }) {
  const key = crypto.createHash('sha256').update(JSON.stringify([model, system, user, tools, addDir ? 'dir' : ''])).digest('hex').slice(0, 24);
  const file = path.join(WORK, 'cache', key + '.json');
  if (fs.existsSync(file)) return Promise.resolve(readJson(file).text);
  return new Promise((resolve) => {
    const args = ['-p', user, '--output-format', 'json', '--model', model, '--effort', 'low', '--strict-mcp-config', '--tools', tools];
    if (tools) args.push('--allowedTools', tools);
    if (addDir) args.push('--add-dir', addDir);
    if (system) args.push('--system-prompt', system);
    const c = spawn('claude', args, { env: ENV, cwd: addDir || WORK });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let text = '', error = false;
      try { const j = JSON.parse(out); text = j.result || ''; error = !!j.is_error; } catch { text = out; error = true; }
      // a failed call (an error, a session or usage limit) is never cached, and stops the run rather than poisoning it
      if (error || /hit your (session|usage) limit|usage limit reached/i.test(text.slice(0, 300))) {
        console.error(`model call failed: ${text.slice(0, 160)}`);
        process.exit(2);
      }
      if (text) writeJson(file, { model, text });
      resolve(text);
    });
  });
}

const firstJson = (s) => { const m = s.match(/[[{][\s\S]*[\]}]/); try { return JSON.parse(m[0]); } catch { return null; } };
async function pool(items, fn, n = jobs) {
  const q = items.map((x, i) => [x, i]), out = new Array(items.length);
  await Promise.all(Array.from({ length: n }, async () => { while (q.length) { const [x, i] = q.shift(); out[i] = await fn(x, i); } }));
  return out;
}

// the blocks worth mining: a prose step comment over code that plays something our markup can express
function usable() {
  return readJsonl(path.join(WORK, 'blocks.jsonl'))
    .filter((b) => !b.flags.length && b.plays > 0 && Number(b.year) >= 2019)
    .map((b) => ({ ...b, id: `${b.file}:${b.line}` }));
}

// ---------------------------------------------------------------- taxonomy
const TAXONOMY_ASK = (comments) => `These are step comments 3Blue1Brown wrote above blocks of animation code in his videos.
Each says, in his words, what the next animation shows. We are building a library of reusable *visual moves* for a
live AI tutor that explains any topic (maths, science, computing) by speaking while an engine animates a picture.

Propose a taxonomy of the kinds of visual move these comments describe: the communicative move (highlight a part,
count things one by one, transform one formula into another, zoom in on a detail, compare two quantities side by side,
slide something along, label a thing...), not the topic. Rules:
- 25 to 45 kinds, each a short snake_case name, a one-sentence definition, and 2 example comments from the list.
- Kinds must be mutually exclusive enough that a reader can pick one for most comments; add "setup" (just
  establishing objects, no move) and "other".
- Don't make kinds for things a tutor can't use (Pi creature reactions, channel logos, end screens).

Reply with only JSON: {"kinds":[{"name":"…","definition":"…","examples":["…","…"]}]}

Comments:
${comments.map((c, i) => `${i + 1}. ${c}`).join('\n')}`;

const MERGE_ASK = (a, b) => `Two taxonomies of visual moves, proposed from two different samples of 3Blue1Brown's step comments.
Merge them into one: 30 to 45 kinds, mutually exclusive, each with a snake_case name, a one-sentence definition that
says how to tell it apart from its nearest neighbour, and 2 examples. Keep "setup" and "other". Reply with only JSON
{"kinds":[…]} in the same shape.

A: ${JSON.stringify(a)}

B: ${JSON.stringify(b)}`;

async function taxonomy() {
  const blocks = shuffled(usable(), 7);
  const sample = (from) => blocks.slice(from, from + 300).map((b) => b.comment.slice(0, 160));
  const [a, b] = await Promise.all([claude({ user: TAXONOMY_ASK(sample(0)) }), claude({ user: TAXONOMY_ASK(sample(300)) })]);
  const ta = firstJson(a), tb = firstJson(b);
  const merged = firstJson(await claude({ user: MERGE_ASK(ta, tb) }));
  writeJson(path.join(WORK, 'taxonomy-proposed.json'), merged); // hand-edited into tools/mine/taxonomy.json
  console.log(`${ta?.kinds?.length} + ${tb?.kinds?.length} kinds proposed, merged into ${merged?.kinds?.length}:`);
  for (const k of merged.kinds) console.log(`  ${k.name}: ${k.definition}`);
}

// ---------------------------------------------------------------- label
const LABEL_ASK = (kinds, batch) => `Label each 3Blue1Brown step comment with the ONE kind of visual move it describes. Read the
animation calls and the code too: they show what actually happens, and decide when the comment is terse. Use "other" when nothing fits and "setup" for establishing objects
with no move.

Kinds:
${kinds.map((k) => `- ${k.name}: ${k.definition}`).join('\n')}

Reply with only JSON: {"labels":[{"i":1,"kind":"…"}, …]} with one entry per comment.

${batch.map((b, i) => `${i + 1}. "${b.comment.slice(0, 200)}"\n   plays: ${b.calls.flat().slice(0, 8).join(', ')}\n   code: ${codeGist(b.code)}`).join('\n')}`;

// the first lines of the block's code, squeezed: enough to see what the comment's move really is
const codeGist = (code) => code.split('\n').map((l) => l.trim()).filter((l) => l && !l.startsWith('#')).slice(0, 10).join(' ⏎ ').slice(0, 500);

async function labelAll(model, blocks, file) {
  const kinds = readJson(path.join(HERE, 'taxonomy.json')).kinds;
  const names = new Set(kinds.map((k) => k.name));
  const batches = [];
  for (let i = 0; i < blocks.length; i += 25) batches.push(blocks.slice(i, i + 25));
  const res = await pool(batches, async (batch) => {
    const j = firstJson(await claude({ model, user: LABEL_ASK(kinds, batch) }));
    return batch.map((b, i) => {
      const k = j?.labels?.find((l) => l.i === i + 1)?.kind;
      return { id: b.id, kind: names.has(k) ? k : 'unlabelled' };
    });
  });
  const rows = res.flat();
  writeJsonl(file, rows);
  return rows;
}

async function label() {
  const rows = await labelAll(flag('--model', 'sonnet'), usable(), path.join(WORK, flag('--out', 'labels.jsonl')));
  const n = {};
  for (const r of rows) n[r.kind] = (n[r.kind] || 0) + 1;
  console.log(`${rows.length} blocks labelled`);
  for (const [k, v] of Object.entries(n).sort((a, b) => b[1] - a[1])) console.log(`  ${String(v).padStart(5)}  ${k}`);
}

// ---------------------------------------------------------------- audit: a second labeller on a sample, agreement
async function audit() {
  const labels = new Map(readJsonl(path.join(WORK, 'labels.jsonl')).map((r) => [r.id, r.kind]));
  const sample = shuffled(usable(), 11).slice(0, Number(flag('--n', 400)));
  const second = await labelAll(flag('--model', 'opus'), sample, path.join(WORK, 'labels-audit.jsonl'));
  let agree = 0;
  const per = {}, kinds = new Set();
  for (const r of second) {
    const a = labels.get(r.id), b = r.kind;
    kinds.add(a); kinds.add(b);
    if (a === b) agree++;
    per[a] = per[a] || { n: 0, same: 0 };
    per[a].n++;
    if (a === b) per[a].same++;
  }
  // Cohen's kappa: agreement beyond what the two label distributions give by chance
  const pa = {}, pb = {};
  for (const r of second) { pa[labels.get(r.id)] = (pa[labels.get(r.id)] || 0) + 1; pb[r.kind] = (pb[r.kind] || 0) + 1; }
  const N = second.length, po = agree / N;
  let pe = 0;
  for (const k of kinds) pe += ((pa[k] || 0) / N) * ((pb[k] || 0) / N);
  const kappa = (po - pe) / (1 - pe);
  const report = { n: N, agreement: po, kappa, perKind: per };
  writeJson(path.join(WORK, 'audit.json'), report);
  console.log(`agreement ${(po * 100).toFixed(1)}%, Cohen's kappa ${kappa.toFixed(2)} over ${N} blocks`);
  for (const [k, v] of Object.entries(per).sort((a, b) => b[1].n - a[1].n)) console.log(`  ${k.padEnd(28)} ${v.same}/${v.n}`);
}

// ---------------------------------------------------------------- select: varied candidates per kind
// The pool is the blocks both labellers agree on. Per kind: the most typical block first (the medoid by code
// similarity), then each next pick the one least like those already picked (k-center), one per video, no near-duplicates.
const tokens = (code) => new Set((code.match(/[A-Za-z_]\w+/g) || []).filter((t) => t.length > 2));
const jaccard = (a, b) => { let n = 0; for (const t of a) if (b.has(t)) n++; return n / (a.size + b.size - n || 1); };
const video = (id) => id.split('/').slice(0, 2).join('/');

function select() {
  const per = Number(flag('--per', 6));
  const a = new Map(readJsonl(path.join(WORK, 'labels.jsonl')).map((r) => [r.id, r.kind]));
  const b = new Map(readJsonl(path.join(WORK, 'labels-opus.jsonl')).map((r) => [r.id, r.kind]));
  const pool = usable().filter((x) => a.get(x.id) === b.get(x.id) && !['setup', 'other', 'unlabelled'].includes(a.get(x.id)))
    .filter((x) => x.comment.split(/\s+/).length >= 3) // a comment that says something
    .map((x) => ({ ...x, kind: a.get(x.id), toks: tokens(x.code), lines: x.code.split('\n').filter((l) => l.trim()).length }))
    .filter((x) => x.lines >= 3 && x.lines <= 30);
  const byKind = {};
  for (const x of pool) (byKind[x.kind] = byKind[x.kind] || []).push(x);
  const out = [];
  for (const [kind, xs] of Object.entries(byKind)) {
    const sim = (p, q) => jaccard(p.toks, q.toks);
    const medoid = xs.map((x) => [x, xs.reduce((s, y) => s + sim(x, y), 0)]).sort((p, q) => q[1] - p[1])[0][0];
    const picked = [medoid];
    while (picked.length < per) {
      const seen = new Set(picked.map((p) => video(p.id)));
      const cand = xs.filter((x) => !picked.includes(x) && !seen.has(video(x.id)) && picked.every((p) => sim(x, p) < 0.8));
      if (!cand.length) break;
      cand.sort((p, q) => Math.min(...picked.map((r) => 1 - sim(q, r))) - Math.min(...picked.map((r) => 1 - sim(p, r))));
      picked.push(cand[0]);
    }
    for (const p of picked) out.push({ id: p.id, kind, year: p.year, cls: p.cls, comment: p.comment, code: p.code });
    console.log(`${kind.padEnd(24)} ${String(xs.length).padStart(4)} agreed, ${picked.length} picked`);
  }
  writeJsonl(path.join(WORK, 'selected.jsonl'), out);
  console.log(`${out.length} candidates`);
}

// ---------------------------------------------------------------- translate: a block -> an idiom in our markup
const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
const LANG = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
const AXIOMS = readme.slice(readme.indexOf('## The dictionary'), readme.indexOf('## raymanim directly'));
let EXEMPLARS = '';
for (const f of fs.readdirSync(path.join(ROOT, 'chat/examples')).filter((f) => f.endsWith('.dan')).sort())
  EXEMPLARS += `\n### ${f.slice(0, -4)}\n\`\`\`\n${fs.readFileSync(path.join(ROOT, 'chat/examples', f), 'utf8').trim()}\n\`\`\`\n`;

const TRANSLATE_SYSTEM = `You turn one move from 3Blue1Brown's animation code (Manim) into a reusable *idiom* for a live
tutor engine that plays a small declarative markup. An idiom is a tiny, self-contained, playable scene: a few set-up
lines, then one or two \`Say:\` lines whose cued lines perform the move. A tutor will later copy the pattern for other
topics, so keep it general and short (at most ~12 lines), on a simple concrete example.

Hard rules (the engine's axioms and founding rule):
- Never give positions for layout: no coordinates outside an \`axes\`/\`plane\` schema's world, no region/below/near/left-of
  words, no pos=. Structure is layout: links, groups, schema order. \`at=\` points at a thing; \`on=/in=/over=\` attach.
- There is no delete: re-mark updates in place, \`hide\` dims, a new \`scene\` clears.
- Colour means something: same colour = same kind/role; never decorative.
- Never put the narration's sentence on stage; labels, numbers, formulas only.
- Every \`|cue|\` in a Say has an \`@cue\` line and every \`@cue\` line has its \`|cue|\`. The move happens on the word it is about.
- Plain spoken numbers in Say (no {expr} references).
If the move can't be expressed in this language, reply exactly: NOT EXPRESSIBLE: <what is missing>.

Reply with only JSON: {"title":"<the move, 2-5 words>","when":"<one line: when a tutor should use this>","scene":"<the markup, \\n-separated>"}

${LANG}

${AXIOMS}

## Gold-standard scenes (style)
${EXEMPLARS}`;

const run = (f, ...a) => spawnSync(TUTOR, [f, ...a], { cwd: ROOT, encoding: 'utf8' });
// every warning the engine gives, except layout notes: an idiom must be clean
function problems(file) {
  const r = run(file, '--check');
  const out = (r.stdout || '') + (r.stderr || '');
  const bad = out.split('\n').filter((l) => /^(lint|parse):/.test(l) && !/layout:/.test(l));
  if (!/decodable: yes/.test(out)) bad.push('not decodable: the readback does not rebuild the same scene');
  if (/\b(pos|region|below|above|left-of|right-of|near|align)=/.test(fs.readFileSync(file, 'utf8'))) bad.push('uses a retired placement word');
  return [...new Set(bad)];
}

async function translate() {
  const sel = readJsonl(path.join(WORK, 'selected.jsonl'));
  const kinds = new Map(readJson(path.join(HERE, 'taxonomy.json')).kinds.map((k) => [k.name, k.definition]));
  const dir = path.join(WORK, 'idioms');
  fs.mkdirSync(dir, { recursive: true });
  const res = await pool(sel, async (s, n) => {
    const name = `${s.kind}-${n}`;
    let ask = `The move (${s.kind}: ${kinds.get(s.kind)})\n3Blue1Brown's comment: "${s.comment}"\nHis code (${s.id}, scene ${s.cls}):\n\`\`\`python\n${s.code}\n\`\`\``;
    let last = null, why = 'no reply';
    for (let round = 0; round < 3; round++) {
      const text = await claude({ system: TRANSLATE_SYSTEM, user: ask });
      if (/NOT EXPRESSIBLE/i.test(text) && !text.includes('"scene"')) { why = text.trim().slice(0, 200); last = null; break; }
      const j = firstJson(text);
      if (!j?.scene) { why = 'no JSON'; ask += '\n\nReply with only the JSON object.'; continue; }
      const f = path.join(dir, name + '.dan');
      fs.writeFileSync(f, j.scene.trim() + '\n');
      const bad = problems(f);
      last = { ...j, file: f, problems: bad, rounds: round + 1 };
      if (!bad.length) { why = 'ok'; break; }
      why = 'problems';
      ask += `\n\nYour scene:\n${j.scene}\n\nThe engine reported:\n${bad.join('\n')}\nFix them and reply with the corrected JSON.`;
    }
    const row = { ...s, name, status: why === 'ok' ? 'ok' : why === 'problems' ? 'rejected' : 'not-expressible', why, idiom: last };
    console.log(`${name.padEnd(30)} ${row.status}${last ? ` (${last.rounds} round${last.rounds > 1 ? 's' : ''})` : ''}`);
    return row;
  });
  writeJsonl(path.join(WORK, 'translated.jsonl'), res);
  const n = (st) => res.filter((r) => r.status === st).length;
  console.log(`${res.length} translated: ${n('ok')} clean, ${n('rejected')} still with problems, ${n('not-expressible')} not expressible`);
}

// ---------------------------------------------------------------- judge: frames at the end of each sentence, pass/fail
const JUDGE = (row, frames) => `You check one *idiom* for an AI visual tutor: a tiny scene meant to teach a reusable visual move,
translated from 3Blue1Brown's animation code. Read every frame with the Read tool first.

The move: ${row.kind}. 3Blue1Brown's own comment on it: "${row.comment}".
The idiom: "${row.idiom.title}", for when ${row.idiom.when}
Its markup:
\`\`\`
${fs.readFileSync(row.idiom.file, 'utf8').trim()}
\`\`\`
Frames, each taken as a sentence ends (the last is the end of the scene):
${frames.map((f) => `- ${f.label}: ${f.file}`).join('\n')}

Answer each question yes or no, strictly:
- faithful: the frames visibly perform the move the comment describes (not just something near it).
- legible: nothing overlaps, nothing is cut off, text is readable, the picture is uncluttered.
- shown: what each sentence says is visible on stage when it ends.
- reusable: the same few lines would work as a pattern on other topics (not tied to one odd setup).
Reply with only JSON: {"faithful":"yes|no","legible":"yes|no","shown":"yes|no","reusable":"yes|no","notes":"<= 25 words"}`;

function sentenceFrames(file, out) {
  fs.rmSync(out, { recursive: true, force: true });
  fs.mkdirSync(out, { recursive: true });
  const v = spawnSync(TUTOR, [file, '--snapshot', path.join(out, 'x'), '--at', 'end', '--verbose'], { cwd: ROOT, encoding: 'utf8' });
  const says = [...(v.stderr || '').matchAll(/^\[\s*([\d.]+)s\] line \d+: Say:/gm)].map((m) => +m[1]);
  const at = says.map((t, i) => (says[i + 1] ? Math.max(t + 0.5, says[i + 1] - 0.15).toFixed(2) : 'end'));
  if (!at.length) at.push('end');
  for (let a = 0; a < 3 && !fs.readdirSync(out).some((f) => f.startsWith('f_')); a++)
    spawnSync(TUTOR, [file, '--snapshot', path.join(out, 'f'), '--at', at.join(',')], { cwd: ROOT });
  return at.map((t, i) => ({ label: t === 'end' ? `sentence ${i + 1} (end)` : `sentence ${i + 1}`, file: path.join(out, `f_${t}.png`) }))
    .filter((f) => fs.existsSync(f.file));
}

async function judge() {
  const rows = readJsonl(path.join(WORK, 'translated.jsonl')).filter((r) => r.status === 'ok');
  const res = await pool(rows, async (r) => {
    const frames = sentenceFrames(r.idiom.file, path.join(WORK, 'frames', r.name));
    if (!frames.length) return { ...r, verdict: { error: 'no frames' }, pass: false };
    const v = firstJson(await claude({ user: JUDGE(r, frames), tools: 'Read', addDir: WORK })) || { error: 'no JSON' };
    const pass = ['faithful', 'legible', 'shown', 'reusable'].every((k) => v[k] === 'yes');
    console.log(`${r.name.padEnd(30)} ${pass ? 'PASS' : 'fail'}  ${['faithful', 'legible', 'shown', 'reusable'].map((k) => k[0] + ':' + (v[k] || '?')[0]).join(' ')}  ${v.notes || v.error || ''}`);
    return { ...r, frames: frames.map((f) => f.file), verdict: v, pass };
  });
  writeJsonl(path.join(WORK, 'judged.jsonl'), res);
  console.log(`${res.filter((r) => r.pass).length} of ${res.length} pass every check`);
}

// ---------------------------------------------------------------- assemble: the kept idioms -> chat/idioms.md
function assemble() {
  const per = Number(flag('--per', 2)), cap = Number(flag('--cap', 50));
  const rows = readJsonl(path.join(WORK, 'judged.jsonl')).filter((r) => r.pass && (!fs.existsSync(path.join(HERE, 'rejected.txt')) ||
    !fs.readFileSync(path.join(HERE, 'rejected.txt'), 'utf8').split('\n').includes(r.name))); // reviewed by hand: tools/mine/rejected.txt
  const taxonomy = readJson(path.join(HERE, 'taxonomy.json')).kinds.map((k) => k.name);
  const lines = (r) => fs.readFileSync(r.idiom.file, 'utf8').trim().split('\n').length;
  // one per kind first, then second picks for the kinds 3Blue1Brown uses most, up to the cap
  const freq = {};
  for (const r of readJsonl(path.join(WORK, 'labels.jsonl'))) freq[r.kind] = (freq[r.kind] || 0) + 1;
  const byKind = Object.fromEntries(taxonomy.map((k) => [k, rows.filter((r) => r.kind === k).sort((a, b) => lines(a) - lines(b))]));
  const picked = Object.fromEntries(taxonomy.map((k) => [k, []]));
  for (let round = 0; round < per; round++)
    for (const kind of [...taxonomy].sort((a, b) => (freq[b] || 0) - (freq[a] || 0))) {
      if (Object.values(picked).flat().length >= cap) break;
      const x = byKind[kind].find((x) => !picked[kind].includes(x) && !picked[kind].some((m) => video(m.id) === video(x.id)));
      if (x) picked[kind].push(x);
    }
  const out = taxonomy.flatMap((k) => picked[k]); // grouped by kind, in the taxonomy's order
  let md = `## Visual moves, from 3Blue1Brown

Each is a move 3Blue1Brown uses, written in this language on a small example: when to use it, then the pattern.
Copy the *move* onto your topic (its things, words and numbers), not the example. Built by tools/mine from his step
comments (source file:line under each).
`;
  for (const r of out) {
    md += `\n### ${r.idiom.title}\nUse: ${r.idiom.when.replace(/^use (this |it )?/i, '')}\n\`\`\`\n${fs.readFileSync(r.idiom.file, 'utf8').trim()}\n\`\`\`\n` +
      `<!-- ${r.kind} · 3b1b ${r.id} "${r.comment.slice(0, 80)}" -->\n`;
  }
  fs.writeFileSync(path.join(ROOT, 'chat', 'idioms.md'), md);
  // each idiom also as a script, so make check / golden cover it
  const sdir = path.join(ROOT, 'chat', 'idioms');
  fs.rmSync(sdir, { recursive: true, force: true });
  fs.mkdirSync(sdir);
  for (const r of out) fs.copyFileSync(r.idiom.file, path.join(sdir, r.name.replace(/-\d+$/, '') + '-' + r.name.split('-').pop() + '.dan'));
  writeJsonl(path.join(HERE, 'idioms.jsonl'), out.map((r) => ({ name: r.name, kind: r.kind, source: r.id, year: r.year, comment: r.comment,
    title: r.idiom.title, when: r.idiom.when, rounds: r.idiom.rounds, verdict: r.verdict })));
  console.log(`${out.length} idioms from ${new Set(out.map((r) => r.kind)).size} kinds -> chat/idioms.md (${md.length} chars)`);
}

const stages = { taxonomy, label, audit, select, translate, judge, assemble };
if (!stages[stage]) { console.error('stages: ' + Object.keys(stages).join(' ')); process.exit(1); }
await stages[stage]();
