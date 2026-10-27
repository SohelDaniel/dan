// One Claude call for lecture mode: content blocks in (text, PDFs, images), the reply's text out.
// Run with no tools on the subscription, as the chat does. A failed call says why, and a session or usage limit is
// marked, so a long build stops cleanly and resumes later instead of saving an error as a part.
import { spawn } from 'node:child_process';

export const CLEAN_ENV = Object.fromEntries(Object.entries(process.env)
  .filter(([k]) => !/^CLAUDECODE$|^CLAUDE_CODE_|^CLAUDE_PID$|^CLAUDE_EFFORT$/.test(k)));

const LIMIT = /hit your (session|usage) limit|usage limit reached/i;

// -> { text, error, limit, secs, sid }. resume: a session id from an earlier call, to go on in that conversation.
export function ask({ system, content, model = 'opus', effort = 'high', cwd = process.cwd(), resume }) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const args = ['-p', '--input-format', 'stream-json', '--output-format', 'stream-json', '--verbose',
      '--tools', '', '--strict-mcp-config', '--model', model, '--effort', effort];
    if (system) args.push('--append-system-prompt', system);
    if (resume) args.push('--resume', resume);
    const p = spawn('claude', args, { cwd, env: CLEAN_ENV });
    let buf = '', text = '', error = '', err = '', sid = resume;
    p.stdout.on('data', (d) => {
      buf += d;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        let e; try { e = JSON.parse(line); } catch { continue; }
        if (e.session_id) sid = e.session_id;
        if (e.type === 'result') e.is_error ? (error = String(e.result || e.subtype)) : (text = e.result || '');
      }
    });
    p.stderr.on('data', (d) => { err = (err + d).slice(-2000); });
    p.stdin.on('error', () => {});
    p.on('close', (code) => {
      if (!text && !error) error = err.trim().split('\n').pop() || `claude exited with ${code}`;
      const limit = LIMIT.test(error) || LIMIT.test(text.slice(0, 300));
      if (limit && !error) { error = text.slice(0, 200); text = ''; }
      resolve({ text, error, limit, secs: (Date.now() - t0) / 1000, sid });
    });
    p.stdin.end(JSON.stringify({ type: 'user', message: { role: 'user', content } }) + '\n');
  });
}

// The body of the first ```name fence in a reply ('' if none).
export function fence(reply, name) {
  const m = reply.match(new RegExp('```\\s*' + name + '\\b[^\\n]*\\n([\\s\\S]*?)\\n```'));
  return m ? m[1] : '';
}
