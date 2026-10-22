// Python blocks: computation that hands DATA to the scene, never drawings. A reply may carry
//
//   ```python id=walk seed=7
//   steps = [0]
//   for _ in range(20): steps.append(steps[-1] + random.choice([-1, 1]))
//   out(steps=steps, end=steps[-1])
//   ```
//
// and scene lines use the data by name: `mark w array items={walk.steps}`, `Say: "It ends at {walk.end}."`,
// `each v in walk.steps: mark s{i} dot score={v}`. The server runs the block, then writes the values into the lines
// before they reach the engine, so the engine (its readback, its prescan, every axiom) only ever sees literal data.
//
// Each block runs in a fresh temporary folder under macOS sandbox-exec (no network, no writes outside that folder, no
// reading the home folder, where the keys and the chats are), with CPU and file-size limits and a wall-clock kill.
// Results are cached by a hash of the code and seed.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawn, spawnSync } from 'node:child_process';

const LIMITS = { cpuSeconds: 4, wallMs: 5000, outputBytes: 256 * 1024, eachMax: 200 };
const PYTHON = fs.realpathSync(spawnSync('/usr/bin/which', ['python3'], { encoding: 'utf8' }).stdout.trim() || '/usr/bin/python3');
const [PY_VERSION, PY_HOME] = spawnSync(PYTHON, ['-I', '-c', 'import sys; print(sys.version.split()[0]); print(sys.base_prefix)'],
  { encoding: 'utf8' }).stdout.trim().split('\n'); // Homebrew's python3 re-launches itself from its framework

// what the block's code runs inside: limits, the seed, and out() as the only way to hand data back
const RUNNER = `import json, random, resource, sys
resource.setrlimit(resource.RLIMIT_CPU, (__CPU__, __CPU__))
resource.setrlimit(resource.RLIMIT_FSIZE, (1 << 20, 1 << 20))
random.seed(__SEED__)
_data = {}
def out(**values):
    _data.update(values)
import math, statistics, itertools, fractions
exec(compile(open('block.py').read(), 'block.py', 'exec'), {'out': out, 'random': random, 'math': math, 'statistics': statistics,
     'itertools': itertools, 'fractions': fractions, '__name__': '__main__'})
sys.stdout.write('\\x1eDATA' + json.dumps(_data))
`;

function sandboxProfile(dir) {
  return `(version 1)
(allow default)
(deny network*)
(deny file-write*)
(allow file-write* (subpath "${dir}"))
(deny file-read* (subpath "${os.homedir()}"))
(allow file-read* (subpath "${dir}"))
(deny process-exec)
(allow process-exec (literal "${PYTHON}") (subpath "${fs.realpathSync(PY_HOME)}"))`;
}

// data must be plain: numbers, strings, booleans, lists and objects of them
function plain(v, depth = 0) {
  if (depth > 6) return false;
  if (v === null || ['number', 'string', 'boolean'].includes(typeof v)) return typeof v !== 'number' || Number.isFinite(v);
  if (Array.isArray(v)) return v.every((x) => plain(x, depth + 1));
  if (typeof v === 'object') return Object.values(v).every((x) => plain(x, depth + 1));
  return false;
}

