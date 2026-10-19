// TeX -> glyph outlines for the tutor engine.
//
// A long-lived helper: one JSON request per stdin line, one JSON reply per stdout line.
//   in:  {"tex": "\\frac{a}{b}", "display": true}
//   out: {"ok": true, "w": 1234, "h": 900, "base": 650,
//         "glyphs": [{"k": "1D44E", "s": 0, "x": 12.5, "y": 650, "c": "red"}, ...],
//         "shapes": [{"id": "9f..", "pts": [[x,y,x,y,...], ...]}]}
//      or {"ok": false, "error": "Undefined control sequence \\fracc"}
// Units are MathJax's: 1000 per em, y down, origin at the top-left of the formula box.
// A glyph is shape `s` translated by (x, y); shapes are deduplicated so the engine can
// rasterize each distinct outline once. `k` is the character code (MathJax's data-c), or
// "rect" for rules (fraction bars, radicals), and lets the engine match glyphs across formulas.
import { mathjax } from 'mathjax-full/js/mathjax.js';
import { TeX } from 'mathjax-full/js/input/tex.js';
import { SVG } from 'mathjax-full/js/output/svg.js';
import { liteAdaptor } from 'mathjax-full/js/adaptors/liteAdaptor.js';
import { RegisterHTMLHandler } from 'mathjax-full/js/handlers/html.js';
import { AllPackages } from 'mathjax-full/js/input/tex/AllPackages.js';
import { createHash } from 'node:crypto';
import readline from 'node:readline';

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
const tex = new TeX({
  packages: AllPackages.filter((p) => !['bussproofs', 'noerrors', 'noundefined'].includes(p)),
  formatError: (_jax, err) => { throw err; },
});
const doc = mathjax.document('', { InputJax: tex, OutputJax: new SVG({ fontCache: 'none' }) });

// ---------------------------------------------------------------- affine [a b c d e f]
const mul = (m, n) => [
  m[0] * n[0] + m[2] * n[1], m[1] * n[0] + m[3] * n[1],
  m[0] * n[2] + m[2] * n[3], m[1] * n[2] + m[3] * n[3],
  m[0] * n[4] + m[2] * n[5] + m[4], m[1] * n[4] + m[3] * n[5] + m[5],
];
const ID = [1, 0, 0, 1, 0, 0];
function parseTransform(s) {
  let m = ID;
  if (!s) return m;
  for (const [, fn, args] of s.matchAll(/(\w+)\(([^)]*)\)/g)) {
    const v = args.split(/[\s,]+/).filter(Boolean).map(Number);
    if (fn === 'translate') m = mul(m, [1, 0, 0, 1, v[0] || 0, v[1] || 0]);
    else if (fn === 'scale') m = mul(m, [v[0], 0, 0, v.length > 1 ? v[1] : v[0], 0, 0]);
    else if (fn === 'matrix') m = mul(m, v);
    else if (fn === 'rotate') {
      const r = (v[0] * Math.PI) / 180, c = Math.cos(r), s2 = Math.sin(r);
      m = mul(m, [c, s2, -s2, c, 0, 0]);
    }
  }
  return m;
}

// ---------------------------------------------------------------- path data -> polylines
function flatten(d) {
  const toks = d.match(/[a-zA-Z]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?/g) || [];
  const out = [];
  let cur = null, x = 0, y = 0, sx = 0, sy = 0, cmd = '', i = 0, lcx = 0, lcy = 0, lastCtl = '';
  const num = () => Number(toks[i++]);
  const push = (px, py) => cur.push(px, py);
  const curve = (pts, n) => { // de Casteljau sampling of a bezier with control points pts
    for (let s = 1; s <= n; s++) {
      const t = s / n;
      let p = pts.map((q) => q.slice());
      while (p.length > 1) p = p.slice(1).map((q, k) => [p[k][0] + (q[0] - p[k][0]) * t, p[k][1] + (q[1] - p[k][1]) * t]);
      push(p[0][0], p[0][1]);
    }
  };
  const segs = (pts) => Math.max(2, Math.min(16, Math.ceil(Math.hypot(pts.at(-1)[0] - pts[0][0], pts.at(-1)[1] - pts[0][1]) / 40)));
  while (i < toks.length) {
    if (/[a-zA-Z]/.test(toks[i])) cmd = toks[i++];
    const rel = cmd === cmd.toLowerCase(), C = cmd.toUpperCase();
    const ox = rel ? x : 0, oy = rel ? y : 0;
    if (C === 'Z') { if (cur && cur.length > 4) out.push(cur); cur = null; x = sx; y = sy; lastCtl = ''; continue; }
    if (C === 'M') {
      if (cur && cur.length > 4) out.push(cur);
      x = ox + num(); y = oy + num(); sx = x; sy = y; cur = [x, y];
      cmd = rel ? 'l' : 'L'; lastCtl = ''; continue;
    }
    if (!cur) cur = [x, y];
    if (C === 'L') { x = ox + num(); y = oy + num(); push(x, y); lastCtl = ''; }
    else if (C === 'H') { x = ox + num(); push(x, y); lastCtl = ''; }
    else if (C === 'V') { y = oy + num(); push(x, y); lastCtl = ''; }
    else if (C === 'Q' || C === 'T') {
      let cx, cy;
      if (C === 'Q') { cx = ox + num(); cy = oy + num(); }
      else if (lastCtl === 'Q') { cx = 2 * x - lcx; cy = 2 * y - lcy; }
      else { cx = x; cy = y; }
      const ex = ox + num(), ey = oy + num();
      const pts = [[x, y], [cx, cy], [ex, ey]];
      curve(pts, segs(pts));
      lcx = cx; lcy = cy; x = ex; y = ey; lastCtl = 'Q';
    } else if (C === 'C' || C === 'S') {
      let c1x, c1y;
      if (C === 'C') { c1x = ox + num(); c1y = oy + num(); }
      else if (lastCtl === 'C') { c1x = 2 * x - lcx; c1y = 2 * y - lcy; }
      else { c1x = x; c1y = y; }
      const c2x = ox + num(), c2y = oy + num(), ex = ox + num(), ey = oy + num();
      const pts = [[x, y], [c1x, c1y], [c2x, c2y], [ex, ey]];
      curve(pts, segs(pts));
      lcx = c2x; lcy = c2y; x = ex; y = ey; lastCtl = 'C';
    } else if (C === 'A') { // rare in MathJax: approximate arcs by their chord
      i += 5; x = ox + num(); y = oy + num(); push(x, y); lastCtl = '';
    } else i++; // unknown token: skip
  }
  if (cur && cur.length > 4) out.push(cur);
  return out;
}

