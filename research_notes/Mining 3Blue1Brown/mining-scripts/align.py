"""Align caption sentences to scenes (and step blocks) of one video.
usage: align.py sentence_timings.json out_prefix code1.py [code2.py ...]"""
import ast, json, math, re, sys, collections
caps=json.load(open(sys.argv[1])); OUT=sys.argv[2]; files=sys.argv[3:]
STOP=set("the a an of to and in is that it this for on with as you be are we what but so at by or not your if can its it's have from there which about one would just they these how all like when more some then out get any do has was way here also into than our now been were will very much really kind sort thing things going let's lets let say said same other each well even only really self def none true false color width height opacity stroke fill run_time lag_ratio buff left right down up np range len mob mobs group vgroup".split())
TEXCMD={'sigma':'standard deviation sigma','mu':'mean','pi':'pi','sum':'sum','int':'integral','frac':'fraction','sqrt':'square root','infty':'infinity','cdot':'times','rightarrow':'','text':'','left':'','right':''}
def stem(w):
    for suf in ('ations','ation','ings','ing','ies','es','ed','ly','s'):
        if len(w)>len(suf)+3 and w.endswith(suf): return w[:-len(suf)]
    return w
def toks(s):
    s=re.sub(r'\\([a-zA-Z]+)',lambda m:' '+TEXCMD.get(m.group(1),m.group(1))+' ',s)
    s=re.sub(r'([a-z])([A-Z])',r'\1 \2',s).replace('_',' ')
    return [stem(w) for w in re.findall(r"[a-z]{3,}",s.lower()) if w not in STOP]
# --- scenes
scenes=[]  # dict(name,file,idx,text Counter, blocks=[(comment, code, text)])
for fi,f in enumerate(files):
    src=open(f,errors='replace').read(); lines=src.split('\n')
    try: tree=ast.parse(src)
    except Exception: continue
    k=0
    for cls in tree.body:
        if not isinstance(cls,ast.ClassDef): continue
        meths=[n for n in cls.body if isinstance(n,ast.FunctionDef)]
        bases=[ast.unparse(b) for b in cls.bases]
        if not any('Scene' in b or 'Wrapper' in b or b in {s['name'] for s in scenes} for b in bases): continue
        if any(b in ('VGroup','VMobject','Group') for b in bases): continue
        strs=[n.value for n in ast.walk(cls) if isinstance(n,ast.Constant) and isinstance(n.value,str) and len(n.value)>1]
        body='\n'.join(lines[cls.lineno-1:cls.end_lineno])
        comments=re.findall(r'#\s*(.+)',body)
        idents=[n.id for n in ast.walk(cls) if isinstance(n,ast.Name)]+[n.attr for n in ast.walk(cls) if isinstance(n,ast.Attribute)]+[a.arg for n in ast.walk(cls) if isinstance(n,ast.FunctionDef) for a in n.args.args]
        text=toks(' '.join(strs+comments+[cls.name]*2+[m.name for m in meths]))+toks(' '.join(set(idents)))
        par=[p for p in scenes if p['name'] in bases]
        if par: text+=list(par[0]['text'].elements())
        # step blocks: comment lines starting a block, inside the class
        blocks=[]; cur=None
        for ln in range(cls.lineno,cls.end_lineno+1):
            m=re.match(r'^(\s+)#\s*(.+)$',lines[ln-1])
            if m and not re.search(r'self\.|\w\(|=',m.group(2)) and not re.match(r'(?i)(test|todo|old)\b',m.group(2)):
                if cur and cur['end']==ln-1 and cur['code']=='': cur['comment']+=' '+m.group(2); cur['end']=ln; continue
                cur={'comment':m.group(2),'line':ln,'end':ln,'code':''}; blocks.append(cur)
            elif cur: cur['code']+=lines[ln-1]+'\n'
        for b in blocks:
            bs=re.findall(r'"([^"\n]{2,})"|\'([^\'\n]{2,})\'',b['code'])
            b['text']=collections.Counter(toks(b['comment']+' '+' '.join(a or c for a,c in bs)))
        if not blocks and par: blocks=par[0]['blocks']
        if not blocks and meths:
            blocks=[{'comment':'(no step comments) '+cls.name,'line':cls.lineno,'code':body,'text':collections.Counter(text)}]
        scenes.append({'name':cls.name,'file':f.split('/')[-1],'fi':fi,'idx':k,'text':collections.Counter(text),'blocks':blocks,'nplay':body.count('self.play(')})
        k+=1
# --- tf-idf
sents=[c[0] for c in caps]; S=len(sents)
stoks=[collections.Counter(toks(s)) for s in sents]
docs=[sc['text'] for sc in scenes]+stoks
df=collections.Counter(); [df.update(set(d)) for d in docs]; N=len(docs)
def vec(c):
    v={w:(1+math.log(n))*math.log((N+1)/(1+df[w])) for w,n in c.items()}
    nm=math.sqrt(sum(x*x for x in v.values())) or 1; return {w:x/nm for w,x in v.items()}
def cos(a,b): return sum(x*b.get(w,0) for w,x in a.items())
SV=[vec(sc['text']) for sc in scenes]
win=[vec(stoks[i]+collections.Counter({w:0.5*n for w,n in (stoks[i-1] if i else {}).items()})+collections.Counter({w:0.5*n for w,n in (stoks[i+1] if i+1<S else {}).items()})) for i in range(S)]
E=[[cos(win[i],SV[j]) for j in range(len(scenes))] for i in range(S)]
own=[[cos(vec(stoks[i]),SV[j]) for j in range(len(scenes))] for i in range(S)]
if '--z' in sys.argv[0:1] or True:
    import statistics
    for j in range(len(scenes)):
        col=[E[i][j] for i in range(S)]; mu=statistics.mean(col); sd=statistics.pstdev(col) or 1
        for i in range(S): E[i][j]=0.1*(E[i][j]-mu)/sd   # z-score, scaled to cosine range
