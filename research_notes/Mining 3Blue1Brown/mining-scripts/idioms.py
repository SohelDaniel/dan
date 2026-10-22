import json, re, sys, collections
B=[json.loads(l) for l in open(sys.argv[1])]
LEX=[ # (category, regex on the start of the comment); first match within first 5 words wins, phrases checked first
 ('add',      r'set ?up|setup|initiali[sz]e|initial'),
 ('zoom',     r'zoom|reorient|frame|camera|pan|look at'),
 ('compare',  r'compare|contrast|vs\.?|versus|side by side|alternate'),
 ('swap',     r'swap|exchange|shuffle|permute|reorder|interchange'),
 ('split',    r'split|break|separate|divide|decompose|cut|partition'),
 ('combine',  r'combine|merge|join|collect|gather|group|stack|sum up|add up|consolidate'),
 ('connect',  r'connect|link|attach|bridge'),
 ('flow',     r'flow|trace|travel|sweep|cycle|ambient|simulat\w*|run|play|animate|iterate|repeat|vary|wiggle|oscillat\w*|spin|roll|drop|throw|flip|toss|sample'),
 ('count',    r'count|tally|enumerate|number|calculat\w*|compute|evaluat\w*|sum|multiply|tick'),
 ('grow',     r'grow|increase|extend|stretch|enlarge|raise|scale up|expand'),
 ('shrink',   r'shrink|squish|collapse|decrease|lower|compress|squeeze|scale down|reduce'),
 ('highlight',r'highlight|emphasi[sz]e|indicate|circle|flash|point|focus|isolate|underline|light up|color|mark|accent|single out|call out|draw attention|frame'),
 ('label',    r'label|name|annotate|title|caption|tag'),
 ('write',    r'write|rewrite|spell'),
 ('transform',r'transform|change|replace|turn into|morph|convert|become|switch|substitute|plug|simplify|factor|apply|map|swap out|transition|update|tweak|adjust|modify|edit|set'),
 ('move',     r'move|shift|bring|pull|push|slide|go|walk|rotate|turn|center|rearrange|organi[sz]e|align|arrange|put|place|lift|position|scroll'),
 ('remove',   r'remove|clear|fade|clean|hide|erase|get rid|kill|dismiss|drop'),
 ('restore',  r'back|return|restore|revert|undo|reset'),
 ('show',     r'show|display|reveal|present|introduce|preview|demonstrate|illustrate|see|view|visuali[sz]e'),
 ('add',      r'add|create|draw|make|build|construct|fill|insert|new|generate|spawn|bring in|pop'),
 ('talk',     r'ask|question|comment|mention|talk|discuss|describe|say|reference|explain|why|what|how|note|react|respond|wonder|think|answer|reply|ponder|hint|claim'),
 ('define',   r'define|let|choose|pick|suppose|assume|consider|declare'),
 ('example',  r'example|try|check|test|verify|confirm'),
]
LEXC=[(c,re.compile(r'^(?:'+p+r')\w{0,3}\b')) for c,p in LEX]
def classify(t):
    t=t.lower(); t=re.sub(r'^[#\s]*(\d+[.)]|step \d+:?|part \d+:?|first,?|then,?|now,?|next,?|finally,?|and|also|maybe|just|quickly|slowly)\s*','',t)
    words=t.split()
    for i in range(min(5,len(words))):
        rest=' '.join(words[i:])
        for c,rx in LEXC:
            if rx.match(rest): return c
    return 'other'
