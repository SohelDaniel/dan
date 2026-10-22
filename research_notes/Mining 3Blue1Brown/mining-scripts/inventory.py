import ast, os, re, sys, json, collections
ROOT=sys.argv[1]
V=os.path.join(ROOT,'videos')
stats=collections.defaultdict(lambda: collections.Counter())
for y in sorted(os.listdir(V)):
    if not y.startswith('_'): continue
    for dp,dn,fn in os.walk(os.path.join(V,y)):
        for f in fn:
            if not f.endswith('.py'): continue
            p=os.path.join(dp,f); src=open(p,errors='replace').read()
            stats[y]['files']+=1
            try: tree=ast.parse(src); stats[y]['parsed']+=1
            except Exception: tree=None
            stats[y]['play_calls']+=len(re.findall(r'self\.play\(',src))
            stats[y]['classes']+=len(re.findall(r'^class \w+',src,re.M))
            if tree:
                for n in ast.walk(tree):
                    if isinstance(n,ast.ClassDef):
                        if any(isinstance(b,ast.FunctionDef) and b.name=='construct' for b in n.body): stats[y]['scenes_with_construct']+=1
            # step comments: indented comment lines (inside methods)
            stats[y]['indented_comments']+=len(re.findall(r'^[ \t]{8,}#\s*\S',src,re.M))
tot=collections.Counter()
print('year\tfiles\tparsed\tclasses\tscenes(construct)\tplay()\tstep_comments')
for y in sorted(stats):
    s=stats[y]; tot+=s
    print(f"{y}\t{s['files']}\t{s['parsed']}\t{s['classes']}\t{s['scenes_with_construct']}\t{s['play_calls']}\t{s['indented_comments']}")
s=tot;print(f"TOTAL\t{s['files']}\t{s['parsed']}\t{s['classes']}\t{s['scenes_with_construct']}\t{s['play_calls']}\t{s['indented_comments']}")
