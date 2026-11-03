#!/usr/bin/env python3
"""Question: which of pfr's names for the game's things does our code not carry? Takes the variants
of pfr's binding enums (LightBind, EffectBind, SfxBind, JingleBind, PhysmapBind, RollTrigger,
HitTrigger, UopKind, TaskKind, ScriptBind, ScriptScore) and looks for each in src/engine/table, by
its name without the table's prefix (StonesTowerJackpot as towerJackpot; Gate and Hit dropped
too, as gates:: and the targets' lists say them), as the base of a numbered series
(SpeedMb2 as mb), or cited by its full name in a comment. Prints the count and those not found.
  usage: python3 tools/probes/pfr-names/pfr_coverage.py <repo> <pfr checkout>"""
import glob, re, sys
repo, pfr = sys.argv[1], sys.argv[2]
kinds = ('LightBind', 'EffectBind', 'SfxBind', 'JingleBind', 'PhysmapBind', 'RollTrigger', 'HitTrigger', 'UopKind',
         'TaskKind', 'ScriptBind', 'ScriptScore')
enums = {}
for f in glob.glob(f'{pfr}/src/**/*.rs', recursive=True):
    for m in re.finditer(r'pub enum (\w+)\s*\{(.*?)\n\}', open(f).read(), re.S):
        if m.group(1) in kinds:
            enums.setdefault(m.group(1), set()).update(re.findall(r'^\s*(\w+)\s*[,(]', m.group(2), re.M))
ours = ''.join(open(f).read() for f in glob.glob(f'{repo}/src/engine/table/*.[ch]*') + [f'{repo}/src/engine/table/Names.inc'])
low = ours.lower()
def strip(v):
    for p in ('Party', 'Speed', 'Show', 'Stones', 'Gate', 'Hit'):
        if v.startswith(p) and len(v) > len(p) and v[len(p)].isupper():
            v = v[len(p):]
    return v
total, missing = 0, []
for e, vs in sorted(enums.items()):
    for v in sorted(vs):
        if v == 'Dummy':
            continue
        total += 1
        s = strip(v)
        if not (s.lower() in low or re.sub(r'\d+$', '', s).lower() in low or v in ours):
            missing.append(f'{e}::{v}')
print(f'{total - len(missing)} of {total} found; not found: {" ".join(missing) or "none"}')