# --- (a) independent argmax
argmax=[max(range(len(scenes)),key=lambda j:E[i][j]) for i in range(S)]
# --- (b) per-file monotone alignment with gaps (each file = its own ordered track; overlay files interleave)
J=len(scenes); NULL=J
NULLE=float(dict(a.split('=') for a in sys.argv if '=' in a).get('null','0.1')) if False else 0.1
def track(fi):
    idx=[j for j in range(J) if scenes[j]['fi']==fi]; K=len(idx)
    if K==0: return [None]*S
    # states: 0..K-1 = in scene k ; K..2K = gap after scene (k-K-1), K = gap before first
    NS=2*K+1
    def em(i,s): return E[i][idx[s]] if s<K else NULLE
    def tr(a,b):
        if a<K and b<K: d=b-a; return 0 if d==0 else (-0.1 if d==1 else (-0.1-0.05*min(d,6) if d>1 else -9))
        if a<K and b>=K: return -0.05 if b-K-1==a else -9      # leave scene a into gap_a
        if a>=K and b>=K: return 0 if a==b else -9
        if a>=K and b<K: last=a-K-1; return -0.05 if b>last else -9
        return -9
    sc=[em(0,s)+(0 if s in (0,K) else -0.3) for s in range(NS)]; bps=[]
    for i in range(1,S):
        new=[];bp=[]
        for b in range(NS):
            a=max(range(NS),key=lambda a:sc[a]+tr(a,b)); new.append(sc[a]+tr(a,b)+em(i,b)); bp.append(a)
        sc=new; bps.append(bp)
    s=max(range(NS),key=lambda x:sc[x]); p=[s]
    for bp in reversed(bps): s=bp[s]; p.append(s)
    p=p[::-1]
    return [idx[x] if x<K else None for x in p]
tracks=[track(fi) for fi in range(len(files))]
path=[]
for i in range(S):
    cands=[t[i] for t in tracks if t[i] is not None]
    path.append(max(cands,key=lambda j:E[i][j]) if cands else NULL)
# --- confidence: segment-level evidence
segs=[]; i=0
while i<S:
    j=i
    while j+1<S and path[j+1]==path[i]: j+=1
    segs.append((i,j,path[i])); i=j+1
conf=[None]*S
for a,b,st in segs:
    if st==NULL:
        for i in range(a,b+1): conf[i]='null'
        continue
    seg_own=[own[i][st] for i in range(a,b+1)]
    anchors=sum(1 for i in range(a,b+1) if own[i][st]>=0.12 and argmax[i]==st)
    for i in range(a,b+1):
        conf[i]='high' if anchors>=2 or (anchors>=1 and b-a<=3) else ('anchor' if own[i][st]>=0.12 else 'low')
# --- within-scene block assignment (monotonic DP over blocks)
blk=[None]*S
for a,b,st in segs:
    if st==NULL: continue
    BL=scenes[st]['blocks']; BV=[vec(x['text']) for x in BL]; n=b-a+1; m=len(BL)
    if m==0: continue
    sim=[[cos(vec(stoks[a+i]),BV[k]) - 0.05*abs(k/m - i/n) for k in range(m)] for i in range(n)]
    dp=[[-1e9]*m for _ in range(n)]; bk=[[0]*m for _ in range(n)]
    for k in range(m): dp[0][k]=sim[0][k]-0.02*k
    for i in range(1,n):
        best=-1e9;bi=0
        for k in range(m):
            if dp[i-1][k]>best: best=dp[i-1][k];bi=k
            dp[i][k]=best+sim[i][k]; bk[i][k]=bi
    k=max(range(m),key=lambda k:dp[n-1][k]); ks=[k]
    for i in range(n-1,0,-1): k=bk[i][k]; ks.append(k)
    for i,k in enumerate(reversed(ks)): blk[a+i]=(k,sim[i][k])
with open(OUT+'_align.tsv','w') as f:
    f.write('i\tstart\tsentence\tscene\tconf\tsent_sim\targmax_scene\tblock_comment\tblock_sim\n')
    for i in range(S):
        st=path[i]; nm='NULL' if st==NULL else scenes[st]['file']+':'+scenes[st]['name']
        bc=scenes[st]['blocks'][blk[i][0]]['comment'] if blk[i] else ''
        f.write(f"{i}\t{caps[i][1]}\t{sents[i]}\t{nm}\t{conf[i]}\t{own[i][st] if st<J else 0:.2f}\t{scenes[argmax[i]]['name']}\t{bc}\t{blk[i][1] if blk[i] else 0:.2f}\n")
json.dump([{'i':i,'start':caps[i][1],'sentence':sents[i],'scene':('NULL' if path[i]==NULL else scenes[path[i]]['file']+':'+scenes[path[i]]['name']),'conf':conf[i],
  'block':(scenes[path[i]]['blocks'][blk[i][0]]['comment'] if blk[i] else None),'code':(scenes[path[i]]['blocks'][blk[i][0]]['code'][:1500] if blk[i] else None)} for i in range(S)],open(OUT+'_pairs.json','w'),indent=1)
c=collections.Counter(conf); used=len(set(p for p in path if p!=NULL))
print(f'{OUT}: sentences={S} scenes={J} scenes_used={used} segments={len(segs)} conf={dict(c)}')
print('scene order visited:',' > '.join(dict.fromkeys(scenes[p]["name"] for p in path if p!=NULL)))
