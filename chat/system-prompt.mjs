// The tutor's system prompt, built one way for the chat server and the eval alike: the protocol (prompt.md) with the
// worked examples and the toolbox where it says {{EXAMPLES}}, then the language reference (the README's "The language").
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const CHAT = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(CHAT, '..');

export const defaults = {
  prompt: path.join(CHAT, 'prompt.md'),
  examples: path.join(CHAT, 'examples'), // a few whole scenes, each line saying why
  toolbox: path.join(CHAT, 'toolbox.md'), // the moves, by what they show, to be composed
};

// examples / toolbox: a folder / file, or null to leave them out
export function buildSystem({ prompt = defaults.prompt, examples = defaults.examples, toolbox = defaults.toolbox } = {}) {
  let worked = '';
  if (examples && fs.existsSync(examples))
    for (const f of fs.readdirSync(examples).filter((f) => f.endsWith('.txt')).sort())
      worked += `\n### ${f.slice(0, -4)}\n\n\`\`\`scene\n${fs.readFileSync(path.join(examples, f), 'utf8').trim()}\n\`\`\`\n`;
  if (toolbox && fs.existsSync(toolbox)) worked += '\n' + fs.readFileSync(toolbox, 'utf8') + '\n';
  const readme = fs.readFileSync(path.join(ROOT, 'README.md'), 'utf8');
  const language = readme.slice(readme.indexOf('## The language'), readme.indexOf('## The dictionary'));
  return fs.readFileSync(prompt, 'utf8').replace('{{EXAMPLES}}', worked) + language;
}

// --examples DIR, --toolbox FILE ("none" leaves either out), as both the server and the eval accept them
export function systemFromArgs(argv, overrides = {}) {
  const arg = (name) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : undefined; };
  const pick = (name, dflt) => { const v = arg(name); return v === undefined ? dflt : v === 'none' ? null : path.resolve(v); };
  return buildSystem({
    prompt: pick('--prompt', overrides.prompt ?? defaults.prompt),
    examples: pick('--examples', 'examples' in overrides ? overrides.examples : defaults.examples),
    toolbox: pick('--toolbox', 'toolbox' in overrides ? overrides.toolbox : defaults.toolbox),
  });
}
