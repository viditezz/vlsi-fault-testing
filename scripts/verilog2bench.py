#!/usr/bin/env python3
"""Convert gate-level ISCAS-85 Verilog (primitive instances only) to .bench.

Usage: verilog2bench.py in.v out.bench

Handles: module/input/output/wire declarations (multi-line), and primitive
instances  `nand NAME (out, in1, in2, ...);`  for and/or/nand/nor/not/buf/
xor/xnor.  Signal names of the form N<digits> are written as <digits>, which
reproduces the canonical ISCAS-85 .bench naming (c17: 1, 2, 3, 6, 7, ...).
"""
import re
import sys

GATES = {"and": "AND", "or": "OR", "nand": "NAND", "nor": "NOR",
         "not": "NOT", "buf": "BUF", "xor": "XOR", "xnor": "XNOR"}


def clean(name):
    name = name.strip()
    m = re.fullmatch(r"N(\d+)", name)
    return m.group(1) if m else name


def convert(text):
    text = re.sub(r"//.*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    inputs, outputs, gates = [], [], []
    for stmt in text.split(";"):
        stmt = " ".join(stmt.split())
        if not stmt:
            continue
        kw = stmt.split()[0]
        if kw in ("input", "output"):
            names = [clean(n) for n in stmt[len(kw):].split(",") if n.strip()]
            (inputs if kw == "input" else outputs).extend(names)
        elif kw in GATES:
            m = re.fullmatch(r"\w+\s+[\w\\$]*\s*\((.*)\)", stmt)
            if not m:
                raise ValueError("cannot parse: " + stmt)
            pins = [clean(p) for p in m.group(1).split(",")]
            gates.append((pins[0], GATES[kw], pins[1:]))
        elif kw in ("module", "wire", "endmodule"):
            continue
        else:
            raise ValueError("unsupported statement: " + stmt)
    return inputs, outputs, gates


def main():
    src, dst = sys.argv[1], sys.argv[2]
    with open(src) as f:
        inputs, outputs, gates = convert(f.read())
    name = src.rsplit("/", 1)[-1].rsplit(".", 1)[0]
    with open(dst, "w") as f:
        f.write(f"# {name}\n# converted from ISCAS-85 Verilog\n")
        f.write(f"# {len(inputs)} inputs, {len(outputs)} outputs, {len(gates)} gates\n\n")
        for n in inputs:
            f.write(f"INPUT({n})\n")
        f.write("\n")
        for n in outputs:
            f.write(f"OUTPUT({n})\n")
        f.write("\n")
        for out, typ, ins in gates:
            f.write(f"{out} = {typ}({', '.join(ins)})\n")
    print(f"{name}: {len(inputs)} PI, {len(outputs)} PO, {len(gates)} gates")


if __name__ == "__main__":
    main()
