"""Extract (step comment -> animation calls) blocks from 3Blue1Brown's scene code.
Each block is a prose comment in a scene method and the code under it, with the animations it plays, the scene's base
classes and flags for what our markup can't express (3D, Pi creatures, images, objects generated in loops).
  python3 -I tools/mine/extract.py tools/mine/work tools/mine/work/blocks.jsonl
Output JSONL: one record per comment block."""
import ast, os, re, sys, json, warnings
warnings.filterwarnings('ignore', category=SyntaxWarning)  # 3b1b's TeX strings use "\p" and friends
ROOT=sys.argv[1]; OUT=sys.argv[2]
V=os.path.join(ROOT,'videos')
CONTAINERS={'LaggedStart','AnimationGroup','Succession','LaggedStartMap'}
CODEY=re.compile(r'(self\.|\w\(|\)\s*$|=|\bdef\b|\bimport\b|^\s*[\w.]+\s*$|^[\w.]+\.\w+|lambda|\[|\])')
def dotted(n):
    parts=[]
    while isinstance(n,ast.Attribute): parts.append(n.attr); n=n.value
    if isinstance(n,ast.Name): parts.append(n.id)
    elif isinstance(n,ast.Call): parts.append('()')
    return list(reversed(parts))
def desc_anim(a, out):
    if isinstance(a,ast.Starred): a=a.value
    if isinstance(a,(ast.GeneratorExp,ast.ListComp)): a=a.elt
    if isinstance(a,(ast.List,ast.Tuple)):
        for e in a.elts: desc_anim(e,out); return
    if isinstance(a,ast.Call):
        d=dotted(a.func)
        if 'animate' in d:
            i=d.index('animate'); meth=d[i+1] if i+1<len(d) else '?'
            tgt='frame' if any(x in ('frame','camera') for x in d[:i]) else ''
            out.append(('frame.animate.' if tgt else 'animate.')+meth); return
        name=d[-1] if d else '?'
        if name in CONTAINERS:
            out.append(name)
            for e in a.args:
                if isinstance(e,ast.Name) and e.id[:1].isupper(): out.append(e.id)  # LaggedStartMap(FadeIn, ...)
                else: desc_anim(e,out)
            return
        if name and name[0].isupper(): out.append(name); return
        out.append('call.'+name)  # e.g. mob.some_method_returning_anim()
        return
    if isinstance(a,ast.Name): out.append('var'); return
    if isinstance(a,ast.Attribute): out.append('animate.'+a.attr); return  # old style self.play(mob.shift, UP)
FLAGS={  # what the tutor's markup has no counterpart for
    '3d': r'ThreeD|Sphere\(|Surface\(|Cube\(|Prism|set_euler|reorient|Torus|ParametricSurface',
    'pi': r'PiCreature|Randolph|Mortimer|pi_creature|\bstudents?\b|\bteacher\b|\.says\(|\.thinks\(|\.change\(\s*["\']',
    'image': r'ImageMobject|SVGMobject|SVG\(|get_image|\.png|\.jpg',
    'loop': r'for \w+ in range\(\s*\d{3,}|for \w+ in range\(\s*\w+\s*\)',
}
def is_prose(t):
    if len(re.findall(r'[A-Za-z]{2,}',t))<1: return False
    if CODEY.search(t): return False
    if re.match(r'(TODO|todo|noqa|type:|pragma|Old|old|Test|test|Note|NOTE|Hack|hack)\b',t): return False
    return True
def process(path, year, out):
    src=open(path,errors='replace').read()
    try: tree=ast.parse(src)
    except Exception: return
    lines=src.split('\n')
    for cls in [n for n in ast.walk(tree) if isinstance(n,ast.ClassDef)]:
        for fn in [n for n in cls.body if isinstance(n,ast.FunctionDef)]:
            # comments in this fn
            comments=[]  # (lineno, indent, text)
            prev=None
            for ln in range(fn.body[0].lineno if fn.body else fn.lineno, fn.end_lineno+1):
                s=lines[ln-1]; m=re.match(r'^(\s*)#\s?(.*)$',s)
                if m:
                    t=m.group(2).strip()
                    if prev and prev[0]==ln-1 and prev[1]==len(m.group(1)):
                        prev[2]+=' '+t; prev[0]=ln; continue
                    prev=[ln,len(m.group(1)),t,ln]; comments.append(prev)
                else: prev=None
            comments=[c for c in comments if is_prose(c[2])]
            if not comments: continue
            blocks=[{'comment':c[2],'line':c[3],'indent':c[1],'calls':[],'plays':0,'adds':0,'waits':0,'end':fn.end_lineno} for c in comments]
            for i,b in enumerate(blocks[:-1]): b['end']=blocks[i+1]['line']-1
            for node in ast.walk(fn):
                if not isinstance(node,ast.Call): continue
                d=dotted(node.func)
                if d[:1]!=['self'] or len(d)!=2: 
                    continue
                verb=d[1]
                # owning block: last comment before the line with indent <= statement indent
                ind=len(lines[node.lineno-1])-len(lines[node.lineno-1].lstrip())
                own=None
                for b in blocks:
                    if b['line']<node.lineno and b['indent']<=ind: own=b
                if own is None or node.lineno>own['end'] and False: continue
                if verb=='play':
                    own['plays']+=1
                    seq=[]
                    for a in node.args: desc_anim(a,seq)
                    own['calls'].append(seq)
                elif verb=='add': own['adds']+=1
                elif verb=='wait': own['waits']+=1
            bases=[ '.'.join(dotted(x)) for x in cls.bases ]
            for b in blocks:
                code='\n'.join(lines[b['line']:min(b['end'],b['line']+40)])
                flags=[k for k,rx in FLAGS.items() if re.search(rx, code) or (k=='3d' and any('ThreeD' in x for x in bases))
                       or (k=='pi' and any(re.search(r'PiCreature|TeacherStudents',x) for x in bases))]
                out.write(json.dumps({'year':year,'file':os.path.relpath(path,V),'cls':cls.name,'fn':fn.name,'line':b['line'],
                    'comment':b['comment'],'bases':bases,'flags':flags,'calls':b['calls'],'plays':b['plays'],'adds':b['adds'],'waits':b['waits'],'code':code})+'\n')
with open(OUT,'w') as out:
    for y in sorted(os.listdir(V)):
        if not y.startswith('_'): continue
        for dp,_,fn in os.walk(os.path.join(V,y)):
            for f in sorted(fn):
                if f.endswith('.py'): process(os.path.join(dp,f),y[1:],out)
