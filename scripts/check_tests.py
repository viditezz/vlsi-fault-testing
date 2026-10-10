#!/usr/bin/env python3
"""Independent re-simulation of the adaptive test sets in results/tests/.

Shares no simulation code with faultatpg: its own .bench parser, levelization and
gate evaluation (Python big ints, one bit per test vector), serial fault injection.
Only the collapsed fault list is taken from `faultatpg faults`.

Usage: scripts/check_tests.py ./faultatpg results c432 c880 ...
"""
import re, subprocess, sys, csv
def parse(path):
    pis, pos, gates = [], [], {}
    for line in open(path):
        line = line.split('#')[0].strip()
        if not line: continue
        m = re.match(r'INPUT\((.+)\)', line)
        if m: pis.append(m.group(1).strip()); continue
        m = re.match(r'OUTPUT\((.+)\)', line)
        if m: pos.append(m.group(1).strip()); continue
        out, rhs = [x.strip() for x in line.split('=')]
        t, args = re.match(r'(\w+)\((.*)\)', rhs).groups()
        gates[out] = (t.upper(), [a.strip() for a in args.split(',')])
    order, done = [], set(pis)
    pending = dict(gates)
    while pending:
        ready = [g for g, (t, a) in pending.items() if all(x in done for x in a)]
        assert ready, 'cycle'
        for g in ready: order.append(g); done.add(g); del pending[g]
    return pis, pos, gates, order
def ev(t, vals, mask):
    if t in ('BUF', 'BUFF'): return vals[0]
    if t == 'NOT': return ~vals[0] & mask
    r = vals[0]
    for v in vals[1:]:
        r = r & v if t in ('AND', 'NAND') else r | v if t in ('OR', 'NOR') else r ^ v
    return (~r & mask) if t in ('NAND', 'NOR', 'XNOR') else r
def sim(pis, gates, order, pivals, mask, fault=None):
    val = dict(pivals)
    if fault and fault[0] == 'stem' and fault[1] in val: val[fault[1]] = fault[2]
    for g in order:
        t, args = gates[g]
        ins = [val[a] for a in args]
        if fault and fault[0] == 'branch' and fault[1] == g:
            ins[fault[2]] = fault[3]
        val[g] = ev(t, ins, mask)
        if fault and fault[0] == 'stem' and fault[1] == g: val[g] = fault[2]
    return val
def check(bench, tests, faultlist):
    pis, pos, gates, order = parse(bench)
    vecs = [l.strip() for l in open(tests) if l.strip() and not l.startswith('#')]
    n = len(vecs); mask = (1 << n) - 1
    pivals = {p: sum(int(v[i]) << j for j, v in enumerate(vecs)) for i, p in enumerate(pis)}
    good = sim(pis, gates, order, pivals, mask)
    det = 0
    for line in faultlist:
        site, sa = line.split('\t')[1].split()
        sv = mask if sa.endswith('1') else 0
        if '->' in site:
            s, gp = site.split('->'); g, pin = gp.rsplit('.', 1)
            f = ('branch', g, int(pin), sv)
        else:
            f = ('stem', site, sv)
        fv = sim(pis, gates, order, pivals, mask, f)
        if any(fv[o] != good[o] for o in pos): det += 1
    return det, len(faultlist), n
if __name__ == '__main__':
    tool, rdir = sys.argv[1], sys.argv[2]
    claimed = {r['circuit']: int(r['detected']) for r in csv.DictReader(open(rdir + '/summary.csv'))
               if r['method'] == 'adaptive' and r['seed'] == '1'}
    for c in sys.argv[3:]:
        fl = subprocess.run([tool, 'faults', f'benchmarks/{c}.bench'], capture_output=True, text=True).stdout.splitlines()
        d, nf, nt = check(f'benchmarks/{c}.bench', f'{rdir}/tests/{c}_adaptive.txt', fl)
        print(f'{c}: {nt} tests, independent {d}/{nf}, tool claims {claimed[c]}  {"OK" if d == claimed[c] else "MISMATCH"}', flush=True)
