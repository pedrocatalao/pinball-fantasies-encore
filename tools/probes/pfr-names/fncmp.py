# Question: which function bodies differ between two objcmp.sh outputs, ignoring their order?
#   usage: python3 fncmp.py before.txt after.txt
import sys, collections
def fns(p):
    body, out = [], []
    for l in open(p).read().split('\n')[5:]:
        if l.startswith(':') or l == '<fn>:':
            if body: out.append('\n'.join(body).strip())
            body = []
        else: body.append(l)
    if body: out.append('\n'.join(body).strip())
    return collections.Counter(out)
a, b = fns(sys.argv[1]), fns(sys.argv[2])
only_a, only_b = a - b, b - a
print(f"functions: before {sum(a.values())}, after {sum(b.values())}, same {sum((a & b).values())}")
for name, d in (("only before", only_a), ("only after", only_b)):
    print(f"== {name}: {sum(d.values())}")
    for f in d: print("----\n" + f)
