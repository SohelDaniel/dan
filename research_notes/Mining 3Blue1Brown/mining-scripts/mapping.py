import os, re, sys, json, math, collections
ROOT=sys.argv[1]; V=os.path.join(ROOT,'videos'); C=os.path.join(ROOT,'captions')
STOP=set("the a an of to and in is that it this for on with as you be are we what but so at by or not your if can its it's i have from there which about one would just they this these how all like when more some then its out get any do has was way here also into than our now".split())
def toks(s): return [w for w in re.findall(r"[a-z]{3,}",s.lower()) if w not in STOP]
# code units
units={}
for y in sorted(os.listdir(V)):
    if not y.startswith('_') : continue
    for e in os.listdir(os.path.join(V,y)):
        p=os.path.join(V,y,e); txt=[]
        files=[p] if p.endswith('.py') else [os.path.join(dp,f) for dp,_,fn in os.walk(p) for f in fn if f.endswith('.py')]
        if not files: continue
        for f in files:
            src=open(f,errors='replace').read()
            txt+=re.findall(r'"([^"\n]{3,})"|\'([^\'\n]{3,})\'',src) and [a or b for a,b in re.findall(r'"([^"\n]{3,})"|\'([^\'\n]{3,})\'',src)]
            txt+=re.findall(r'#\s*(.+)',src)
            txt+=re.findall(r'class (\w+)',src)
        t=' '.join(txt); t=re.sub(r'([a-z])([A-Z])',r'\1 \2',t).replace('_',' ')
        units[(y[1:],e)]=collections.Counter(toks(t)+toks(e.replace('_',' '))*5)
caps={}
for y in sorted(d for d in os.listdir(C) if d.isdigit()):
    for v in os.listdir(os.path.join(C,y)):
        tp=os.path.join(C,y,v,'english','transcript.txt')
        if os.path.exists(tp): caps[(y,v)]=collections.Counter(toks(open(tp,errors='replace').read())+toks(v.replace('-',' '))*5)
N=len(units); df=collections.Counter()
for c in units.values(): df.update(set(c))
for c in caps.values(): df.update(set(c))
N+=len(caps)
def vec(c): 
    v={w:(1+math.log(n))*math.log(N/(1+df[w])) for w,n in c.items()}
    nm=math.sqrt(sum(x*x for x in v.values())) or 1
    return {w:x/nm for w,x in v.items()}
UV={k:vec(c) for k,c in units.items()}
out=[]
for k,c in sorted(caps.items()):
    cv=vec(c); y=int(k[0])
    sc=[]
    for uk,uv in UV.items():
        if abs(int(uk[0])-y)>1: continue
        sc.append((sum(x*uv.get(w,0) for w,x in cv.items()),uk))
    sc.sort(reverse=True)
    (s1,u1),(s2,u2)=sc[0],sc[1]
    out.append((k,u1,round(s1,3),u2,round(s2,3),round(s1/max(s2,1e-6),2)))
for r in out: print('\t'.join(map(str,[f'{r[0][0]}/{r[0][1]}',f'{r[1][0]}/{r[1][1]}',r[2],f'{r[3][0]}/{r[3][1]}',r[4],r[5]])))
