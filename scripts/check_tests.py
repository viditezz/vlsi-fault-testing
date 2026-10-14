#!/usr/bin/env python3
"""Independent re-simulation of the adaptive test sets in results/tests/.

Shares no simulation code with faultatpg: its own .bench parser, levelization and
gate evaluation (Python big ints, one bit per test vector), serial fault injection.
Only the collapsed fault list is taken from `faultatpg faults`.

What it checks: that the committed test set really detects the number of faults the
tool claims. What it does not check: completeness. Whether the faults left undetected
are truly redundant is established by the tool's SAT fallback, which `make test`
validates against exhaustive simulation. The script also prints how many faults the
tool says are proved redundant, so detected + redundant can be compared with the
collapsed fault count.

Sequential (ISCAS-89) circuits are read in full-scan form, as the tool does:
q = DFF(d) makes q a pseudo-primary input (in file order with the INPUT lines)
and d a pseudo-primary output.

Usage: scripts/check_tests.py ./faultatpg results c432 c880 ...
"""
import re, subprocess, sys, csv
def parse(path):
    pis, pos, gates, scan_d = [], [], {}, []
    for line in open(path):
        line = line.split('#')[0].strip()
        if not line: continue
        m = re.match(r'INPUT\((.+)\)', line)
        if m: pis.append(m.group(1).strip()); continue
        m = re.match(r'OUTPUT\((.+)\)', line)
        if m: pos.append(m.group(1).strip()); continue
        out, rhs = [x.strip() for x in line.split('=')]
        t, args = re.match(r'(\w+)\((.*)\)', rhs).groups()
        if t.upper() == 'DFF':                      # full scan
            pis.append(out); scan_d.append(args.strip()); continue
        gates[out] = (t.upper(), [a.strip() for a in args.split(',')])
    for d in scan_d:
        if d not in pos: pos.append(d)
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
def cone_sim(gates, order_of, fanout, good, mask, fault):
    """Faulty simulation restricted to the fault's fanout cone; everything else is good."""
    site = fault[1]
    seen, todo = set(), list(fanout.get(site, ())) if fault[0] == 'stem' else [site]
    while todo:
        g = todo.pop()
        if g in seen: continue
        seen.add(g); todo.extend(fanout.get(g, ()))
    val = {}
    if fault[0] == 'stem': val[site] = fault[2]
    get = lambda x: val.get(x, good[x])
    for g in sorted(seen, key=order_of.__getitem__):
        t, args = gates[g]
        ins = [get(a) for a in args]
        if fault[0] == 'branch' and g == site: ins[fault[2]] = fault[3]
        v = ev(t, ins, mask)
        if fault[0] == 'stem' and g == site: v = fault[2]
        val[g] = v
    return get
def check(bench, tests, faultlist):
    pis, pos, gates, order = parse(bench)
    order_of = {g: i for i, g in enumerate(order)}
    fanout = {}
    for g, (t, args) in gates.items():
        for a in args: fanout.setdefault(a, []).append(g)
    vecs = [l.strip() for l in open(tests) if l.strip() and not l.startswith('#')]
    n = len(vecs); mask = (1 << n) - 1
    pivals = {p: sum(int(v[i]) << j for j, v in enumerate(vecs)) for i, p in enumerate(pis)}
    good = sim(pis, gates, order, pivals, mask)
    det = 0
    for line in faultlist:
        site, sa = line.split('\t')[1].split()
        sv = mask if sa.endswith('1') else 0
        if site.endswith('->PO'):                   # fault on the branch into the output only
            s = site[:-4]
            if any((sv if o == s else good[o]) != good[o] for o in pos): det += 1
            continue
        if '->' in site:
            s, gp = site.split('->'); g, pin = gp.rsplit('.', 1)
            f = ('branch', g, int(pin), sv)
        else:
            f = ('stem', site, sv)
        fv = cone_sim(gates, order_of, fanout, good, mask, f)
        if any(fv(o) != good[o] for o in pos): det += 1
    return det, len(faultlist), n
if __name__ == '__main__':
    tool, rdir = sys.argv[1], sys.argv[2]
    claimed = {r['circuit']: int(r['detected']) for r in csv.DictReader(open(rdir + '/summary.csv'))
               if r['method'] == 'adaptive' and r['seed'] == '1'}
    redundant = {r['circuit']: int(r['redundant']) for r in csv.DictReader(open(rdir + '/summary.csv'))
                 if r['method'] == 'adaptive' and r['seed'] == '1'}
    for c in sys.argv[3:]:
        fl = subprocess.run([tool, 'faults', f'benchmarks/{c}.bench'], capture_output=True, text=True).stdout.splitlines()
        d, nf, nt = check(f'benchmarks/{c}.bench', f'{rdir}/tests/{c}_adaptive.txt', fl)
        red = redundant[c]
        print(f'{c}: {nt} tests, independent {d}/{nf}, tool claims {claimed[c]}  '
              f'{"OK" if d == claimed[c] else "MISMATCH"};  tool proves {red} redundant -> '
              f'{"all faults accounted for" if d + red == nf else "NOT ALL ACCOUNTED FOR"}', flush=True)
