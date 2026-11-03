# Question: for the function bodies that differ between two objcmp.sh outputs (see fncmp.py),
# what exactly changed in each? Pairs each body found only before with its nearest body found
# only after, and prints how many lines differ, and how many of those are not padding (nop).
#   usage: python3 fnpair.py before.txt after.txt
import sys, difflib, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fncmp.py')).read().split('a, b = fns')[0])
a, b = fns(sys.argv[1]), fns(sys.argv[2])
oa, ob = list((a - b).elements()), list((b - a).elements())
used = set()
for f in oa:
    free = [j for j in range(len(ob)) if j not in used]
    if not free: print("before only, unpaired:\n" + f); continue
    j = max(free, key=lambda j: difflib.SequenceMatcher(None, f.split('\n'), ob[j].split('\n')).ratio())
    used.add(j)
    d = [l for l in difflib.unified_diff(f.split('\n'), ob[j].split('\n'), lineterm='', n=0)
         if l[:1] in '+-' and not l.startswith(('+++', '---'))]
    real = [l for l in d if 'nop' not in l]
    print(f"######## before {len(f.splitlines())} -> after {len(ob[j].splitlines())} lines: {len(d)} differ, {len(real)} not padding")
    print('\n'.join(real[:40]))
for j in range(len(ob)):
    if j not in used: print("######## after only, unpaired:\n" + ob[j])
