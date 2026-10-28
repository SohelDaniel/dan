// Registers the tutor's MCP server where Claude looks for tools: Claude Code (user scope, so every session has it)
// and the Claude desktop app (claude_desktop_config.json; the app reads it at launch). `tutor mcp install [-y]`.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import readline from 'node:readline/promises';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const SERVER = path.join(HERE, 'server.mjs');
const DESKTOP = path.join(os.homedir(), 'Library', 'Application Support', 'Claude', 'claude_desktop_config.json');
const yes = process.argv.includes('-y');
const entry = { command: process.execPath, args: [SERVER] };

if (!fs.existsSync(path.join(HERE, 'node_modules'))) {
  console.log('installing the MCP SDK (once)...');
  spawnSync('npm', ['install', '--silent'], { cwd: HERE, stdio: 'inherit' });
}

// Claude Code: `claude mcp add` at user scope (an existing entry is replaced)
const code = spawnSync('claude', ['mcp', 'remove', '--scope', 'user', 'tutor'], { encoding: 'utf8' });
const add = spawnSync('claude', ['mcp', 'add', '--scope', 'user', 'tutor', '--', process.execPath, SERVER], { encoding: 'utf8' });
console.log(add.status === 0 ? 'Claude Code: tutor tools added for every session (claude mcp list to check)' : `Claude Code: claude mcp add failed: ${(add.stderr || add.stdout).trim()}`);
void code;

// Claude desktop app: merge into its config, keeping everything else
if (fs.existsSync(path.dirname(DESKTOP))) {
  let cfg = {};
  try { cfg = JSON.parse(fs.readFileSync(DESKTOP, 'utf8')); } catch {}
  const same = JSON.stringify(cfg.mcpServers?.tutor) === JSON.stringify(entry);
  if (same) console.log('Claude desktop app: already set');
  else {
    let ok = yes;
    if (!ok) {
      const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
      ok = /^y/i.test(await rl.question(`Claude desktop app: add the tutor to ${DESKTOP}? [y/N] `));
      rl.close();
    }
    if (ok) {
      cfg.mcpServers = { ...(cfg.mcpServers || {}), tutor: entry };
      fs.copyFileSync(DESKTOP, DESKTOP + '.bak');
      fs.writeFileSync(DESKTOP, JSON.stringify(cfg, null, 2) + '\n');
      console.log('Claude desktop app: added (quit and reopen the app; the tools show under the tools icon). Backup: ' + DESKTOP + '.bak');
    } else console.log('Claude desktop app: skipped');
  }
} else console.log('Claude desktop app: not installed (no config folder)');
console.log('claude.ai in the browser cannot reach this machine; use the desktop app there.');
