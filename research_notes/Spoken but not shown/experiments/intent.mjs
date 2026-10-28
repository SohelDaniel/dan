// Experiment: Claude writes only the words and a plain-language "show:" line per sentence; an interpreter model
// turns each beat into markup (objects, relations, cues), as a small local model would. Output has run.mjs's
// shape so judge.mjs and sentences.mjs score it.
//   node intent.mjs --out DIR --variant NAME --interp sonnet|haiku|ollama:qwen3:8b [--jobs 4] [--only a,b] [--writer-from DIR/variant]
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';

const ROOT = '<repo>';
const TUTOR = path.join(ROOT, 'tutor');
const SP = '<scratchpad>';
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const OUT = path.resolve(flag('--out')), variant = flag('--variant'), interp = flag('--interp', 'sonnet');
const jobs = Number(flag('--jobs', 4)), only = flag('--only', ''), writerFrom = flag('--writer-from', '');
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const WRITER = fs.readFileSync(path.join(SP, 'exp/variants/intent-writer.md'), 'utf8');
const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
const LANG = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
const prompt = fs.readFileSync(path.join(ROOT, 'chat/prompt.md'), 'utf8');
const GUIDE = prompt.slice(prompt.indexOf('## Show what you say'), prompt.indexOf('## How scenes play'));
let EXAMPLES = '';
for (const f of fs.readdirSync(path.join(ROOT, 'chat/exemplars')).filter((f) => f.endsWith('.txt')).sort())
  EXAMPLES += `\n### ${f.slice(0, -4)}\n\`\`\`\n${fs.readFileSync(path.join(ROOT, 'chat/exemplars', f), 'utf8').trim()}\n\`\`\`\n`;

const INTERP = `You translate a live tutor's intent into the markup of the \`tutor\` engine. The tutor wrote what is said
(\`Say:\` lines) and, for each sentence, a plain-language \`show:\` line saying what the learner sees appear or change
while it is said (\`cue: what happens\`). You write the markup that makes exactly that happen.

You get the scene so far (markup already on stage) and ONE chunk: either the opening picture (a show line before any
Say, plus title/schema lines) or one beat (a Say line and its show line). Reply with the markup for that chunk only,
in a \`\`\`markup block, nothing else.

Rules:
- Copy the Say line with its words unchanged. You may glue \`|cue|\` markers and \`[id]\` pointers onto its words; every
  \`|cue|\` needs an \`@cue\` line in this beat and every \`@cue\` line needs its \`|cue|\`.
- After the Say, write the lines that make the show line happen, each cue's lines tagged \`@cue\`. Untagged lines run
  after the sentence ends.
- Use the ids the show line names; reuse things already on stage (re-mark, morph, indicate, hide) rather than making
  new ones. Only reference ids that exist or that you create.
- Never give positions (no coordinates, no region/below/near); structure is layout: links, groups, schema order.
- Never put the sentence itself on stage. No extra decoration beyond what the show line says.
- \`show: -\` means: just the Say line.

${GUIDE}

${LANG}

## Gold-standard scenes (whole scenes, for the markup style)
${EXAMPLES}`;

