#!/usr/bin/env python3
"""Question: how many numbers are still written as numbers in the game's code (src/engine and the
referee, tools/oracle), at a revision? Counts the numeric literals of each .cpp and .h outside
constexpr definitions, preprocessor lines, comments and strings, leaving out 0, 1 and 2, and prints
the total and the files with the most.
  usage: python3 tools/probes/pfr-names/count_remaining.py <rev> [<rev>...]   (from the repo's root)"""
import collections, re, subprocess, sys
files = [f for f in subprocess.run(['git', 'ls-files', 'src/engine', 'tools/oracle'], capture_output=True, text=True).stdout.split()
         if f.endswith(('.cpp', '.h'))]
num = re.compile(r'(?<![\w.])(0x[0-9a-fA-F]+|\d+)(?:u|U|f|F|ull|ul|L)?\b')
for rev in sys.argv[1:]:
    total, per = 0, collections.Counter()
    for f in files:
        text = subprocess.run(['git', 'show', f'{rev}:{f}'], capture_output=True, text=True).stdout
        for line in text.splitlines():
            code = line.split('//')[0]
            if re.search(r'\bconstexpr\b', code) or code.lstrip().startswith('#'):
                continue
            code = re.sub(r'"(\\.|[^"\\])*"', '""', code)
            for m in num.finditer(code):
                v = m.group(1)
                if (int(v, 16) if v.startswith('0x') else int(v)) in (0, 1, 2):
                    continue
                total += 1
                per[f] += 1
    print(rev, total)
    for f, n in per.most_common(8):
        print('  ', n, f)