NOTANIM={'VGroup','Group','Animation','VMobject','Mobject','Tex','Text','TexText','OldTex','OldTexText','Dot','Line','Arrow','Circle','Square','Rectangle','SurroundingRectangle','Brace','Integer','DecimalNumber','Point','Vector','Axes','NumberLine','Polygon','Randolph','Mortimer','PiCreature','TextMobject','TexMobject','ValueTracker'}
# animation vocabulary: names used directly as play args
direct=collections.Counter(x for b in B for s in b['calls'] for x in s)
VOC={x for x,n in direct.items() if n>=5 and x[0].isupper() and x not in NOTANIM}
ANAME=re.compile(r'\b('+'|'.join(sorted(VOC,key=len,reverse=True))+r')\(')
ANIM=re.compile(r'(frame|camera\.frame)?\.animate\.(\w+)')
def bag(b):
    s=set()
    for seq in b['calls']:
        for x in seq:
            if x not in ('var',) and not x.startswith('call.') and x not in NOTANIM: s.add(x)
            if x=='call.change' or x=='call.change_students': s.add('PiCreature.change')
    for m in ANAME.finditer(b['code']): s.add(m.group(1))
    for m in ANIM.finditer(b['code']): s.add(('frame.animate.' if m.group(1) else 'animate.')+m.group(2))
    s={('ValueTracker.set_value' if x in('animate.set_value','animate.increment_value','ChangeDecimalToValue') else x) for x in s}
    s={('frame.animate.*' if x.startswith('frame.animate.') else x) for x in s}
    return s
rows=collections.defaultdict(list)
for b in B:
    if b['plays']==0: continue
    b['verb']=classify(b['comment']); b['bag']=sorted(bag(b))
    if b['bag']: rows[b['verb']].append(b)
glob=collections.Counter(x for v in rows.values() for b in v for x in b['bag']); NB=sum(len(v) for v in rows.values())
with open(sys.argv[2],'w') as f:
    f.write('verb\tn_blocks\ttop1\ttop2\ttop3\ttop3_excl_FadeIn/Out\tdistinctive(lift>=1.5, n>=5)\texample_comments\n')
    for verb,bl in sorted(rows.items(),key=lambda kv:-len(kv[1])):
        c=collections.Counter(x for b in bl for x in b['bag']); n=len(bl)
        top=[f'{x} {100*k/n:.0f}%' for x,k in c.most_common(3)]
        topnf=[f'{x} {100*k/n:.0f}%' for x,k in c.most_common(10) if x not in ('FadeIn','FadeOut')][:3]
        lift=sorted([(k/n)/(glob[x]/NB),x,k] for x,k in c.items() if k>=5)
        dist=[f'{x} x{l:.1f}' for l,x,k in reversed(lift) if l>=1.5][:3]
        ex=' | '.join(b['comment'][:50] for b in bl[:: max(1,n//3)][:3])
        f.write(f'{verb}\t{n}\t'+'\t'.join(top+['']*(3-len(top)))+'\t'+', '.join(topnf)+'\t'+', '.join(dist)+'\t'+ex+'\n')
json.dump([{k:b[k] for k in ('year','file','cls','fn','line','comment','verb','bag')} for v in rows.values() for b in v],open(sys.argv[3],'w'))
print('blocks with plays & anims:',NB,' other:',len(rows['other']))

# fine table: lead verb word
fine=collections.defaultdict(list)
for v in rows.values():
    for b in v:
        w=re.sub(r'[^a-z]','',(b['comment'].lower().split() or [''])[0])
        if w: fine[w].append(b)
with open(sys.argv[2].replace('.tsv','_fine.tsv'),'w') as f:
    f.write('lead_word\tcategory\tn_blocks\ttop3_excl_FadeIn/Out\tdistinctive\n')
    for w,bl in sorted(fine.items(),key=lambda kv:-len(kv[1]))[:60]:
        c=collections.Counter(x for b in bl for x in b['bag']); n=len(bl)
        topnf=[f'{x} {100*k/n:.0f}%' for x,k in c.most_common(10) if x not in ('FadeIn','FadeOut')][:3]
        lift=sorted([(k/n)/(glob[x]/NB),x,k] for x,k in c.items() if k>=3)
        dist=[f'{x} x{l:.1f}' for l,x,k in reversed(lift) if l>=1.5][:3]
        f.write(f"{w}\t{collections.Counter(b['verb'] for b in bl).most_common(1)[0][0]}\t{n}\t{', '.join(topnf)}\t{', '.join(dist)}\n")
