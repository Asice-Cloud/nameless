#!/usr/bin/env python3
"""Generate a simple Fibonacci glider display pattern.

This places small glider seeds in groups where the n-th group contains
F(n) gliders (n starting at 1). Gliders are spaced to avoid early
collisions so the pattern visualizes Fibonacci numbers as separate
streams of gliders.
"""
from pathlib import Path

patterns = Path(__file__).resolve().parents[1] / 'patterns'
out = patterns / 'fibonacci_gun.txt'

# Fibonacci sequence (first 7 terms)
F = [1,1,2,3,5,8]

# base glider coordinates (from patterns/glider.txt)
glider = [(0,0),(1,0),(2,0),(2,1),(1,2)]

coords = []
for i,count in enumerate(F):
    group_x = 5 + i*20
    group_y = 5 + i*10
    for k in range(count):
        # offset each glider within group vertically to avoid overlap
        dx = group_x
        dy = group_y + k*4
        for (gx,gy) in glider:
            coords.append((gx+dx, gy+dy))

patterns.mkdir(parents=True, exist_ok=True)
with out.open('w') as f:
    f.write('# Fibonacci glider display: groups with sizes 1,1,2,3,5,8\n')
    for x,y in coords:
        f.write(f"{x} {y}\n")

print('Wrote', out)