// run one block: { data } or { error }
export function runPython(code, { seed = 0, cacheDir = path.join(process.cwd(), '.tutor_cache', 'compute') } = {}) {
  const key = crypto.createHash('sha256').update(`${PY_VERSION}\n${seed}\n${code}`).digest('hex').slice(0, 32);
  const cached = path.join(cacheDir, key + '.json');
  if (fs.existsSync(cached)) return Promise.resolve(JSON.parse(fs.readFileSync(cached, 'utf8')));
  const dir = fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), 'tutor-py-')));
  fs.writeFileSync(path.join(dir, 'block.py'), code);
  fs.writeFileSync(path.join(dir, 'runner.py'), RUNNER.replaceAll('__CPU__', String(LIMITS.cpuSeconds)).replace('__SEED__', String(Number(seed) || 0)));
  return new Promise((resolve) => {
    const p = spawn('/usr/bin/sandbox-exec', ['-p', sandboxProfile(dir), PYTHON, '-I', 'runner.py'],
      { cwd: dir, env: { PATH: '/usr/bin:/bin', HOME: dir, LANG: 'C.UTF-8' } });
    let out = '', err = '', done = false;
    const finish = (result) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      fs.rmSync(dir, { recursive: true, force: true });
      if (result.data) { fs.mkdirSync(cacheDir, { recursive: true }); fs.writeFileSync(cached, JSON.stringify(result)); }
      resolve(result);
    };
    const timer = setTimeout(() => { p.kill('SIGKILL'); finish({ error: `took longer than ${LIMITS.wallMs / 1000} s` }); }, LIMITS.wallMs);
    p.stdout.on('data', (d) => { out += d; if (out.length > LIMITS.outputBytes) { p.kill('SIGKILL'); finish({ error: 'output too large' }); } });
    p.stderr.on('data', (d) => (err += d));
    p.on('close', (status, signal) => {
      const at = out.lastIndexOf('\x1eDATA');
      if (signal === 'SIGXCPU' || signal === 'SIGKILL') return finish({ error: `stopped: more than ${LIMITS.cpuSeconds} s of computing` });
      if (status !== 0 || at < 0) return finish({ error: (err.trim().split('\n').pop() || `exited with ${status}`).slice(0, 300) });
      let data;
      try { data = JSON.parse(out.slice(at + 5)); } catch { return finish({ error: 'out() values must be JSON' }); }
      if (!plain(data)) return finish({ error: 'out() takes numbers, strings, lists and dicts of them' });
      finish({ data });
    });
  });
}

