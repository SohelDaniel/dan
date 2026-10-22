// Pause and ask: while the lesson is paused, the learner asks about what's on screen ("?why is that 6?" typed on the
// stage). A side conversation answers with a short scene that starts from the stage exactly as it was at the pause,
// played in a second window; the lesson stays paused and untouched until the learner closes the window and resumes.
//
// The side conversation is a fork of the lesson's Claude session (--resume <sid> --fork-session): it knows the whole
// lesson so far, and the lesson's own session and process never see it. The lesson hears about it on its next turn.
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { expandReply } from './compute.mjs';

const ASK = ({ question, heard, stage }) => `<pause>
The learner paused the lesson here and asks about this moment: "${question || 'explain what is on screen right now'}"
They had heard: "${heard || '(nothing yet)'}"
The stage at the pause is the attached image, and this markup (it is already drawn):
</pause>
<stage_at_pause>
${stage.trim()}
</stage_at_pause>

Answer with a short side explanation, in the usual reply format (a plan block, then a scene block; no memory block):
- one scene that continues from this stage: reuse its things by their ids, don't redraw it or start a new scene
- answer exactly what they asked, at the lesson's level, in at most 5 Say lines
- end with one sentence that hands them back to the lesson (they press space to go on)`;

// one forked, one-shot Claude call; resolves with the reply text ('' on failure)
function askFork({ sid, system, model, effort, cwd, env, frame, text }) {
  return new Promise((resolve) => {
    const args = ['-p', '--input-format', 'stream-json', '--output-format', 'stream-json', '--verbose',
      '--tools', '', '--strict-mcp-config', '--model', model, '--effort', effort, '--append-system-prompt', system];
    if (sid) args.push('--resume', sid, '--fork-session'); // the lesson so far, without touching the lesson
    const p = spawn('claude', args, { cwd, env });
    let buf = '', reply = '';
    p.stdout.on('data', (d) => {
      buf += d;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        let e; try { e = JSON.parse(line); } catch { continue; }
        if (e.type === 'result' && !e.is_error) reply = e.result || reply;
      }
    });
    p.stdin.on('error', () => {});
    p.on('close', () => resolve(reply));
    const content = [];
    try { content.push({ type: 'image', source: { type: 'base64', media_type: 'image/png', data: fs.readFileSync(frame).toString('base64') } }); } catch {}
    content.push({ type: 'text', text });
    p.stdin.end(JSON.stringify({ type: 'user', message: { role: 'user', content } }) + '\n');
  });
}

// session: the chat's state (dir, sid, ...); stage: { send(line), captured(dir) -> Promise }; ui: { broadcast, note(text) }
export function createExplainer({ tutor, root, system, model, effort, env, voiceArgs }) {
  let window = null; // the explanation's stage window, one at a time
  let count = 0;

  async function explain({ session, question, heard, stage, ui }) {
    if (window) window.kill(); // a new question replaces the last answer
    const dir = path.join(session.dir, 'explain', String(++count));
    fs.mkdirSync(dir, { recursive: true });
    ui.broadcast({ t: 'explain', state: 'asking', q: question });
    stage.send('#!pause');
    const captured = stage.captured(dir);
    stage.send(`#!capture ${dir}`);
    await captured;
    const atPause = fs.readFileSync(path.join(dir, 'stage.txt'), 'utf8');

    const reply = await askFork({ sid: session.sid, system, model: model(), effort, cwd: session.dir, env,
      frame: path.join(dir, 'frame.png'), text: ASK({ question, heard, stage: atPause }) });
    fs.writeFileSync(path.join(dir, 'reply.txt'), reply);
    const { scene, problems } = await expandReply(reply);
    if (!scene.trim()) {
      ui.broadcast({ t: 'explain', state: 'failed', q: question });
      return;
    }
    // the second window starts from the stage at the pause, then plays the answer
    const file = path.join(dir, 'explain.txt');
    fs.writeFileSync(file, `${atPause.trim()}\n# --- the learner asked: ${question || 'explain this'} ---\n${scene}`);
    if (problems.length) fs.writeFileSync(path.join(dir, 'compute.txt'), problems.join('\n') + '\n');
    const spoken = scene.split('\n').filter((l) => l.trim().startsWith('Say:'))
      .map((l) => l.trim().slice(4).trim().replace(/^"|"$/g, '').replace(/\|[^|\s]*\|/g, '').replace(/\[[^\]\s]*\]/g, ''));
    ui.broadcast({ t: 'explain', state: 'playing', q: question, spoken });
    const w = spawn(tutor, [file, '--window-pos', '120,90', ...voiceArgs()], { cwd: root, stdio: 'ignore' });
    window = w;
    w.on('close', () => {
      if (window === w) window = null;
      ui.broadcast({ t: 'explain', state: 'done', q: question });
    });
    ui.note(`(While the lesson was paused at "${heard.split(' ').slice(-12).join(' ')}", the learner asked "${question ||
      'explain this'}" and a side explanation answered it: "${spoken.join(' ')}". Carry on from where they paused.)`);
  }

  return { explain, close: () => window?.kill() };
}
