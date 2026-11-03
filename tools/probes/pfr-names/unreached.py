#!/usr/bin/env python3
"""Question: of the engine's routines with a name in re/symbols/table1.txt, which does no referee
run reach? Reads the "never called" lines (ENCORE_COVER=1) of the equiv.sh folders <prefix>-1 to
<prefix>-4, one a table, and prints per table the named routines no run called, then those that
no table's runs called. (The "never called" lists are each table's own addresses and the names
are Party Land's, so only table 1's line means anything until the other three are mapped.)
  usage: python3 tools/probes/pfr-names/unreached.py <repo> <prefix>    (e.g. build/equiv-after)"""
import re, sys, glob
repo, prefix = sys.argv[1], sys.argv[2]
names = {}
for l in open(repo + '/re/symbols/table1.txt'):
    m = re.match(r'^c ([0-9a-f]+) (\w+)', l)
    if m:
        names[int(m[1], 16)] = m[2]
every = None
for t in (1, 2, 3, 4):
    never = None
    for f in glob.glob(f'{prefix}-{t}/oracle-[fw]*.txt'):
        s = set()
        for l in open(f):
            if l.startswith('never called:'):
                s |= {int(x, 16) for x in l.split(':')[1].split()}
        if s: never = s if never is None else never & s
    if never is None:
        continue
    named = {a for a in never if a in names}
    print(f'table {t}: {len(named)} named routines never called:', ' '.join(names[a] for a in sorted(named)))
    every = named if every is None else every & named
print('on no table:', ' '.join(names[a] for a in sorted(every)))