// ---------------------------------------------------------------- models
function claude(model, system, user, stream) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const args = ['-p', user, '--tools', '', '--strict-mcp-config', '--model', model, '--effort', 'low'];
    if (stream) args.push('--output-format', 'stream-json', '--verbose', '--include-partial-messages', '--append-system-prompt', system);
    else args.push('--output-format', 'json', '--system-prompt', system);
    const c = spawn('claude', args, { env: ENV });
    let buf = '', text = '', firstSay = null, firstLine = null;
    c.stdout.on('data', (d) => {
      buf += d;
      if (!stream) return;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        let e; try { e = JSON.parse(line); } catch { continue; }
        const delta = e.type === 'stream_event' && e.event?.delta?.type === 'text_delta' ? e.event.delta.text : '';
        if (!delta) continue;
        text += delta;
        const sc = text.indexOf('```scene');
        if (sc >= 0 && firstLine === null && /```scene[^\n]*\n[^\n]+\n/.test(text.slice(sc))) firstLine = (Date.now() - t0) / 1000;
        if (sc >= 0 && firstSay === null && /\nSay:[^\n]*\n/.test(text.slice(sc))) firstSay = (Date.now() - t0) / 1000;
      }
    });
    c.on('close', () => {
      if (!stream) { try { text = JSON.parse(buf).result || ''; } catch { text = buf; } }
      resolve({ text, secs: (Date.now() - t0) / 1000, firstSay, firstLine });
    });
  });
}

async function ollama(model, system, user) {
  const t0 = Date.now();
  const r = await fetch('http://127.0.0.1:11434/api/chat', {
    method: 'POST',
    body: JSON.stringify({ model, stream: false, think: false, keep_alive: '30m', options: { num_ctx: 32768, temperature: 0.2 },
      messages: [{ role: 'system', content: system }, { role: 'user', content: user }] }),
  });
  const j = await r.json();
  return { text: j.message?.content || '', secs: (Date.now() - t0) / 1000, evalTokens: j.eval_count, promptTokens: j.prompt_eval_count,
    promptSecs: (j.prompt_eval_duration || 0) / 1e9, genSecs: (j.eval_duration || 0) / 1e9 };
}

const translate = (user) => interp.startsWith('ollama:') ? ollama(interp.slice(7), INTERP, user) : claude(interp, INTERP, user, false);

// ---------------------------------------------------------------- helpers
const block = (s, name) => { const m = s.match(new RegExp('```\\s*' + name + '\\b[^\\n]*\\n([\\s\\S]*?)```')); return m ? m[1] : ''; };
const isSay = (l) => l.trim().startsWith('Say:');
const words = (l) => l.trim().slice(4).replace(/\|[^|\s]*\|/g, '').replace(/\[[^\]\s]*\]/g, '').replace(/\s+/g, ' ').trim();
const run = (f, ...a) => spawnSync(TUTOR, [f, ...a], { cwd: ROOT, encoding: 'utf8' });
const otherLints = (r) => ((r.stderr || '') + (r.stdout || '')).split('\n').filter((l) => /^lint:|^parse:/.test(l) && !/binding:|layout:|zero:/.test(l));

// the intent scene in chunks: an opening (title/schema/scene lines + the show before any Say), then beats
function chunks(intent) {
  const out = [];
  let cur = { head: [], say: null, show: [] };
  const flush = () => { if (cur.head.length || cur.say || cur.show.length) out.push(cur); };
  for (const raw of intent.split('\n')) {
    const l = raw.trim();
    if (!l) continue;
    if (isSay(l)) { flush(); cur = { head: [], say: l, show: [] }; }
    else if (/^show:/i.test(l)) cur.show.push(l.replace(/^show:\s*/i, ''));
    else if (/^(scene|title|schema)\b/.test(l)) { if (cur.say) { flush(); cur = { head: [l], say: null, show: [] }; } else cur.head.push(l); }
    else if (l.startsWith('#')) continue;
    else cur.show.push(l); // anything else: treat as more intent
  }
  flush();
  return out;
}

async function one(p) {
  const dir = path.join(OUT, variant, p.id);
  fs.mkdirSync(dir, { recursive: true });
  const ctx = (p.prior ? `<history>\nLearner: ${p.prior}\n(you answered it on the stage below)\n</history>\n\n` : '') +
    `<stage>\n${p.stage || '(empty)'}\n</stage>\n\nLearner: ${p.q}`;
  let w;
  if (writerFrom) w = JSON.parse(fs.readFileSync(path.join(writerFrom, p.id, 'writer.json'), 'utf8'));
  else w = await claude('sonnet', WRITER, ctx, true);
  fs.writeFileSync(path.join(dir, 'writer.json'), JSON.stringify(w));
  fs.writeFileSync(path.join(dir, 'reply.txt'), w.text);
  const plan = block(w.text, 'plan'), intent = block(w.text, 'scene');
  fs.writeFileSync(path.join(dir, 'plan.txt'), plan);
  fs.writeFileSync(path.join(dir, 'intent.txt'), intent);
  const prior = p.stage ? p.stage + '\n' : '';
  const sofar = [];
  const log = [];
  for (const c of chunks(intent)) {
    const showText = c.show.join(' ').trim();
    const ask = `<scene_so_far>\n${prior}${sofar.join('\n')}\n</scene_so_far>\n\n<chunk>\n${c.head.join('\n')}${c.head.length ? '\n' : ''}` +
      `${c.say ? c.say + '\n' : ''}show: ${showText || '-'}\n</chunk>`;
    let lines = null, why = '', tries = 0, t = [];
    let feedback = '';
    while (tries < 2 && !lines) {
      tries++;
      const r = await translate(ask + feedback);
      t.push(r);
      let m = block(r.text, 'markup') || block(r.text, '');
      let got = m ? m.replace(/\n+$/, '').split('\n').filter((l) => l.trim()) : null;
      if (!got) { why = 'no block'; feedback = '\n\nYour reply had no ```markup block. Reply with only the block.'; continue; }
      // the head lines (title/schema) are the writer's: keep them, drop the interpreter's copies
      got = got.filter((l) => !/^(scene|title|schema)\b/.test(l.trim()) || !c.head.length);
      const s0 = c.say ? [words(c.say)] : [], s1 = got.filter(isSay).map(words);
      if (JSON.stringify(s0) !== JSON.stringify(s1)) { why = 'words changed'; feedback = '\n\nYou changed the Say line\'s words. Copy them exactly.'; continue; }
      const tmp = path.join(dir, '.try.txt');
      fs.writeFileSync(tmp, prior + [...sofar, ...c.head, ...got].join('\n') + '\n');
      const was = path.join(dir, '.was.txt');
      fs.writeFileSync(was, prior + [...sofar, ...c.head, ...(c.say ? [c.say] : [])].join('\n') + '\n');
      const a = run(tmp, '--check'), b = run(was, '--check');
      const extra = otherLints(a).filter((l) => !otherLints(b).includes(l));
      if (extra.length) { why = 'lints'; feedback = `\n\nThe engine reported:\n${extra.join('\n')}\nFix the markup.`; continue; }
      lines = got; why = 'ok';
    }
    if (!lines) lines = [...c.head, ...(c.say ? [c.say] : [])]; // fall back: the words only
    else lines = [...c.head, ...lines];
    sofar.push(...lines);
    log.push({ show: showText, say: c.say, out: lines, why, tries, secs: t.map((x) => x.secs), meta: t.map((x) => ({ promptTokens: x.promptTokens, evalTokens: x.evalTokens, promptSecs: x.promptSecs, genSecs: x.genSecs })) });
  }
  for (const f of ['.try.txt', '.was.txt']) fs.rmSync(path.join(dir, f), { force: true });
  const scene = sofar.join('\n');
  fs.writeFileSync(path.join(dir, 'scene.txt'), scene);
  fs.writeFileSync(path.join(dir, 'played.txt'), prior + scene);
  fs.writeFileSync(path.join(dir, 'interp.json'), JSON.stringify(log, null, 1));
  const chk = run(path.join(dir, 'played.txt'), '--check');
  fs.writeFileSync(path.join(dir, 'check.txt'), (chk.stdout || '') + (chk.stderr || ''));
  for (let a = 0; a < 3 && !fs.readdirSync(dir).some((f) => f.startsWith('frame_')); a++)
    run(path.join(dir, 'played.txt'), '--snapshot', path.join(dir, 'frame'), '--at', '6,14,end');
  const lr = run(path.join(dir, 'played.txt'), '--layout-report', '-');
  let layout = null;
  try { layout = JSON.parse(lr.stdout.trim().split('\n').pop()); fs.writeFileSync(path.join(dir, 'layout.json'), JSON.stringify(layout)); } catch {}
  const says = scene.split('\n').filter(isSay);
  const per = log.map((l) => l.secs.reduce((a, b) => a + b, 0)).sort((a, b) => a - b);
  const meta = { id: p.id, subject: p.subject, type: p.type, q: p.q, prior: p.prior, variant, model: 'sonnet+' + interp, secs: w.secs,
    ok: chk.status === 0, says: says.length, lastSay: says.at(-1) || '', lines: scene.split('\n').filter((l) => l.trim()).length,
    layoutGeom: layout ? layout.mean : null, layoutWorst: layout ? layout.worst : null, usesPos: false, usesNew: [], yourTurn: /your turn/i.test(scene),
    binding: ((chk.stdout || '').match(/^  binding:/gm) || []).length,
    writer: { firstLine: w.firstLine, firstSay: w.firstSay, total: w.secs, chars: w.text.length, sceneChars: intent.length },
    interp: { chunks: log.length, ok: log.filter((l) => l.why === 'ok').length, retried: log.filter((l) => l.tries > 1).length,
      failed: log.filter((l) => l.why !== 'ok').length, medianSecs: per[per.length >> 1] } };
  fs.writeFileSync(path.join(dir, 'meta.json'), JSON.stringify(meta, null, 1));
  console.log(`${p.id}: first Say ${w.firstSay?.toFixed(1)} s, ${log.length} chunks, ok ${meta.interp.ok}, retried ${meta.interp.retried}, failed ${meta.interp.failed}, ${meta.interp.medianSecs?.toFixed(1)} s/chunk, check ${meta.ok ? 'ok' : 'FAIL'}`);
}

let prompts = JSON.parse(fs.readFileSync(path.join(ROOT, 'tools/eval/prompts-broad.json'), 'utf8'));
if (only) prompts = prompts.filter((p) => only.split(',').includes(p.id));
const q = [...prompts];
await Promise.all(Array.from({ length: jobs }, async () => { while (q.length) await one(q.shift()); }));