// ---------------------------------------------------------------- walk the SVG tree
function convert(src, display) {
  const node = doc.convert(src, { display });
  doc.clear(); // don't accumulate math items between requests
  const svg = adaptor.firstChild(node);
  const vb = (adaptor.getAttribute(svg, 'viewBox') || '0 0 0 0').split(/\s+/).map(Number);
  const [minX, minY, w, h] = vb;
  const glyphs = [], shapes = [], shapeIx = new Map();
  const r1 = (v) => Math.round(v * 10) / 10;

  const addShape = (key, contours) => {
    if (shapeIx.has(key)) return shapeIx.get(key);
    const id = createHash('sha1').update(key).digest('hex').slice(0, 16);
    shapes.push({ id, pts: contours.map((c) => c.map(r1)) });
    shapeIx.set(key, shapes.length - 1);
    return shapes.length - 1;
  };
  // contours under linear part L (no translation), so equal glyphs at equal sizes share a shape
  const emit = (k, keySrc, contours, m, color) => {
    const L = [m[0], m[1], m[2], m[3]];
    const lk = L.map((v) => v.toFixed(4)).join(',');
    const tc = contours.map((c) => {
      const o = new Array(c.length);
      for (let j = 0; j < c.length; j += 2) {
        o[j] = L[0] * c[j] + L[2] * c[j + 1];
        o[j + 1] = L[1] * c[j] + L[3] * c[j + 1];
      }
      return o;
    });
    const s = addShape(keySrc + '|' + lk, tc);
    glyphs.push({ k, s, x: r1(m[4] - minX), y: r1(m[5] - minY), c: color });
  };

  const walk = (n, m, color) => {
    if (adaptor.kind(n) === '#text' || adaptor.kind(n) === '#comment') return;
    const kind = adaptor.kind(n);
    const fill = adaptor.getAttribute(n, 'fill');
    if (fill && fill !== 'currentColor' && fill !== 'none') color = fill;
    let mm = mul(m, parseTransform(adaptor.getAttribute(n, 'transform')));
    if (kind === 'svg' && n !== svg) { // nested svg (mpadded and friends): x/y offset
      mm = mul(mm, [1, 0, 0, 1, Number(adaptor.getAttribute(n, 'x') || 0), Number(adaptor.getAttribute(n, 'y') || 0)]);
    }
    if (kind === 'path') {
      const d = adaptor.getAttribute(n, 'd') || '';
      if (d) emit(adaptor.getAttribute(n, 'data-c') || 'path', d, flatten(d), mm, color);
      return;
    }
    if (kind === 'rect') {
      const rx = Number(adaptor.getAttribute(n, 'x') || 0), ry = Number(adaptor.getAttribute(n, 'y') || 0);
      const rw = Number(adaptor.getAttribute(n, 'width') || 0), rh = Number(adaptor.getAttribute(n, 'height') || 0);
      if (rw > 0 && rh > 0) emit('rect', `rect${rw}x${rh}`, [[0, 0, rw, 0, rw, rh, 0, rh]], mul(mm, [1, 0, 0, 1, rx, ry]), color);
      return;
    }
    if (kind === 'line') { // rules drawn as strokes (\cancel, borders): a thin quad
      const x1 = +adaptor.getAttribute(n, 'x1') || 0, y1 = +adaptor.getAttribute(n, 'y1') || 0;
      const x2 = +adaptor.getAttribute(n, 'x2') || 0, y2 = +adaptor.getAttribute(n, 'y2') || 0;
      const sw = Number(adaptor.getAttribute(n, 'stroke-width') || 40) / 2;
      const len = Math.hypot(x2 - x1, y2 - y1) || 1, nx = (-(y2 - y1) / len) * sw, ny = ((x2 - x1) / len) * sw;
      emit('rect', `line${x1},${y1},${x2},${y2},${sw}`,
        [[x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny]], mm, color);
      return;
    }
    for (const c of adaptor.childNodes(n)) walk(c, mm, color);
  };
  walk(svg, ID, null);
  return { ok: true, w: r1(w), h: r1(h), base: r1(-minY), glyphs, shapes };
}

process.stdout.on('error', () => process.exit(0)); // the engine went away: just stop
const rl = readline.createInterface({ input: process.stdin });
rl.on('line', (line) => {
  let reply;
  try {
    const req = JSON.parse(line);
    reply = convert(String(req.tex ?? ''), req.display !== false);
  } catch (e) {
    reply = { ok: false, error: String(e?.message || e).split('\n')[0] };
  }
  process.stdout.write(JSON.stringify(reply) + '\n');
});
