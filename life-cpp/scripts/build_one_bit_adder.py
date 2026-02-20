#!/usr/bin/env python3
"""
Build a starter one-bit adder layout by combining two gliders with a reflector and an eater.
This script parses simple RLE files (`reflectors.rle`, `eaters.rle`) and the glider coord
file, places components at chosen offsets, and writes `patterns/modules/one_bit_adder_built.txt`.

This is an iterative helper: run the Qt binary with AUTO_PATTERN pointing to the output file
and inspect `build/gameoflife_debug.log` to refine offsets.
"""
import os

ROOT = os.path.dirname(os.path.dirname(__file__))
OUT = os.path.join(ROOT, 'patterns', 'modules', 'one_bit_adder_built.txt')
GLIDER = os.path.join(ROOT, 'patterns', 'glider.txt')
REF = os.path.join(ROOT, 'patterns', 'modules', 'reflectors.rle')
EATER = os.path.join(ROOT, 'patterns', 'modules', 'eaters.rle')

def read_coords(path):
    pts=[]
    with open(path) as f:
        for line in f:
            line=line.strip()
            if not line or line.startswith('#'): continue
            x,y = map(int, line.split())
            pts.append((x,y))
    return pts

def parse_rle(path):
    # crude RLE parser: supports header with x=,y= and only 'o', '$', digits, '!' tokens
    with open(path) as f:
        lines=[l.strip() for l in f if not l.startswith('#')]
    header=None
    i=0
    while i<len(lines):
        if lines[i].startswith('x') or 'x=' in lines[i]:
            header=lines[i]
            i+=1
            break
        i+=1
    if header is None:
        # fallback: try first non-empty line
        header=lines[0]
        i=1
    data=''.join(lines[i:])
    x=0;y=0;row=0;col=0
    pts=[]
    num=''
    r=0
    c=0
    curx=0;cury=0
    cur_row=0
    j=0
    while j<len(data):
        ch=data[j]
        if ch.isdigit():
            num+=ch
        elif ch=='o':
            n=int(num) if num else 1
            for k in range(n):
                pts.append((curx, cur_row))
                curx+=1
            num=''
        elif ch=='$':
            n=int(num) if num else 1
            cur_row += n
            curx = 0
            num=''
        elif ch=='!':
            break
        j+=1
    # normalize to have min coords at 0,0
    if not pts:
        return []
    minx = min(p[0] for p in pts)
    miny = min(p[1] for p in pts)
    pts = [(x-minx, y-miny) for x,y in pts]
    return pts

def write_output(allpts):
    with open(OUT,'w') as f:
        f.write('# one-bit adder built pattern\n')
        for x,y in allpts:
            f.write(f"{x} {y}\n")
    print('Wrote', OUT)

def main():
    gl = read_coords(GLIDER)
    ref = parse_rle(REF)
    eater = parse_rle(EATER)

    # chosen offsets (tweak these after testing)
    # place two gliders entering from left/top-right
    A = [(x, y) for x,y in gl]
    B = [(x+14, y-4) for x,y in gl]
    # place a reflector slightly to the right of collision point
    R = [(x+10, y+2) for x,y in ref]
    # place an eater to clear stray debris
    E = [(x+6, y+6) for x,y in eater]

    allpts = A+B+R+E
    # shift so all coords positive
    minx = min(p[0] for p in allpts)
    miny = min(p[1] for p in allpts)
    allpts = [(x-minx+20, y-miny+20) for x,y in allpts]

    write_output(allpts)

if __name__=='__main__':
    main()