// ---------------------------------------------------------------- writing data into the markup
const fmtNum = (x) => (Number.isInteger(x) ? String(x) : String(Number(x.toPrecision(6))));
function literal(v) {
  if (typeof v === 'number') return fmtNum(v);
  if (typeof v === 'boolean') return v ? '1' : '0';
  if (typeof v === 'string') return /[\s,"=#{}[\]]/.test(v) || v === '' ? `"${v.replaceAll('"', "'")}"` : v;
  if (Array.isArray(v)) return '[' + v.map(literal).join(', ') + ']';
  return null; // a dict has no literal: name one of its keys
}
function spoken(v) { return typeof v === 'number' ? fmtNum(v) : typeof v === 'string' ? v : null; }

// walk.steps, walk.steps[3], walk.rows[0].x, walk.steps[-1]
function lookup(data, ref) {
  const m = ref.match(/^(\w+)((?:\.\w+|\[-?\d+\])*)$/);
  if (!m || !(m[1] in data)) return { missing: false, value: undefined };
  let v = data[m[1]];
  for (const step of m[2].match(/\.\w+|\[-?\d+\]/g) || []) {
    if (v === undefined || v === null) break;
    if (step[0] === '.') v = v[step.slice(1)];
    else { const k = Number(step.slice(1, -1)); v = Array.isArray(v) ? v[k < 0 ? v.length + k : k] : undefined; }
  }
  return { missing: v === undefined, value: v };
}

// One scene line with the data written in. Returns the lines to send (an each-line becomes many) and any problems.
// data: { blockId: {key: value, …}, … }. A {…} that doesn't start with a block id is left for the engine.
export function substitute(line, data) {
  const problems = [];
  const fill = (text, scope, inSay) =>
    text.replace(/(\\val)?\{([\w.[\]-]+)\}/g, (whole, val, ref) => {
      ref = ref.replace(/\[([A-Za-z_]\w*)\]/g, (b, v) => (typeof scope[v] === 'number' ? `[${scope[v]}]` : b)); // walk.x[i]
      const [head] = ref.split(/[.[]/);
      const pool = head in scope ? scope : head in data ? data : null;
      if (!pool) return whole; // an engine expression ({k*x^2}, {a[2]})
      const { value, missing } = head in scope && !ref.includes('.') && !ref.includes('[') ? { value: scope[head] } : lookup(pool, ref);
      const lit = value === undefined ? null : inSay ? spoken(value) : literal(value);
      if (missing || lit === null) { problems.push(`{${ref}}: ${missing ? 'no such value' : 'not a single value here'}`); return whole; }
      return lit;
    });
  // each v in walk.steps: <line>, possibly cued or alongside: "@go each …", "with each …" (every line it makes is too)
  const each = line.match(/^(\s*(?:@\w+\s+)?(?:with\s+)?)each\s+(\w+)\s+in\s+([\w.[\]-]+)\s*:\s*(.+)$/);
  if (!each) {
    const isSay = line.trim().startsWith('Say:');
    const out = fill(line, {}, isSay);
    return { lines: problems.length ? [] : [out], problems };
  }
  const [, prefix, name, ref, raw] = each;
  const template = generated(raw, ref.split(/[.[]/).filter(Boolean).pop());
  const { value } = lookup(data, ref);
  if (!Array.isArray(value)) return { lines: [], problems: [`each: ${ref} is not a list`] };
  if (value.length > LIMITS.eachMax) problems.push(`each: ${ref} has ${value.length} items, only the first ${LIMITS.eachMax} are drawn`);
  const lines = value.slice(0, LIMITS.eachMax).map((item, i) => {
    const scope = { [name]: item, i };
    return prefix + fill(withLoopValues(template, scope), scope, false);
  });
  return { lines: problems.some((p) => !p.startsWith('each:')) ? [] : lines, problems };
}

// Marks made by one each-line are one kind of thing (the dictionary's rule: same kind, same colour): they get one
// category, the list's name, when the line gives none; and no name drawn from their made-up ids (s0, s1, …).
function generated(template, listName) {
  const t = template.trim().match(/(?:[^\s"]+|"[^"]*")+/g) || []; // words, a quoted value staying whole
  if (t[0] !== 'mark' || t.length < 3 || t[2].includes('=')) return template; // not a mark with a kind
  const hasCategory = t.slice(3).some((w) => !w.includes('=') && !w.startsWith('"'));
  if (!hasCategory) t.splice(3, 0, listName.replace(/\W/g, '') || 'item');
  if (!t.some((w) => w.startsWith('label='))) t.push('label=""');
  return t.join(' ');
}

// inside an engine expression, the loop's numbers are written in: {i+1} -> {3+1}, {v.x*2} -> {0.5*2}
function withLoopValues(template, scope) {
  return template.replace(/\{([^{}]*)\}/g, (whole, inner) => {
    if (/^[\w.[\]-]+$/.test(inner)) return whole; // a plain reference: fill() reads it
    const filled = inner.replace(/\b([A-Za-z_]\w*)(?:\.(\w+))?/g, (m, n, field) => {
      const v = field !== undefined ? scope[n]?.[field] : scope[n];
      return typeof v === 'number' ? String(v) : m;
    });
    // only numbers and arithmetic left: the number itself (an id like q{i+4} must be one)
    if (/^[\d\s.+\-*/()]+$/.test(filled)) {
      const n = Function(`return (${filled})`)();
      if (Number.isFinite(n)) return fmtNum(n);
    }
    return '{' + filled + '}';
  });
}

// a block's fence line: ```python id=walk seed=7
export function blockHeader(fence) {
  const id = (fence.match(/\bid=(\w+)/) || [])[1];
  const seed = Number((fence.match(/\bseed=(-?\d+)/) || [])[1] || 0);
  return { id, seed };
}

// For a whole reply at once (the eval): run its python blocks, then write their data into the scene.
export async function expandReply(reply) {
  const data = {}, problems = [];
  for (const m of reply.matchAll(/```python([^\n]*)\n([\s\S]*?)```/g)) {
    const { id, seed } = blockHeader(m[1]);
    if (!id) { problems.push('python block without id='); continue; }
    const r = await runPython(m[2], { seed });
    if (r.error) problems.push(`python ${id}: ${r.error}`);
    else data[id] = r.data;
  }
  const scene = (reply.match(/```\s*scene\b[^\n]*\n([\s\S]*?)```/) || [])[1] || '';
  const lines = [];
  for (const line of scene.split('\n')) {
    const r = substitute(line, data);
    lines.push(...r.lines);
    problems.push(...r.problems);
  }
  return { scene: lines.join('\n'), data, problems };
}
