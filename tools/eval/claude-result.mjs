// The text of a `claude -p --output-format json` reply. A failed call (an error, a session or usage limit) must
// never be scored as if it were an answer: it stops the run, and the rerun picks up from there.
export function claudeResult(out, what) {
  let j = null;
  try { j = JSON.parse(out); } catch {}
  const text = j ? j.result || '' : out;
  if (!j || j.is_error || /hit your (session|usage) limit|usage limit reached/i.test(text.slice(0, 300))) {
    console.error(`${what}: the model call failed, stopping (${text.slice(0, 160).replace(/\s+/g, ' ')})`);
    process.exit(2);
  }
  return text;
}
