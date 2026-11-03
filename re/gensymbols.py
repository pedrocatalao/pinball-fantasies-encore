#!/usr/bin/env python3
"""Writes the two files the engine knows the four table programs by.

src/engine/table/Names.inc: the names in re/symbols/table1.txt, as Party Land's addresses.
The engine's code is written against Party Land's addresses throughout.

src/engine/table/Maps.inc: where each of Party Land's addresses is in the other three
programs, from lining them up (re/align.py, run first for 1 2, 1 3 and 1 4): every variable
its engine code touches, every address-like number that code carries, and every run of
instructions that is the same in both. A line of the names file may place a name by hand,
with `@2:1234` after it, where the line-up could not.

  python3 re/gensymbols.py"""
import pathlib, re

here = pathlib.Path(__file__).parent
dest = here.parent / 'src' / 'engine' / 'table'
dest.mkdir(parents=True, exist_ok=True)

tables = {}
for t in (2, 3, 4):
    m = {'code': [], 'data': {}, 'idx': {}, 'imm': {}, 'csdata': {}, 'target': {}}
    for line in (here / 'fantasy' / f'map_1_{t}.txt').read_text().splitlines():
        f = line.split()
        if not f or f[0].startswith('#'): continue
        if f[0] == 'small': continue
        if f[0] == 'code': m['code'].append((int(f[1], 16), int(f[2], 16), int(f[3], 16)))
        elif int(f[1], 16) <= 0xffff and int(f[2], 16) <= 0xffff: m[f[0]][int(f[1], 16)] = int(f[2], 16)
    tables[t] = m

names = ['// Written by re/gensymbols.py from re/symbols/table1.txt. Do not edit.',
         '// Party Land\'s addresses: c code, d data, v variables kept among the code, i values.', '']
kinds = {'c': 'code', 'd': 'data', 'v': 'csdata', 'i': 'imm'}
pending = []
for line in (here / 'symbols' / 'table1.txt').read_text().splitlines():
    m = re.match(r'^([cdiv]) ([0-9a-f]+) (\w+)((?:\s+@\d:[0-9a-f]+)*)\s*(?:;\s*(.*))?$', line.split('#')[0].strip())
    if not m: continue
    kind, addr, name, hand, note = m.group(1), int(m.group(2), 16), m.group(3), m.group(4), m.group(5)
    names.append(f'constexpr u16 {name} = 0x{addr:04x};' + (f'  ///< {note}' if note else ''))
    pending.append((kind, addr, name, note or ''))
    for t, a in re.findall(r'@(\d):([0-9a-f]+)', hand):
        if kind == 'c': tables[int(t)]['code'].append((addr, addr, int(a, 16)))
        else: tables[int(t)][kinds[kind]][addr] = int(a, 16)
(dest / 'Names.inc').write_text('\n'.join(names) + '\n')
carry = []   # (kind, Party Land's address, name, note), to write the other tables' names from

# Routines the engine's source names that the line-up did not place (ones a table only has
# in its scripts, say) are looked for by their first instructions, numbers left out: where
# exactly one place in the other program begins the same way, that is it.
num = re.compile(r'0x[0-9a-f]+')
def listing(t):
    out = []
    for line in (here / 'fantasy' / f'TABLE{t}_seg0010.asm').read_text().splitlines():
        try: a = int(line[:8], 16)
        except ValueError: continue
        ins = line[28:].strip()
        if ins and line[10:28].strip(): out.append((a, num.sub('#', ins)))
    return out
first = listing(1)
index = {a: i for i, (a, _) in enumerate(first)}
# (by number, or by a name of this file: at::name stands for its address)
address = {name: addr for _, addr, name, _ in pending}
wanted = set()
for src in sorted(dest.glob('Engine*.cpp')) + sorted(dest.glob('Flow*.cpp')):
    text = src.read_text()
    wanted |= {int(x, 16) for x in re.findall(r'0x([0-9a-f]{4})\b', text)}
    wanted |= {address[x] for x in re.findall(r'\bat::(\w+)', text) if x in address}
wanted = sorted(a for a in wanted if a in index and 0x0200 <= a < 0x9240)
found = 0
for t in (2, 3, 4):
    other = listing(t)
    text = [i for _, i in other]
    placed = lambda a: any(x <= a <= y for x, y, _ in tables[t]['code']) or a in tables[t]['target']
    for a in wanted:
        if placed(a): continue
        i = index[a]
        for n in (12, 10, 8, 6, 5, 4):
            want = [x for _, x in first[i:i + n]]
            hits = [j for j in range(len(text) - n) if text[j] == want[0] and text[j:j + n] == want]
            if len(hits) == 1:
                tables[t]['target'][a] = other[hits[0]][0]
                found += 1
                break
            if len(hits) > 1: break   # shorter would only be less sure

out = ['// Written by re/gensymbols.py from the line-up of the four table programs. Do not edit.', '']
for kind, macro in (('data', 'ENCORE_MAP_DATA'), ('idx', 'ENCORE_MAP_INDEXED'), ('csdata', 'ENCORE_MAP_CODE_DATA'), ('imm', 'ENCORE_MAP_VALUE'), ('target', 'ENCORE_MAP_TARGET')):
    keys = sorted(set().union(*(tables[t][kind].keys() for t in (2, 3, 4))))
    for k in keys:
        # 0xffff: not found in that table
        row = [tables[t][kind].get(k, 0xffff) for t in (2, 3, 4)]
        out.append(f'{macro}(0x{k:04x}, ' + ', '.join(f'0x{a:04x}' for a in row) + ')')
for t in (2, 3, 4):
    for a, b, to in sorted(tables[t]['code']):
        out.append(f'ENCORE_MAP_CODE({t - 1}, 0x{a:04x}, 0x{b:04x}, 0x{to:04x})')
(dest / 'Maps.inc').write_text('\n'.join(out) + '\n')
# The same names for the other tables' listings (re/disasm.py reads these).
for t in (2, 3, 4):
    lines = [f'# TABLE{t}: Party Land\'s names, carried over by re/gensymbols.py. Do not edit.']
    for kind, addr, name, note in pending:
        if kind == 'c':
            to = next((to + addr - a for a, b, to in tables[t]['code'] if a <= addr <= b), None)
            if to is None: to = tables[t]['target'].get(addr, tables[t]['imm'].get(addr))
        else:
            to = tables[t][kinds[kind]].get(addr)
            if to is None and kind == 'd': to = tables[t]['idx'].get(addr, tables[t]['imm'].get(addr))
        if to is not None and kind != 'i': lines.append(f'{kind} {to:04x} {name}' + (f' ; {note}' if note else ''))
    (here / 'fantasy' / f'table{t}.txt').write_text('\n'.join(lines) + '\n')
print(f'{len(names) - 3} names; {len(out) - 2} map lines; {found} routines placed by their first instructions')
