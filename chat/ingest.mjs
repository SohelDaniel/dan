// Files the learner gives the tutor: what kind each is, how Claude reads it, and how long a lecture it is worth.
// Shared by the chat (attachments) and lecture mode (sources).
//   images -> image blocks; PDFs -> document blocks (Claude sees the pages, figures too) and text by page;
//   anything that reads as text (markdown, JSON, CSV, code, ...) -> a text block named after the file.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const IMAGE = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif', '.webp': 'image/webp' };
const TEXT_LIMIT = 400_000; // characters of one text file sent to Claude; more is cut, with a note

export function kindOf(name, type = '') {
  const ext = path.extname(name).toLowerCase();
  if (type === 'application/pdf' || ext === '.pdf') return 'pdf';
  if (type.startsWith('image/') || IMAGE[ext]) return 'image';
  return 'text';
}

// A file reads as text when it has no NUL bytes and decodes as UTF-8 (binary files are refused, not mangled).
export function looksLikeText(buf) {
  const head = buf.subarray(0, 8192);
  if (head.includes(0)) return false;
  return !new TextDecoder('utf-8').decode(head, { stream: true }).includes('\uFFFD'); // stream: a character cut at the end is fine
}

// JSON is re-indented so its structure is visible; a long array keeps its first items and says how many there were.
function readableJson(src) {
  let data;
  try { data = JSON.parse(src); } catch { return src; }
  const trim = (v, depth) => {
    if (Array.isArray(v)) {
      const kept = v.slice(0, 50).map((x) => trim(x, depth + 1));
      if (v.length > 50) kept.push(`... ${v.length - 50} more items (${v.length} in all)`);
      return kept;
    }
    if (v && typeof v === 'object') return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, trim(x, depth + 1)]));
    return v;
  };
  return JSON.stringify(trim(data, 0), null, 2);
}

// The text of a file. A PDF's pages are marked "[page N]" so a plan can cite them.
export function textOf(file) {
  const kind = kindOf(file.name, file.type);
  if (kind === 'image') return '';
  if (kind === 'pdf') {
    const r = spawnSync('pdftotext', ['-layout', file.path, '-'], { encoding: 'utf8', maxBuffer: 1 << 28 });
    if (r.status !== 0) return '';
    return r.stdout.split('\f').map((p, i) => (p.trim() ? `[page ${i + 1}]\n${p.trimEnd()}` : '')).filter(Boolean).join('\n\n');
  }
  const src = fs.readFileSync(file.path, 'utf8');
  return path.extname(file.name).toLowerCase() === '.json' ? readableJson(src) : src;
}

export function pageCount(file) {
  const r = spawnSync('pdfinfo', [file.path], { encoding: 'utf8' });
  const m = /^Pages:\s+(\d+)/m.exec(r.stdout || '');
  return m ? Number(m[1]) : 0;
}

// How Claude is given the file: content blocks for one user message.
export function blocksFor(file) {
  const kind = kindOf(file.name, file.type);
  if (kind === 'image' || kind === 'pdf') {
    const media = kind === 'pdf' ? 'application/pdf' : (file.type || IMAGE[path.extname(file.name).toLowerCase()]);
    return [{ type: kind === 'pdf' ? 'document' : 'image', source: { type: 'base64', media_type: media, data: fs.readFileSync(file.path).toString('base64') } }];
  }
  let text = textOf(file);
  if (text.length > TEXT_LIMIT) text = text.slice(0, TEXT_LIMIT) + `\n... (cut: the file has ${text.length} characters)`;
  return [{ type: 'text', text: `<file name="${file.name}">\n${text}\n</file>` }];
}

export const label = (file) => ({ pdf: 'PDF', image: 'image', text: 'text' })[kindOf(file.name, file.type)];

// Spoken English runs ~150 words a minute, and a lecture says roughly a third as many words as it teaches from;
// below 15 minutes it is not a lecture. The plan shows the number and the learner can change it.
export const WORDS_PER_MINUTE = 150;
export function suggestMinutes(sourceWords, images = 0) {
  return Math.max(15, Math.round((sourceWords / 3 + images * 60) / WORDS_PER_MINUTE / 5) * 5);
}
export const wordCount = (text) => (text.match(/\S+/g) || []).length;
