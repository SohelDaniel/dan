import re, sys, glob, os, collections
files = []
for pat in sys.argv[1:]: files += glob.glob(pat)
tex_change = re.compile(r'^(?:@\w+\s+|with\s+)*(?:mark\s+(\w+)\s+tex\b|morph\s+(\w+)\.tex\b)')
sayre = re.compile(r'^Say:\s*"(.*)"')
tot_say = 0; alg_say = 0; multi = 0; runs = 0; moved_on = 0; restated = 0; steps = 0
words_alg = []; examples = []
for fn in files:
    lines = [l.rstrip() for l in open(fn, errors='ignore')]
    beats = []  # (say_text, [tex ids changed in this beat])
    cur = None
    for l in lines:
        m = sayre.match(l)
        if m:
            cur = [m.group(1), []]; beats.append(cur); continue
        if cur is None: continue
        t = tex_change.match(l)
        if t: cur[1].append(t.group(1) or t.group(2))
    for i, (text, ids) in enumerate(beats):
        tot_say += 1
        if not ids: continue
        alg_say += 1; steps += len(ids)
        words_alg.append(len(text.split()))
        if len(ids) >= 2: multi += 1
        if i + 1 < len(beats):
            nxt, nids = beats[i+1]
            if nids: runs += 1
            fid = ids[-1]
            refers = (f'[{fid}]' in nxt) or re.search(r'\{[^}]*\b' + re.escape(fid) + r'\b', nxt) or re.search(r'\b(so|that is|notice|read|now we have|in other words|look)\b', nxt, re.I)
            if refers: restated += 1
            else: moved_on += 1
        if len(ids) >= 3 and len(examples) < 6: examples.append((os.path.basename(os.path.dirname(os.path.dirname(fn))) if 'turns' in fn else fn, text[:110], len(ids)))
print(f"files={len(files)} Say lines={tot_say}  sentences that change a formula={alg_say} ({100*alg_say/max(1,tot_say):.0f}%)  formula changes total={steps}")
print(f"  sentences with 2+ formula changes: {multi} ({100*multi/max(1,alg_say):.0f}% of formula sentences); mean words of a formula sentence: {sum(words_alg)/max(1,len(words_alg)):.1f}")
print(f"  formula sentence followed by another formula sentence: {runs} ({100*runs/max(1,alg_say):.0f}%)")
print(f"  next sentence refers back to the formula / restates: {restated}; moves on without touching it: {moved_on}")
for e in examples: print("  e.g.", e)
