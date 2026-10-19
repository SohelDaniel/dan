// Score generated scenes with a Claude judge that reads the script and looks at the rendered frames.
//
//   node tools/eval/judge.mjs --out DIR [--variants base,method,full] [--model sonnet] [--jobs 4]
//
// Writes DIR/<variant>/<id>/score.json and prints a table of mean scores per variant.
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const OUT = path.resolve(flag('--out', path.join(HERE, 'out')));
const variants = flag('--variants', 'base,method,full').split(',');
const model = flag('--model', 'sonnet');
const jobs = Number(flag('--jobs', 4));
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));
const rubricName = flag('--rubric', 'v1');
const KEYS = rubricName === 'v2'
  ? ['fit', 'binding', 'natural', 'picture', 'layout', 'accuracy', 'teaching']
  : ['representation', 'concrete_first', 'key_idea_on_stage', 'sync_pointing', 'check_question', 'layout', 'accuracy', 'teaching'];

const RUBRIC = `You are an expert teacher and visual designer judging one answer from an AI tutor.
The tutor answers by speaking (every \`Say:\` line is narrated) while an engine animates the markup;
\`|cue|\` markers fire the \`@cue\` lines on that word. Judge what a learner would experience.

Score each criterion 1-5 (5 = excellent):
- representation: the picture shows the mechanism itself, well chosen for this concept (3Blue1Brown-like), not a generic chart. Bars are only right for comparing quantities.
- concrete_first: starts from a concrete case; the learner sees the pattern before it is named or formalised.
- key_idea_on_stage: the central formula or idea is visible on stage by the end (judge the last frame).
- sync_pointing: changes are cued on the words they are about; it points (indicate/focus) at what it talks about.
- check_question: ends with a genuine prediction/application question, not "any questions?".
- layout: frames are legible, uncluttered, nothing overlapping or cut off (judge the frames).
- accuracy: everything said and shown is correct.
- teaching: overall, would a learner come away understanding the idea?

Reply with ONLY a JSON object: {"representation":n,"concrete_first":n,"key_idea_on_stage":n,"sync_pointing":n,"check_question":n,"layout":n,"accuracy":n,"teaching":n,"notes":"<= 40 words: the biggest weakness"}`;

// v2: judges the decision (does the answer's shape fit the question?) and the binding of words to picture
const RUBRIC_V2 = `You are an expert teacher and visual designer judging one turn of an AI visual tutor (a conversational
assistant, not a video). The tutor speaks (every \`Say:\` line is narrated) while an engine animates the markup;
\`|cue|\` markers fire the \`@cue\` lines on that word. Judge what this learner would experience.

Score each criterion 1-5 (5 = excellent):
- fit: the answer's shape and length fit THIS question and learner. A quick fact gets a quick answer (answer first,
  one picture); a "why" gets the mechanism; a how-to gets worked steps; a comparison shows both on one picture; a
  big compound question gets a short map of the parts and a sensible first chunk (or an offer to continue), not one
  part answered as if it were everything; a follow-up answers the new part using what is already on stage; an
  expert gets the intuition fast without a beginner's ramp.
- binding: whenever the narration states a count, a sameness/sharing, a distinctness ("each has its own"), a
  quantity, a symbol, or a process, the picture shows it at that moment (labels, shared colours, numbers, motion,
  formula symbols coloured like their things). Metaphors and asides need not be drawn; do not reward clutter.
- natural: sounds like a person answering, not a template (no ritual "Your turn" or stock openers; a question at
  the end only when it genuinely helps).
- picture: the representation suits the idea (the mechanism itself, in the fewest dimensions that hold the claim).
- layout: frames legible, nothing overlapping or cut off (judge the frames).
- accuracy: everything said and shown is correct.
- teaching: overall, would this learner come away understanding what they asked?

Reply with ONLY a JSON object: {"fit":n,"binding":n,"natural":n,"picture":n,"layout":n,"accuracy":n,"teaching":n,"notes":"<= 40 words: the biggest weakness"}`;

