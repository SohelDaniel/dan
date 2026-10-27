// Markdown notes -> a PDF to follow on paper: LaTeX math (MathJax, $...$ and $$...$$), mermaid diagrams
// (```mermaid), tables, code, images relative to the file. Printed by the installed Google Chrome; nothing online.
//
//   node tools/notes/notes-pdf.mjs notes.md [out.pdf] [--html out.html] [--title T]
//
// One-time setup: (cd tools/notes && npm install); math uses tools/tex's MathJax.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { Marked } from 'marked';
import puppeteer from 'puppeteer-core';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const MATHJAX = path.resolve(HERE, '../tex/node_modules/mathjax-full/es5/tex-svg-full.js');
const MERMAID = path.join(HERE, 'node_modules/mermaid/dist/mermaid.min.js');
const CHROME = process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

// Math is lifted out before markdown sees it (markdown would read its _ and * as emphasis) and put back as text
// MathJax typesets. Code spans and fences are skipped, so a $ in code stays a dollar.
function liftMath(md) {
  const math = [];
  const keep = (tex, display) => `MATH${math.push({ tex, display }) - 1}END`;
  const parts = md.split(/(```[\s\S]*?```|`[^`\n]*`)/);
  for (let i = 0; i < parts.length; i += 2)
    parts[i] = parts[i]
      .replace(/\$\$([\s\S]+?)\$\$/g, (_, t) => keep(t, true))
      .replace(/\\\[([\s\S]+?)\\\]/g, (_, t) => keep(t, true))
      .replace(/\\\(([\s\S]+?)\\\)/g, (_, t) => keep(t, false))
      .replace(/(^|[^\\$])\$(?!\s)([^$\n]+?)(?<!\s)\$(?!\d)/g, (_, pre, t) => pre + keep(t, false));
  return { md: parts.join(''), math };
}
const dropMath = (html, math) => html.replace(/MATH(\d+)END/g, (_, i) => {
  const { tex, display } = math[i];
  return display ? `<div class="math">\\[${esc(tex)}\\]</div>` : `\\(${esc(tex)}\\)`;
});

export function notesHtml(md, { title = 'Notes', base = '' } = {}) {
  const lifted = liftMath(md);
  const marked = new Marked({
    renderer: { code: (code, lang) => lang === 'mermaid' ? `<pre class="mermaid">${esc(code)}</pre>` : false },
  });
  const body = dropMath(marked.parse(lifted.md), lifted.math);
  return `<!doctype html><html><head><meta charset="utf-8"><title>${esc(title)}</title>
${base ? `<base href="${base}">` : ''}
<style>
  @page { size: A4; margin: 20mm 18mm 22mm; }
  body { font: 11pt/1.5 "Iowan Old Style", Charter, Georgia, serif; color: #111; max-width: 46em; margin: 0 auto; }
  h1 { font-size: 20pt; margin: 0 0 .4em; } h1:not(:first-of-type) { break-before: page; }
  h2 { font-size: 14pt; margin: 1.4em 0 .4em; border-bottom: 1px solid #ccc; } h3 { font-size: 12pt; }
  h2, h3 { break-after: avoid; }
  pre, table, .math, pre.mermaid, img, blockquote { break-inside: avoid; }
  code { font: 9.5pt Menlo, monospace; } pre { background: #f5f5f3; padding: .6em .8em; border-radius: 4px; overflow: hidden; white-space: pre-wrap; }
  pre.mermaid { background: none; text-align: center; }
  table { border-collapse: collapse; margin: .8em 0; } th, td { border: 1px solid #bbb; padding: .25em .6em; }
  blockquote { margin: .8em 0; padding: .2em 1em; border-left: 3px solid #888; background: #fafafa; }
  img { max-width: 100%; } .math { margin: .6em 0; overflow: hidden; }
</style></head><body>${body}</body></html>`;
}

export async function notesPdf(mdFile, outPdf, { html: htmlOut, title } = {}) {
  const md = fs.readFileSync(mdFile, 'utf8');
  title ||= (md.match(/^#\s+(.+)$/m) || [])[1] || path.basename(mdFile, '.md');
  const html = notesHtml(md, { title, base: pathToFileURL(path.dirname(path.resolve(mdFile))).href + '/' });
  const page_ = htmlOut || path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'notes-')), 'notes.html');
  fs.writeFileSync(page_, html);
  const browser = await puppeteer.launch({ executablePath: CHROME, headless: true, args: ['--allow-file-access-from-files'] });
  try {
    const page = await browser.newPage();
    await page.goto(pathToFileURL(path.resolve(page_)).href, { waitUntil: 'load' });
    await page.evaluate(() => { window.MathJax = { svg: { fontCache: 'global' }, startup: { typeset: false } }; });
    await page.addScriptTag({ path: MATHJAX });
    await page.addScriptTag({ path: MERMAID });
    // a diagram that does not parse is shown as its source with the error, not dropped
    const problems = await page.evaluate(async () => {
      await window.MathJax.startup.promise;
      await window.MathJax.typesetPromise();
      const bad = [...document.querySelectorAll('mjx-merror')].map((e) => 'math: ' + e.getAttribute('data-mjx-error'));
      window.mermaid.initialize({ startOnLoad: false, theme: 'neutral', securityLevel: 'loose' });
      for (const el of document.querySelectorAll('pre.mermaid')) {
        const src = el.textContent;
        try { await window.mermaid.run({ nodes: [el] }); }
        catch (e) { bad.push('mermaid: ' + String(e.message || e).split('\n')[0]); el.className = ''; el.textContent = src + '\n\n(diagram error: ' + (e.message || e) + ')'; }
      }
      return bad;
    });
    await page.pdf({
      path: outPdf, format: 'A4', printBackground: true, preferCSSPageSize: true, displayHeaderFooter: true,
      headerTemplate: '<span></span>',
      footerTemplate: `<div style="font:8pt Georgia,serif;color:#888;width:100%;text-align:center"><span class="title"></span> · <span class="pageNumber"></span>/<span class="totalPages"></span></div>`,
    });
    return { problems };
  } finally { await browser.close(); }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const argv = process.argv.slice(2);
  const flag = (n) => { const i = argv.indexOf(n); return i >= 0 ? argv.splice(i, 2)[1] : undefined; };
  const html = flag('--html'), title = flag('--title');
  const [src, out = src?.replace(/\.md$/i, '') + '.pdf'] = argv;
  if (!src) { console.error('usage: notes-pdf.mjs notes.md [out.pdf] [--html out.html] [--title T]'); process.exit(2); }
  const { problems } = await notesPdf(src, out, { html, title });
  for (const p of problems) console.error('  ' + p);
  console.log(out);
  process.exit(problems.length ? 1 : 0);
}
