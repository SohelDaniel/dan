// tutor lecture: a long lecture prepared from your files, built part by part, played on the stage.
//
//   tutor lecture new FILE... [--minutes N] [--ask "what to focus on"] [--model opus] [--effort high]
//                                  copy the files in and plan the lecture (prints the outline)
//   tutor lecture plan [ID]        plan again (after changing --minutes / --ask)
//   tutor lecture build [ID] [--parts 3,5]
//                                  write, check, look at and fix each part (hours; resumes where it stopped),
//                                  then join them into lecture.dan and write the notes
//   tutor lecture notes [ID]       write the notes to follow on paper (markdown, LaTeX, diagrams, exercises) -> PDF
//   tutor lecture voice [ID]       fetch the voice of every line ahead (build does it), so it plays at once, offline
//   tutor lecture play [ID]        play it on the stage
//   tutor lecture show [ID]        the outline and where each step stands
//   tutor lecture list             all lectures, newest first
//
// ID is a lecture's folder name in chat/lectures/ or part of it; without one, the newest lecture.
import fs from 'node:fs';
import path from 'node:path';
import { create, find, list, saveMeta, dirOf, readJson } from './store.mjs';
import { planLecture, outlineText, targetMinutes } from './plan.mjs';
import { buildLecture, partFile } from './build.mjs';
import { packageLecture, writeNotes } from './package.mjs';
import { play } from './player.mjs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');

const argv = process.argv.slice(2);
const flag = (n) => { const i = argv.indexOf(n); return i >= 0 ? argv.splice(i, 2)[1] : undefined; };
const only = flag('--parts')?.split(',').map(Number);
const opts = { minutes: Number(flag('--minutes')) || undefined, request: flag('--ask'), model: flag('--model'), effort: flag('--effort') };
const [cmd = 'show', ...rest] = argv;

function need(q) {
  const m = find(q);
  if (!m) { console.error(q ? `no lecture matches "${q}"` : 'no lectures yet: tutor lecture new FILE...'); process.exit(1); }
  for (const [k, v] of Object.entries(opts)) if (v) m[k] = v; // flags given now win, and are kept
  return m;
}

function show(m) {
  const o = readJson(path.join(dirOf(m.id), 'outline.json'), null);
  console.log(`${m.id}  [${m.status}]  ${m.sources.map((s) => s.name).join(', ')}  (${targetMinutes(m)} min target)`);
  if (m.error) console.log(`  error: ${m.error}`);
  if (o) console.log('\n' + outlineText(o));
  if (o) {
    const built = o.parts.map((_, i) => readJson(partFile(m, i + 1).replace(/\.dan$/, '.report.json'), null));
    if (built.some(Boolean)) console.log('\nbuilt: ' + built.map((r, i) => r ? `${i + 1} (~${Math.round(r.seconds / 6) / 10} min, critic ${r.rounds.at(-1)?.review?.score ?? '-'})` : `${i + 1} -`).join(', '));
  }
}

// the voice chosen in the browser (chat/settings.json), as the stage takes it
function voiceArgs() {
  const saved = readJson(path.join(ROOT, 'chat', 'settings.json'), {});
  return [...(saved.voice ? ['--voice-id', saved.voice] : []), ...(saved.speed && saved.speed !== 1 ? ['--speed', String(saved.speed)] : [])];
}

function warmVoice(m) {
  console.log('voice: fetching every line into the cache...');
  const r = spawnSync(path.join(ROOT, 'tutor'), [path.join(dirOf(m.id), 'lecture.dan'), '--prepare-voice', ...voiceArgs()], { cwd: ROOT, stdio: 'inherit' });
  if (r.status !== 0) console.error('voice: some lines failed; they will be fetched when played');
}

async function notes(m) {
  try {
    const { pdf, problems } = await writeNotes(m);
    console.log(`notes: ${pdf}${problems.length ? `  (${problems.length} problems left: ${problems.join('; ')})` : ''}`);
  } catch (e) { console.error(`notes failed: ${e.message}`); process.exitCode = e.limit ? 3 : 1; }
}

async function plan(m) {
  try { await planLecture(m); }
  catch (e) { console.error(`plan failed: ${e.message}`); process.exit(m.status === 'limit' ? 3 : 1); }
  show(m);
  console.log(`\nfolder: ${dirOf(m.id)}`);
}

switch (cmd) {
  case 'new': {
    const files = rest.map((f) => path.resolve(f));
    const missing = files.filter((f) => !fs.existsSync(f));
    if (!files.length || missing.length) { console.error(missing.length ? `not found: ${missing.join(', ')}` : 'tutor lecture new FILE...'); process.exit(1); }
    const m = create(files, Object.fromEntries(Object.entries(opts).filter(([, v]) => v)));
    await plan(m);
    break;
  }
  case 'plan': { const m = need(rest[0]); saveMeta(m); await plan(m); break; }
  case 'build': {
    const m = need(rest[0]);
    let done;
    try { done = await buildLecture(m, { only }); }
    catch (e) { console.error(`build stopped: ${e.message}${m.status === 'limit' ? ' (run the same command again later to go on)' : ''}`); process.exit(m.status === 'limit' ? 3 : 1); }
    if (!done) { show(m); break; }
    packageLecture(m);
    warmVoice(m);
    show(m);
    await notes(m);
    console.log(`\nplay it: tutor lecture play ${m.id}`);
    break;
  }
  case 'notes': await notes(need(rest[0])); break;
  case 'voice': warmVoice(need(rest[0])); break;
  case 'play': {
    const m = need(rest[0]);
    if (m.status !== 'packaged') { // not all built: what is, as a preview
      try { console.log(`preview: parts 1-${packageLecture(m, { preview: true }).length} of the plan`); }
      catch (e) { console.error(`${e.message}: tutor lecture build`); process.exit(1); }
    }
    const voice = [...voiceArgs(), ...rest.slice(1)];
    console.log('space pause · ← back a sentence · shift ← → parts · type to ask (?question while paused) · after a part\'s question, type your answer');
    play(m, { voiceArgs: voice }).on('close', (c) => process.exit(c ?? 0));
    break;
  }
  case 'show': show(need(rest[0])); break;
  case 'list':
    for (const m of list()) console.log(`${m.id}  [${m.status}]  ${m.sources.map((s) => s.name).join(', ')}`);
    break;
  case 'help': case '-h': case '--help':
    console.log(fs.readFileSync(new URL(import.meta.url), 'utf8').split('\n').slice(2).filter((l, i, a) => a.slice(0, i + 1).every((x) => x.startsWith('//'))).map((l) => l.replace(/^\/\/ ?/, '')).join('\n'));
    break;
  default:
    console.error(`unknown: tutor lecture ${cmd}`);
    process.exit(1);
}
