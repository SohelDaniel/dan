// Notes to PDF (tools/notes/notes-pdf.mjs): math is lifted past markdown, diagrams render, a bad diagram is reported.
//   node tests/notes.mjs        (needs Google Chrome and `cd tools/notes && npm install`)
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { notesHtml, notesPdf } from '../tools/notes/notes-pdf.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
let failed = 0;
const check = (name, ok, got) => { if (!ok) failed++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok ? '' : '  got ' + JSON.stringify(got)}`); };

let h = notesHtml('a $x_1 * y_2$ and $x_3*$ b');
check('inline math keeps its _ and * (not read as emphasis)', h.includes('\\(x_1 * y_2\\)') && !h.includes('<em>'), h);
h = notesHtml('costs $5 and `$x$` in code');
check('a lone dollar and code spans stay text', !h.includes('\\(') && h.includes('<code>$x$</code>'), h);
h = notesHtml('$$\na < b\n$$');
check('display math becomes a block, escaped', h.includes('<div class="math">\\[\na &lt; b\n\\]</div>'), h);
h = notesHtml('```mermaid\nflowchart LR\n A-->B\n```');
check('a mermaid fence becomes a diagram', h.includes('<pre class="mermaid">flowchart LR\n A--&gt;B</pre>'), h);

const out = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'notes-test-')), 'sample.pdf');
const { problems } = await notesPdf(path.join(HERE, 'notes', 'sample.md'), out);
const pages = Number((/Pages:\s+(\d+)/.exec(spawnSync('pdfinfo', [out], { encoding: 'utf8' }).stdout) || [])[1]);
check('the sample prints, two pages (the answers start a page)', pages === 2, pages);
check('exactly the one broken diagram is reported', problems.length === 1 && problems[0].startsWith('mermaid:'), problems);

process.exit(failed ? 1 : 0);
