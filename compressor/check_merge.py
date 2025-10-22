#!/usr/bin/env python3
import sys

x_merge = 0
y_merge = 0
z_merge = 0

lines = [l.strip() for l in sys.stdin if l.strip()]
blocks = []

for line in lines:
    parts = line.split(',')
    if len(parts) >= 7:
        try:
            x, y, z, dx, dy, dz = map(int, parts[:6])
            label = parts[6]
            blocks.append((x, y, z, dx, dy, dz, label))
        except:
            pass

print(f'Checking {len(blocks)} blocks...', file=sys.stderr)

for i in range(len(blocks)):
    x1, y1, z1, dx1, dy1, dz1, l1 = blocks[i]
    for j in range(i+1, min(i+100, len(blocks))):
        x2, y2, z2, dx2, dy2, dz2, l2 = blocks[j]

        if l1 != l2:
            continue

        if y1==y2 and z1==z2 and dy1==dy2 and dz1==dz2 and x1+dx1==x2:
            x_merge += 1

        if x1==x2 and z1==z2 and dx1==dx2 and dz1==dz2 and y1+dy1==y2:
            y_merge += 1

        if x1==x2 and y1==y2 and dx1==dx2 and dy1==dy2 and z1+dz1==z2:
            z_merge += 1

print(f'X-mergeable: {x_merge}')
print(f'Y-mergeable: {y_merge}')
print(f'Z-mergeable: {z_merge}')
print(f'Total mergeable: {x_merge + y_merge + z_merge}')