function judge(dir) {
  return new Promise((resolve) => {
    const meta = JSON.parse(fs.readFileSync(path.join(dir, 'meta.json'), 'utf8'));
    let scene = fs.readFileSync(path.join(dir, 'scene.txt'), 'utf8');
    if (meta.prior && fs.existsSync(path.join(dir, 'played.txt'))) { // a follow-up: the stage it started from, then its lines
      const played = fs.readFileSync(path.join(dir, 'played.txt'), 'utf8');
      scene = '# --- already on stage ---\n' + played.slice(0, played.length - scene.length) + '# --- this turn ---\n' + scene;
    }
    const check = fs.readFileSync(path.join(dir, 'check.txt'), 'utf8');
    const frames = fs.readdirSync(dir).filter((f) => f.startsWith('frame_') && f.endsWith('.png')).sort();
    const asked = meta.prior ? `Earlier the learner asked "${meta.prior}" (answered on the stage shown before the new lines). Now they ask: "${meta.q}"` : `The learner asked: "${meta.q}"`;
    const prompt = `${rubricName === 'v2' ? RUBRIC_V2 : RUBRIC}\n\n${asked}\n\nThe script:\n\`\`\`\n${scene}\n\`\`\`\n\nEngine check:\n${check}\n\n` +
      `Look at these rendered frames with the Read tool before scoring (early, middle, end): ${frames.map((f) => path.join(dir, f)).join(', ')}` +
      (frames.length ? '' : '\n(No frames: the scene did not render. Score layout 1.)');
    const c = spawn('claude', ['-p', prompt, '--output-format', 'json', '--model', model, '--effort', 'low',
      '--strict-mcp-config', '--tools', 'Read', '--allowedTools', 'Read', '--add-dir', dir], { cwd: dir, env: ENV });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let res = '';
      try { res = JSON.parse(out).result || ''; } catch { res = out; }
      let score = null;
      const m = res.match(/\{[\s\S]*\}/);
      try { score = JSON.parse(m[0]); } catch { score = { error: res.slice(0, 300) }; }
      fs.writeFileSync(path.join(dir, 'score.json'), JSON.stringify(score, null, 1));
      console.log(`${path.basename(path.dirname(dir))}/${meta.id}: ${KEYS.map((k) => score[k] ?? '?').join(' ')}  ${score.notes || score.error || ''}`);
      resolve();
    });
  });
}

const dirs = [];
for (const v of variants) {
  const vd = path.join(OUT, v);
  if (!fs.existsSync(vd)) continue;
  for (const id of fs.readdirSync(vd)) if (fs.existsSync(path.join(vd, id, 'meta.json'))) dirs.push(path.join(vd, id));
}
const queue = [...dirs];
await Promise.all(Array.from({ length: jobs }, async () => { while (queue.length) await judge(queue.shift()); }));

// ---- summary
const rows = [];
for (const v of variants) {
  const vd = path.join(OUT, v);
  if (!fs.existsSync(vd)) continue;
  const scores = fs.readdirSync(vd).map((id) => { try { return JSON.parse(fs.readFileSync(path.join(vd, id, 'score.json'), 'utf8')); } catch { return null; } })
    .filter((s) => s && !s.error);
  const metas = fs.readdirSync(vd).map((id) => { try { return JSON.parse(fs.readFileSync(path.join(vd, id, 'meta.json'), 'utf8')); } catch { return null; } }).filter(Boolean);
  const mean = (k) => (scores.reduce((a, s) => a + (+s[k] || 0), 0) / Math.max(1, scores.length)).toFixed(2);
  const geoms = fs.readdirSync(vd).map((id) => { try { return JSON.parse(fs.readFileSync(path.join(vd, id, 'layout.json'), 'utf8')).mean; } catch { return null; } })
    .filter((g) => g != null);
  rows.push({ variant: v, n: scores.length, ...Object.fromEntries(KEYS.map((k) => [k, mean(k)])),
    layout_geom: geoms.length ? (geoms.reduce((a, b) => a + b, 0) / geoms.length).toFixed(1) : '-',
    pos: `${metas.filter((m) => m.usesPos).length}/${metas.length}`,
    yourTurn: `${metas.filter((m) => m.yourTurn).length}/${metas.length}`,
    bindingLints: metas.reduce((a, m) => a + (m.binding || 0), 0),
    says: (metas.reduce((a, m) => a + m.says, 0) / Math.max(1, metas.length)).toFixed(1),
    checkOk: `${metas.filter((m) => m.ok).length}/${metas.length}`,
    secs: (metas.reduce((a, m) => a + m.secs, 0) / Math.max(1, metas.length)).toFixed(1) });
}
console.table(rows);
fs.writeFileSync(path.join(OUT, 'summary.json'), JSON.stringify(rows, null, 1));
