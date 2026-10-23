import re, sys, os, collections
root = sys.argv[1]
tex_anim = re.compile(r'TransformMatching(Tex|Strings|Shapes)|TransformFromCopy|ReplacementTransform|FadeTransform')
point = re.compile(r'Indicate|FlashAround|FlashUnder|SurroundingRectangle|Circumscribe|ShowPassingFlash|set_color\(YELLOW|animate\.set_color')
per_play = collections.Counter(); seq = collections.Counter(); n = 0
for dp, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith('.py'): continue
        year = dp.split('_')[-1][:4] if '_20' in dp else ''
        if year and year < '2019': continue
        lines = open(os.path.join(dp, f), errors='ignore').read().split('\n')
        stmts = []; i = 0
        while i < len(lines):
            l = lines[i]
            if 'self.play(' in l or 'self.wait(' in l:
                buf = l; depth = l.count('(') - l.count(')'); j = i
                while depth > 0 and j+1 < len(lines):
                    j += 1; buf += '\n' + lines[j]; depth += lines[j].count('(') - lines[j].count(')')
                stmts.append(('wait' if 'self.wait(' in l and 'self.play(' not in l else 'play', buf)); i = j + 1
            else: i += 1
        for k, (kind, text) in enumerate(stmts):
            if kind != 'play': continue
            c = len(tex_anim.findall(text))
            if c: per_play[min(c, 3)] += 1
            if c:
                n += 1
                # what are the next two statements after the transform (+ optional wait)?
                nxt = [s for s in stmts[k+1:k+4]]
                pat = ''.join('W' if s[0]=='wait' else ('T' if tex_anim.search(s[1]) else ('P' if point.search(s[1]) else 'O')) for s in nxt)
                seq[pat[:3]] += 1
print("formula transforms per play (1 / 2 / 3+):", dict(sorted(per_play.items())))
print("what follows a formula transform (W=wait, T=another formula step, P=pointing, O=other), top patterns of the next 3 statements:")
for p, c in seq.most_common(12): print(f"  {p:4s} {c:5d} ({100*c/n:.0f}%)")
