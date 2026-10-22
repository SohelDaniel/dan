// Per-sentence binding: a frame at the end of every Say (just before the next one starts), and a judge that
// says, sentence by sentence, whether that frame shows what the sentence claims. A direct measure of
// "spoken but not shown", unlike the scene judge that sees 3 frames.
//   node sentjudge.mjs --dir /abs/eval/full [--jobs 6] [--only a,b]
import fs from 'node:fs';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';

const ROOT = '/Users/soheldanial/code/server/testing/visulaizations/tutor';
const TUTOR = path.join(ROOT, 'tutor');
const argv = process.argv.slice(2);
const flag = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const DIR = path.resolve(flag('--dir')), jobs = Number(flag('--jobs', 6)), only = flag('--only', '');
const ENV = Object.fromEntries(Object.entries(process.env).filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const RUBRIC = `You judge one thing about an AI visual tutor: does the picture show what each sentence says, at the
moment it is said? Below are the spoken sentences in order. For each sentence there is a frame of the stage
taken just as that sentence ends. Read every frame with the Read tool.

For each sentence, decide:
- claim: the visual claim the sentence makes, in a few words: a number/quantity, a count, a step or action
  ("add them", "it doubles", "splits"), a sameness or difference, a comparison, a thing pointed at ("this curve").
  Use "none" for metaphors, transitions, questions to the learner and asides that need no picture.
- shown: "yes" if the frame (or the change into it) visibly carries that claim, "partly", or "no". Use "na" when
  claim is "none". A number counts as shown only if it is readable on stage or is a count you can see. A step
  counts as shown if its result is visibly there (a sum appears, a thing moved, a colour marks the group).

Reply with ONLY JSON: {"sentences":[{"i":1,"claim":"...","shown":"yes|partly|no|na"}, ...]}`;

function frames(dir) {
  const played = path.join(dir, 'played.txt');
  const out = path.join(dir, 'sent');
  fs.rmSync(out, { recursive: true, force: true });
  fs.mkdirSync(out);
  const v = spawnSync(TUTOR, [played, '--snapshot', path.join(out, 'x'), '--at', 'end', '--verbose'], { cwd: ROOT, encoding: 'utf8' });
  // only this turn's Says (a follow-up starts from an earlier stage)
  const sceneLines = fs.readFileSync(path.join(dir, 'scene.txt'), 'utf8').split('\n').length;
  const totalLines = fs.readFileSync(played, 'utf8').split('\n').length;
  const first = totalLines - sceneLines + 1;
  const says = [...(v.stderr || '').matchAll(/^\[\s*([\d.]+)s\] line (\d+): Say:\s*"?(.*?)"?\s*$/gm)]
    .map((m) => ({ t: +m[1], line: +m[2], text: m[3].replace(/\|[^|\s]*\|/g, '').replace(/\[[^\]\s]*\]/g, '') }));
  const mine = says.filter((s) => s.line >= first);
  if (!mine.length) return [];
  const times = mine.map((s, i) => { const nxt = says[says.indexOf(s) + 1]; return nxt ? Math.max(s.t + 0.5, nxt.t - 0.15).toFixed(2) : 'end'; });
  for (let a = 0; a < 3; a++) {
    spawnSync(TUTOR, [played, '--snapshot', path.join(out, 'f'), '--at', times.join(',')], { cwd: ROOT });
    if (fs.readdirSync(out).some((f) => f.startsWith('f_'))) break;
  }
  return mine.map((s, i) => ({ ...s, frame: path.join(out, `f_${times[i]}.png`) }));
}

function judge(dir) {
  return new Promise((resolve) => {
    const ss = frames(dir);
    if (!ss.length || !ss.every((s) => fs.existsSync(s.frame))) {
      console.log(`${path.basename(dir)}: frames missing`);
      return resolve();
    }
    const body = ss.map((s, i) => `${i + 1}. "${s.text}"\n   frame: ${s.frame}`).join('\n');
    const c = spawn('claude', ['-p', `${RUBRIC}\n\n${body}`, '--output-format', 'json', '--model', 'sonnet', '--effort', 'low',
      '--strict-mcp-config', '--tools', 'Read', '--allowedTools', 'Read', '--add-dir', dir], { cwd: dir, env: ENV });
    let out = '';
    c.stdout.on('data', (d) => (out += d));
    c.on('close', () => {
      let res = '';
      try { res = JSON.parse(out).result || ''; } catch { res = out; }
      let j = null;
      try { j = JSON.parse(res.match(/\{[\s\S]*\}/)[0]); } catch { j = { error: res.slice(0, 300) }; }
      fs.writeFileSync(path.join(dir, 'sentences.json'), JSON.stringify(j, null, 1));
      const s = j.sentences || [];
      const claims = s.filter((x) => x.shown !== 'na');
      const yes = claims.filter((x) => x.shown === 'yes').length, part = claims.filter((x) => x.shown === 'partly').length;
      console.log(`${path.basename(dir)}: ${s.length} sentences, ${claims.length} claims, yes ${yes}, partly ${part}, no ${claims.length - yes - part}`);
      resolve();
    });
  });
}

let ids = fs.readdirSync(DIR).filter((d) => fs.existsSync(path.join(DIR, d, 'played.txt')));
if (only) ids = ids.filter((i) => only.split(',').includes(i));
const q = ids.map((i) => path.join(DIR, i));
await Promise.all(Array.from({ length: jobs }, async () => { while (q.length) await judge(q.shift()); }));
