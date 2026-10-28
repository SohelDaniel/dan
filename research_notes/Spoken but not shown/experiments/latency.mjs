// Time to the first scene line (what the learner waits through: thinking + plan), per prompt variant.
// Streams `claude -p` like the chat server does and timestamps the first text and the ```scene fence.
//   node latency.mjs --prompt FILE --n 12 [--jobs 3] --out results.json
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';

const ROOT = '<repo>';
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const N = Number(flag('--n', 12)), jobs = Number(flag('--jobs', 3));
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
const lang = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
let examples = '';
for (const f of fs.readdirSync(path.join(ROOT, 'chat', 'exemplars')).filter((f) => f.endsWith('.txt')).sort())
  examples += `\n### ${f.slice(0, -4)}\n\n\`\`\`scene\n${fs.readFileSync(path.join(ROOT, 'chat', 'exemplars', f), 'utf8').trim()}\n\`\`\`\n`;
const SYSTEM = fs.readFileSync(path.resolve(flag('--prompt')), 'utf8').replace('{{EXAMPLES}}', examples) + lang;
const prompts = JSON.parse(fs.readFileSync(path.join(ROOT, 'tools/eval/prompts-broad.json'), 'utf8')).filter((p) => !p.prior).slice(0, N);

function one(p) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const c = spawn('claude', ['-p', `<stage>\n(empty)\n</stage>\n\nLearner: ${p.q}`, '--output-format', 'stream-json', '--verbose',
      '--include-partial-messages', '--tools', '', '--strict-mcp-config', '--model', 'sonnet', '--effort', 'low',
      '--append-system-prompt', SYSTEM], { env: ENV });
    let buf = '', text = '', first = null, scene = null;
    c.stdout.on('data', (d) => {
      buf += d;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        let e; try { e = JSON.parse(line); } catch { continue; }
        const delta = e.type === 'stream_event' && e.event?.delta?.type === 'text_delta' ? e.event.delta.text : '';
        if (!delta) continue;
        if (first === null) first = (Date.now() - t0) / 1000;
        text += delta;
        if (scene === null && /```\s*scene[^\n]*\n[^\n]+\n/.test(text)) scene = (Date.now() - t0) / 1000;
      }
    });
    c.on('close', () => resolve({ id: p.id, first, scene, total: (Date.now() - t0) / 1000, planChars: (text.match(/```plan\n([\s\S]*?)```/) || ['', ''])[1].length }));
  });
}

const q = [...prompts], res = [];
await Promise.all(Array.from({ length: jobs }, async () => { while (q.length) { const r = await one(q.shift()); res.push(r); console.log(JSON.stringify(r)); } }));
const med = (k) => { const v = res.map((r) => r[k]).filter((x) => x != null).sort((a, b) => a - b); return v[v.length >> 1]; };
const summary = { prompt: flag('--prompt'), n: res.length, firstText: med('first'), firstSceneLine: med('scene'), total: med('total'), planChars: med('planChars') };
console.log(JSON.stringify(summary));
fs.writeFileSync(path.resolve(flag('--out')), JSON.stringify({ summary, res }, null, 1));
