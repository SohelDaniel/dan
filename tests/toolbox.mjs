// Every move in chat/toolbox.md is a complete scene the engine accepts: decodable, and no lint other than layout notes.
// Claude copies these patterns, so a broken one teaches broken markup.
//   node tests/toolbox.mjs        (make check runs it)
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const moves = fs.readFileSync(path.join(ROOT, 'chat', 'toolbox.md'), 'utf8').split(/^### /m).slice(1);
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'toolbox-'));
let failed = 0;
for (const move of moves) {
  const title = move.split('\n')[0];
  const code = (move.match(/```\n([\s\S]*?)```/) || [])[1];
  const file = path.join(dir, 'move.dan');
  fs.writeFileSync(file, `title "${title}"\n${code || ''}`);
  const r = spawnSync(path.join(ROOT, 'tutor'), [file, '--check'], { cwd: ROOT, encoding: 'utf8' });
  const out = (r.stdout || '') + (r.stderr || '');
  const problems = out.split('\n').filter((l) => /^(lint|parse):/.test(l) && !/layout:/.test(l));
  if (!code) problems.push('no example');
  if (!/decodable: yes/.test(out)) problems.push('not decodable');
  const ok = problems.length === 0;
  if (!ok) failed++;
  console.log(`${title.slice(0, 52).padEnd(54)} ${ok ? 'ok' : 'FAIL ' + [...new Set(problems)].join(' | ')}`);
}
fs.rmSync(dir, { recursive: true, force: true });
process.exit(failed ? 1 : 0);
