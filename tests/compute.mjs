// Python blocks (chat/compute.mjs): the sandbox holds, data comes back as data, and scene lines get literal values.
//   node tests/compute.mjs        (make test runs it)
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runPython, substitute, expandReply } from '../chat/compute.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const cacheDir = fs.mkdtempSync(path.join(os.tmpdir(), 'compute-test-'));
let failed = 0;
const check = (name, ok, got) => { if (!ok) failed++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok ? '' : '  got ' + JSON.stringify(got)}`); };
const run = (code, seed = 0) => runPython(code, { seed, cacheDir });

// data
let r = await run('steps = [0]\nfor _ in range(5): steps.append(steps[-1] + random.choice([-1, 1]))\nout(steps=steps, end=steps[-1])', 7);
check('a seeded walk comes back as data', Array.isArray(r.data?.steps) && r.data.steps.length === 6, r);
const again = await run('steps = [0]\nfor _ in range(5): steps.append(steps[-1] + random.choice([-1, 1]))\nout(steps=steps, end=steps[-1])', 7);
check('the same code and seed give the same data', JSON.stringify(again) === JSON.stringify(r), again);
r = await run('out(r=round(math.sqrt(2), 3), m=statistics.mean([1, 2, 3]))');
check('math and statistics need no import', r.data?.r === 1.414 && r.data?.m === 2, r);
r = await run('out(f=open)');
check('only plain data can be handed back', !!r.error, r);

// the sandbox
r = await run('import socket\nsocket.create_connection(("1.1.1.1", 80), timeout=2)');
check('no network', !!r.error, r);
r = await run(`out(k=open(${JSON.stringify(path.join(ROOT, '.inworld_key'))}).read())`);
check('no reading the home folder (keys, chats)', !!r.error && !r.data, r);
r = await run(`open(${JSON.stringify(path.join(os.homedir(), 'tutor-sandbox-test.txt'))}, "w").write("x")`);
check('no writing outside its folder', !!r.error && !fs.existsSync(path.join(os.homedir(), 'tutor-sandbox-test.txt')), r);
r = await run('import subprocess\nout(r=subprocess.run(["ls", "/"], capture_output=True).returncode)');
check('no running other programs', !!r.error, r);
r = await run('while True: pass');
check('a runaway block is stopped', /stopped/.test(r.error || ''), r);

// data into the markup
const data = { walk: { steps: [0, 1, 41, 124], end: 124, rows: [{ x: 1, y: 2 }, { x: 3, y: 4 }] } };
const same = (line, want) => { const got = substitute(line, data); check(`${line} -> ${want.join(' / ')}`, JSON.stringify(got.lines) === JSON.stringify(want), got); };
same('mark w array items={walk.steps}', ['mark w array items=[0, 1, 41, 124]']);
same('Say: "It ends at {walk.end}, after {walk.steps[-2]}."', ['Say: "It ends at 124, after 41."']);
same('mark f tex tex="x = \\val{walk.end} + \\val{k*2}"', ['mark f tex tex="x = 124 + \\val{k*2}"']);
same('mark f path fn={k*x^2}', ['mark f path fn={k*x^2}']); // the engine's own expressions are left alone
same('each r in walk.rows: mark p{i} dot x={r.x} y={r.y}', ['mark p0 dot x=1 y=2', 'mark p1 dot x=3 y=4']);
const missing = substitute('mark z array items={walk.nope}', data);
check('a missing value drops the line and says why', missing.lines.length === 0 && missing.problems.length === 1, missing);

// a whole reply, as the eval reads it
const reply = '```python id=sq\nout(v=[n * n for n in range(4)])\n```\n```scene\nmark a array items={sq.v}\n```';
const ex = await expandReply(reply);
check('a reply\'s block runs and its scene gets the values', ex.scene.trim() === 'mark a array items=[0, 1, 4, 9]' && !ex.problems.length, ex);

fs.rmSync(cacheDir, { recursive: true, force: true });
process.exit(failed ? 1 : 0);
