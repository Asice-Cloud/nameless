#!/usr/bin/env python3
import os, subprocess, sys

root = os.path.dirname(os.path.dirname(__file__))
patterns = os.path.join(root, 'patterns')
build = os.path.join(root, 'build')
lwss_rle = os.path.join(patterns, 'lwss.rle')

def parse_rle(path):
    with open(path, 'r') as f:
        lines = [l.rstrip('\n') for l in f if not l.startswith('#')]
    header = ''
    data = ''
    for l in lines:
        if 'x' in l and 'y' in l and header=='':
            header = l
            continue
        data += l.strip()
    x=0; y=0; run=0; coords=[]
    for ch in data:
        if ch.isdigit():
            run = run*10 + int(ch)
            continue
        count = run if run!=0 else 1
        run = 0
        if ch=='b':
            x += count
        elif ch=='o':
            for k in range(count):
                coords.append((x,y))
                x += 1
        elif ch=='$':
            y += count; x = 0
        elif ch=='!':
            break
    return coords

def transform(coords, rot, reflect):
    pts = coords
    def rot90(p):
        x,y = p; return (y, -x)
    new = pts
    for i in range(rot):
        new = [rot90(p) for p in new]
    if reflect:
        new = [(-p[0], p[1]) for p in new]
    # shift to non-negative
    minx = min(p[0] for p in new)
    miny = min(p[1] for p in new)
    new = [(p[0]-minx, p[1]-miny) for p in new]
    return new

def write_coords(path, coords):
    with open(path, 'w') as f:
        f.write('# coords x y\n')
        for x,y in coords:
            f.write(f"{x} {y}\n")

coords = parse_rle(lwss_rle)
variants = []
for reflect in (False, True):
    for rot in range(4):
        v = transform(coords, rot, reflect)
        name = f'lwss_var_r{rot}' + ('_m' if reflect else '')
        p = os.path.join(patterns, name + '.txt')
        write_coords(p, v)
        variants.append(p)

def run_variant(path):
    # clear log
    log = os.path.join(build, 'gameoflife_debug.log')
    try:
        if os.path.exists(log): os.remove(log)
    except Exception:
        pass
    cmd = f'AUTO_PATTERN={path} AUTO_SHIFT=30,0 ./qt_life_cpp'
    proc = subprocess.run(cmd, shell=True, cwd=build, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    # read log
    if not os.path.exists(log):
        return None
    with open(log,'r') as f:
        text = f.read()
    gens = []
    for line in text.splitlines():
        if 'Generation' in line and 'alive=' in line:
            try:
                parts = line.split('alive=')
                n = int(parts[1])
                gens.append(n)
            except Exception:
                pass
    return gens, text

good = []
for v in variants:
    print('Testing', v)
    res = run_variant(v)
    if res is None:
        print('no log')
        continue
    gens, text = res
    print('gens', gens)
    if len(gens)>=4 and all(g==gens[0] and g>0 for g in gens[:4]):
        good.append((v, gens[:8]))

print('\nGood variants:')
for v,gs in good:
    print(v, gs)

if not good:
    print('No stable variant found; showing last log for inspection')
    lastlog = os.path.join(build, 'gameoflife_debug.log')
    if os.path.exists(lastlog):
        print('--- log start ---')
        print(open(lastlog).read())
        print('--- log end ---')
