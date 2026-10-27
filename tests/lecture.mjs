// Lecture mode without Claude: the plan's checks, a part's source pages, joining parts into one lecture that plays.
//   node tests/lecture.mjs
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
process.env.TUTOR_LECTURES = fs.mkdtempSync(path.join(os.tmpdir(), 'lectures-test-'));
const { create, dirOf, writeJson } = await import('../chat/lecture/store.mjs');
const { validOutline, targetMinutes } = await import('../chat/lecture/plan.mjs');
const { partFile } = await import('../chat/lecture/build.mjs');
const { packageLecture } = await import('../chat/lecture/package.mjs');
const { suggestMinutes, kindOf, looksLikeText } = await import('../chat/ingest.mjs');

let failed = 0;
const check = (name, ok, got) => { if (!ok) failed++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok ? '' : '  got ' + JSON.stringify(got)}`); };

const part = (title, extra = {}) => ({ title, goal: 'g', frame: 'f', minutes: 2, check: { question: 'q?', answer: 'a' }, ...extra });
check('a whole outline passes', validOutline({ parts: [part('one'), part('two', { needs: [1] })] }).length === 0);
check('a part without a check is caught', validOutline({ parts: [{ ...part('one'), check: {} }] })[0]?.includes('check'));
check('needing a later part is caught', validOutline({ parts: [part('one', { needs: [2] }), part('two')] })[0]?.includes('does not come before'));
check('no parts is caught', validOutline({ parts: [] })[0] === 'no parts');

check('length: never under 15 min', suggestMinutes(500) === 15);
check('length: ~30 pages of text is ~30 min', suggestMinutes(13500) === 30, suggestMinutes(13500));
check('kinds by name', kindOf('a.PDF') === 'pdf' && kindOf('x.png') === 'image' && kindOf('notes.md') === 'text' && kindOf('data.json') === 'text');
check('binary is not text', !looksLikeText(Buffer.from([0x89, 0x50, 0x00, 0x01])) && looksLikeText(Buffer.from('héllo — ok')));

// two built parts, joined: the lecture plays, with a part per scene line
const src = path.join(process.env.TUTOR_LECTURES, 'notes.md');
fs.writeFileSync(src, '# Notes\n\nSome words about counting.\n');
const m = create([src], { minutes: 20 });
check('a lecture keeps its target', targetMinutes(m) === 20 && m.sources[0].words === 6, m);
writeJson(path.join(dirOf(m.id), 'outline.json'), { title: 'Counting', parts: [part('Counting up'), part('Naming')] });
fs.mkdirSync(path.join(dirOf(m.id), 'parts'), { recursive: true });
fs.writeFileSync(partFile(m, 1), 'scene "Part 1: Counting up"\nschema numberline\nmark a dot v=2 label="a"\nSay: "a is two."\nSay: "What is a plus one?"\npause\n');
fs.writeFileSync(partFile(m, 2), 'scene "Part 2: Naming"\nschema graph\nmark x box label="x"\nSay: "Now a box."\n');
let threw = '';
try { packageLecture({ ...m, id: m.id }); } catch (e) { threw = e.message; }
check('packaging joins every part', !threw, threw);
const lecture = fs.readFileSync(path.join(dirOf(m.id), 'lecture.dan'), 'utf8');
const index = JSON.parse(fs.readFileSync(path.join(dirOf(m.id), 'index.json'), 'utf8'));
const lines = lecture.split('\n');
check('the index points at each part\'s first line', index.parts.every((p) => lines[p.line - 1] === `scene "Part ${p.n}: ${p.title}"`), index.parts.map((p) => lines[p.line - 1]));
const chk = spawnSync(path.join(ROOT, 'tutor'), [path.join(dirOf(m.id), 'lecture.dan'), '--check'], { encoding: 'utf8' });
check('the joined lecture is decodable', /decodable: yes/.test(chk.stdout), chk.stdout);
fs.rmSync(partFile(m, 2));
threw = '';
try { packageLecture(m); } catch (e) { threw = e.message; }
check('packaging refuses a lecture with a part missing', threw.includes('2'), threw);

fs.rmSync(process.env.TUTOR_LECTURES, { recursive: true, force: true });
process.exit(failed ? 1 : 0);
