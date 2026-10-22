// Generate one first-turn scene per prompt with a given prompt variant, then check and render it.
//
//   node tools/eval/run.mjs --variant full|method|base --out DIR [--only a,b] [--model sonnet] [--jobs 4] [--prompts prompts-2d.json] [--prompt chat/prompt.md] [--no-idioms]
//
// Variants: base   = format + scene rules only (no plan, no teaching method, no examples)
//           method = chat/prompt.md without the worked examples
//           full   = chat/prompt.md with the worked examples (what the chat server uses)
// Per prompt, DIR/<variant>/<id>/ gets reply.txt, plan.txt, scene.txt, check.txt, frame_*.png, meta.json.
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const variant = flag('--variant', 'full');
const model = flag('--model', 'sonnet');
const OUT = path.resolve(flag('--out', path.join(HERE, 'out')), variant + (model === 'sonnet' ? '' : '-' + model));
const jobs = Number(flag('--jobs', 4));
const only = flag('--only', '');
let prompts = JSON.parse(fs.readFileSync(path.resolve(HERE, flag('--prompts', 'prompts.json')), 'utf8'));
if (only) prompts = prompts.filter((p) => only.split(',').includes(p.id));

function system() {
  const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
  const lang = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
  const ex = path.join(ROOT, 'chat', 'exemplars');
  let examples = '';
  if (variant === 'full')
    for (const f of fs.readdirSync(ex).filter((f) => f.endsWith('.txt')).sort())
      examples += `\n### ${f.slice(0, -4)}\n\n\`\`\`scene\n${fs.readFileSync(path.join(ex, f), 'utf8').trim()}\n\`\`\`\n`;
  // the visual moves mined from 3Blue1Brown, as the chat server adds them (--no-idioms leaves them out)
  if (variant === 'full' && !argv.includes('--no-idioms') && fs.existsSync(path.join(ROOT, 'chat', 'idioms.md')))
    examples += '\n' + fs.readFileSync(path.join(ROOT, 'chat', 'idioms.md'), 'utf8') + '\n';
  const file = variant === 'base' ? path.join(HERE, 'variants', 'base.md') : path.resolve(flag('--prompt', path.join(ROOT, 'chat', 'prompt.md')));
  return fs.readFileSync(file, 'utf8').replace('{{EXAMPLES}}', examples) + lang;
}
const SYSTEM = system();
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

function block(reply, name) {
  const m = reply.match(new RegExp('```\\s*' + name + '\\b[^\\n]*\\n([\\s\\S]*?)```'));
  return m ? m[1] : '';
}

function generate(p) {
  return new Promise((resolve) => {
    const dir = path.join(OUT, p.id);
    fs.mkdirSync(dir, { recursive: true });
    const t0 = Date.now();
    // a follow-up starts from a stage and an earlier question (as the chat server sends them)
    const ctx = (p.prior ? `<history>\nLearner: ${p.prior}\n(you answered it on the stage below)\n</history>\n\n` : '') +
      `<stage>\n${p.stage || '(empty)'}\n</stage>\n\nLearner: ${p.q}`;
    const c = spawn('claude', ['-p', ctx, '--output-format', 'json',
      '--tools', '', '--strict-mcp-config', '--model', model, '--effort', 'low', '--append-system-prompt', SYSTEM],
    { cwd: dir, env: ENV });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let reply = '';
      try { reply = JSON.parse(out).result || ''; } catch { reply = out; }
      const secs = (Date.now() - t0) / 1000;
      fs.writeFileSync(path.join(dir, 'reply.txt'), reply);
      fs.writeFileSync(path.join(dir, 'plan.txt'), block(reply, 'plan'));
      const scene = block(reply, 'scene');
      fs.writeFileSync(path.join(dir, 'scene.txt'), scene);
      // what plays: a follow-up is appended to the stage it started from
      const played = p.stage ? p.stage + '\n' + scene : scene;
      fs.writeFileSync(path.join(dir, 'played.txt'), played);
      const chk = spawnSync(TUTOR, [path.join(dir, 'played.txt'), '--check'], { cwd: ROOT, encoding: 'utf8' });
      fs.writeFileSync(path.join(dir, 'check.txt'), (chk.stdout || '') + (chk.stderr || ''));
      // frames: early, middle, end (muted timing). Rendering opens a hidden window, which can fail
      // when many start at once: retry, and say so if it still fails (the judge must not score blind)
      for (let attempt = 0; attempt < 3; attempt++) {
        spawnSync(TUTOR, [path.join(dir, 'played.txt'), '--snapshot', path.join(dir, 'frame'), '--at', '6,14,end'],
          { cwd: ROOT, encoding: 'utf8' });
        if (fs.readdirSync(dir).some((f) => f.startsWith('frame_'))) break;
        if (attempt === 2) console.warn(`${variant}/${p.id}: frames failed to render`);
      }
      // the exact layout checker (geometry), next to the judge's impression of the frames
      const lr = spawnSync(TUTOR, [path.join(dir, 'played.txt'), '--layout-report', '-'], { cwd: ROOT, encoding: 'utf8' });
      let layout = null;
      try { layout = JSON.parse(lr.stdout.trim().split('\n').pop()); fs.writeFileSync(path.join(dir, 'layout.json'), JSON.stringify(layout)); } catch {}
      const says = scene.split('\n').filter((l) => l.trim().startsWith('Say:'));
      const meta = { id: p.id, subject: p.subject, type: p.type, q: p.q, prior: p.prior, variant, model, secs, ok: chk.status === 0,
        says: says.length, lastSay: says.at(-1) || '', lines: scene.split('\n').filter((l) => l.trim()).length,
        layoutGeom: layout ? layout.mean : null, layoutWorst: layout ? layout.worst : null,
        usesPos: /\bpos=/.test(scene), usesNew: (scene.match(/\b(camera|trail=|field|slope|solution|heat|contour)\b/g) || []).filter((v, i, a) => a.indexOf(v) === i), yourTurn: /your turn/i.test(scene), binding: ((chk.stdout || '').match(/^  binding:/gm) || []).length, usesWords: /\b(region|below|above|left-of|right-of|near|align|on|in|over)=/.test(scene) };
      fs.writeFileSync(path.join(dir, 'meta.json'), JSON.stringify(meta, null, 1));
      console.log(`${variant}/${p.id}: ${secs.toFixed(1)}s, check ${meta.ok ? 'ok' : 'FAIL'}, ${meta.says} Say`);
      resolve(meta);
    });
  });
}

const queue = [...prompts];
await Promise.all(Array.from({ length: jobs }, async () => { while (queue.length) await generate(queue.shift()); }));
