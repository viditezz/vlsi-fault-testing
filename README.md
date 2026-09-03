# Adaptive Fault-Coverage-Driven Test Generation for Combinational Circuits

**Bridging Random Testing and Deterministic ATPG**

Course project — Testing of VLSI Circuits (BEVD309L), VIT Vellore

## Overview

Deterministic ATPG (PODEM, D-algorithm) achieves high fault coverage but is
computationally expensive on larger circuits. Random test generation is fast
per-pattern but its fault-coverage growth plateaus early, leaving a residue
of hard-to-detect faults.

This project applies a UVM-style constrained-random, coverage-driven feedback
loop to stuck-at fault testing: generate random vectors, fault-simulate them,
track fault-coverage growth, and statistically detect when that growth has
stagnated. On stagnation, switch from random generation to a deterministic
pass (PODEM) scoped only to the remaining undetected faults — combining
random testing's low generation cost with deterministic ATPG's targeting
capability, triggered by the circuit's actual coverage trajectory rather
than a fixed pattern count or predetermined threshold.

Evaluated against ISCAS-85 benchmark circuits, comparing fault coverage %,
pattern count, and generation time across pure random, the adaptive method,
and standalone PODEM.

## Repository structure

```
vlsi-fault-testing/
├── src/
│   └── main.cpp        # .bench parser, levelizer, fault-free logic simulator
├── benchmarks/
│   └── c17.bench        # ISCAS-85 benchmark circuit(s)
├── paper/                # Literature review papers
├── links.txt             # Reference links
└── README.md
```

## Status

- [x] `.bench` netlist parser
- [x] Levelization (Kahn's algorithm)
- [x] Fault-free logic simulation — validated by hand on c17
- [ ] Single stuck-at fault injection + detection
- [ ] Random vector generation + coverage tracking
- [ ] Stagnation/plateau detection
- [ ] PODEM (deterministic ATPG, shared backend for baseline + adaptive switch)
- [ ] Comparison sweep (random vs. adaptive vs. PODEM)

## Build & run

```bash
g++ -std=c++17 -Wall -o src/main src/main.cpp
./src/main benchmarks/c17.bench
```

## Benchmarks

Circuits are in ISCAS-85 `.bench` format. Currently included: c17.
Planned: c432, c880, c1355, c1908, c2670, c3540, c5315, c6288 (small to
large, to demonstrate scaling behavior).
