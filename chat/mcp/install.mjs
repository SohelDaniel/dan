// Registers the tutor's MCP server with the Claude desktop app (claude_desktop_config.json; the app reads it at
// launch), and writes chat/mcp/mcp.json for Claude Code, which loads it only when asked (`tutor claude --tools`, or
// `claude --mcp-config <that file>` anywhere), never in every session. `tutor mcp install [-y]`.
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

// Claude Code: a config file to load on demand (and no user-scope entry, so ordinary sessions stay light)
const MCP_JSON = path.join(HERE, 'mcp.json');
fs.writeFileSync(MCP_JSON, JSON.stringify({ mcpServers: { tutor: entry } }, null, 2) + '\n');
spawnSync('claude', ['mcp', 'remove', '--scope', 'user', 'tutor'], { encoding: 'utf8' });
console.log(`Claude Code: load the tools when you want them: tutor claude --tools, or claude --mcp-config ${MCP_JSON}`);

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
