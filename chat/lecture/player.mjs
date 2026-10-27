// Playing a prepared lecture on the stage, with questions answered as asides on the same stage.
//
// The stage plays lecture.dan live (`tutor lecture.dan --stdin`), so it reports what it says and the learner can
// type on it. A question ("?..." while paused, or anything typed) or an answer to a part's check (the stage waits on
// the `pause` after it) becomes an aside:
//   1. the stage holds and writes its frame, its markup and the step to come back to (#!capture)
//   2. Claude answers with a short scene that continues from that stage (the plan, the bible and the part in hand)
//   3. the aside plays in the same window from the stage as it stood (#!load aside.dan LINE: what came before LINE
//      is already there), then the lecture comes back at the sentence it left (#!load lecture.dan, #!seek STEP)
// The aside's things go when the lecture comes back: the lecture's own picture returns as it was (README, lectures).
// Asides are kept in asides/NNN/ with the question, so a later replay can show where the learner asked.
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { buildSystem } from '../system-prompt.mjs';
import { expandReply } from '../compute.mjs';
import { ask } from './claude.mjs';
import { dirOf, readJson } from './store.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');

const ASIDE = ({ kind, text, part, heard, stage }) => `<lecture_moment part="${part?.n ?? '?'}" title="${part?.title ?? ''}">
${kind === 'check'
    ? `The part ended with its check: "${part.check.question}"\nThe expected answer: ${part.check.answer}\nWhy: ${part.check.why || ''}\n\nThe learner answered: "${text}"`
    : `The learner stopped the lecture here and asks: "${text || 'explain what is on screen right now'}"`}
They had just heard: "${heard || '(the start of the part)'}"
The stage at this moment is the attached image, and this markup (already drawn):
</lecture_moment>
<stage_at_pause>
${stage.trim()}
</stage_at_pause>

Reply in the usual format (plan, then scene; no memory block) with a short aside, then the lecture goes on by itself:
- one scene that continues from this stage: reuse its things by their ids; never a new \`scene\` line, never redraw it
- ${kind === 'check'
    ? 'say whether the answer is right; if it is, confirm it with the reason in one or two sentences; if it is wrong or partly right, show on the stage where the reasoning goes off and what the right answer is and why (at most 6 Say lines)'
    : 'answer exactly what they asked, at the lecture\'s level, from the source, in at most 6 Say lines'}
- end with one short sentence back to the lecture`;

export function play(m, { voiceArgs = [], log = console.log } = {}) {
  const dir = dirOf(m.id);
  const lecture = path.join(dir, 'lecture.dan');
  const index = readJson(path.join(dir, 'index.json'), { parts: [] });
  const outline = readJson(path.join(dir, 'outline.json'), {});
  const bible = fs.readFileSync(path.join(dir, 'bible.md'), 'utf8');
  const system = buildSystem() + `\n\n# This is a prepared lecture\n\n<outline>\n${JSON.stringify(outline, null, 1)}\n</outline>\n\n<bible>\n${bible}\n</bible>\n`;
  const asides = path.join(dir, 'asides');
  let count = fs.existsSync(asides) ? fs.readdirSync(asides).length : 0;

  const stage = spawn(path.join(ROOT, 'tutor'), [lecture, '--stdin', ...voiceArgs], { cwd: ROOT, stdio: ['pipe', 'pipe', 'inherit'] });
  const send = (line) => { if (stage.exitCode === null) stage.stdin.write(line + '\n'); };
  stage.stdin.on('error', () => {});
  let part = 0, checking = 0, mode = 'lecture', busy = false, back = null;
  const heard = []; // the sentences said in this part so far
  const captured = new Map();

  async function aside(kind, text) {
    if (busy) return;
    busy = true;
    const n = ++count;
    const at = path.join(asides, String(n).padStart(3, '0'));
    fs.mkdirSync(at, { recursive: true });
    const p = index.parts[(kind === 'check' ? checking : part) - 1];
    send('#!pause');
    const got = new Promise((ok) => captured.set(at, ok));
    send(`#!capture ${at}`);
    await got;
    send(`#!state thinking ${(text || 'explain this').replace(/\n/g, ' ')}`);
    const stageText = fs.readFileSync(path.join(at, 'stage.txt'), 'utf8');
    const step = Number(fs.readFileSync(path.join(at, 'step.txt'), 'utf8'));
    log(`aside ${n} (${kind}, part ${p?.n ?? '?'}): ${text}`);
    const content = [{ type: 'text', text: ASIDE({ kind, text, part: p, heard: heard.slice(-4).join(' '), stage: stageText }) }];
    try { content.push({ type: 'image', source: { type: 'base64', media_type: 'image/png', data: fs.readFileSync(path.join(at, 'frame.png')).toString('base64') } }); } catch {}
    const r = await ask({ system, content, model: m.model, effort: 'medium', cwd: dir });
    fs.writeFileSync(path.join(at, 'reply.txt'), r.text || r.error);
    const { scene } = r.error ? { scene: '' } : await expandReply(r.text);
    // where the lecture picks up: the sentence it was in (a question), or the next part (after a check)
    back = step;
    fs.writeFileSync(path.join(at, 'aside.json'), JSON.stringify({ kind, text, part: p?.n, step, at: new Date().toISOString() }, null, 1) + '\n');
    if (!scene.trim()) { log(`  no aside: ${r.error || 'empty reply'}`); return comeBack(); }
    const stageLines = stageText.trimEnd().split('\n');
    const file = path.join(at, 'aside.dan');
    fs.writeFileSync(file, `${stageLines.join('\n')}\n# --- ${kind === 'check' ? 'the learner answered' : 'the learner asked'}: ${text || 'explain this'} ---\n${scene}\n`);
    mode = 'aside';
    send('#!state idle');
    send(`#!load ${file} ${stageLines.length + 2}`);
  }

  function comeBack() {
    mode = 'lecture'; busy = false; checking = 0;
    send('#!state idle');
    send(`#!load ${lecture}`);
    send(`#!seek ${back ?? 0}`);
  }

  let buf = '';
  stage.stdout.on('data', (d) => {
    buf += d;
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i); buf = buf.slice(i + 1);
      if (!line.startsWith('@ev ')) continue;
      const ev = line.slice(4), sp = ev.indexOf(' ');
      const kind = sp < 0 ? ev : ev.slice(0, sp), rest = sp < 0 ? '' : ev.slice(sp + 1);
      if (kind === 'say') {
        if (mode === 'lecture') heard.push(JSON.parse(rest.slice(rest.indexOf(' ') + 1)));
      } else if (kind === 'part' || kind === 'seek') { // a part starts, or the learner jumped
        if (mode === 'lecture') { heard.length = 0; part = Number(rest) || part; checking = 0; } // a check passed by with space
      } else if (kind === 'waiting' && mode === 'lecture') { // a part's check: the learner may answer, or press space
        checking = part = Number(rest) || part;
        send('#!state listening');
      } else if (kind === 'idle' && mode === 'aside') {
        setTimeout(comeBack, 1200); // a breath, then back to the lecture
      } else if (kind === 'input') {
        aside(checking ? 'check' : 'question', JSON.parse(rest));
      } else if (kind === 'explain') {
        aside('question', JSON.parse(rest));
      } else if (kind === 'captured') {
        captured.get(rest)?.();
      }
    }
  });
  stage.on('close', (code) => log(`stage closed (${code})`));
  return stage;
}
