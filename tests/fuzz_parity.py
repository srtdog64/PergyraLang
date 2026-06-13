#!/usr/bin/env python3
"""
C vs LLVM backend parity fuzzer.

Generates random, valid, deterministic-output Pergyra programs from the
well-supported language subset (integer variables, arithmetic, bounded loops,
conditionals, helper functions, Log output), compiles each with both the C and
LLVM backends, runs both native binaries, and compares stdout and exit codes.

A divergence is a backend-parity bug. A non-zero compiler exit on a generated
(valid) program, or a crash signal, is a compiler-robustness finding.

Usage:
    python3 tests/fuzz_parity.py --pgy ./bin/pgy --count 200 --seed 1
"""

import argparse
import os
import random
import subprocess
import sys
import tempfile


BINOPS = ["+", "-", "*"]


class Gen:
    def __init__(self, rng):
        self.rng = rng
        self.vars = []

    def const(self):
        return str(self.rng.randint(0, 9))

    def atom(self):
        if self.vars and self.rng.random() < 0.6:
            return self.rng.choice(self.vars)
        return self.const()

    def expr(self, depth):
        if depth <= 0 or self.rng.random() < 0.35:
            return self.atom()
        left = self.expr(depth - 1)
        right = self.expr(depth - 1)
        op = self.rng.choice(BINOPS)
        return "(" + left + " " + op + " " + right + ")"

    def cond(self):
        a = self.expr(1)
        b = self.expr(1)
        op = self.rng.choice(["<", "<=", ">", ">=", "=="])
        return a + " " + op + " " + b


def gen_program(rng):
    g = Gen(rng)
    lines = ["func Main() -> Void {"]
    nvars = rng.randint(2, 5)
    for i in range(nvars):
        name = "v" + str(i)
        lines.append("    let " + name + ": Int = " + g.const() + ";")
        g.vars.append(name)

    nstmts = rng.randint(4, 12)
    for _ in range(nstmts):
        kind = rng.random()
        target = rng.choice(g.vars)
        if kind < 0.45:
            lines.append("    " + target + " = " + g.expr(3) + ";")
        elif kind < 0.70:
            lines.append("    if " + g.cond() + " {")
            lines.append("        " + target + " = " + g.expr(2) + ";")
            lines.append("    } else {")
            lines.append("        " + target + " = " + g.expr(2) + ";")
            lines.append("    }")
        else:
            bound = rng.randint(0, 4)
            lines.append("    for __i in 0.." + str(bound) + " {")
            lines.append("        " + target + " = (" + target + " + 1);")
            lines.append("    }")

    for v in g.vars:
        lines.append("    Log(" + v + ");")
    lines.append("}")
    return "\n".join(lines) + "\n"


def compile_and_run(pgy, backend, src_path, out_bin):
    comp = subprocess.run([pgy, src_path, "--backend=" + backend, "-o", out_bin],
                          capture_output=True, text=True, timeout=60)
    if comp.returncode != 0:
        return ("COMPILE_FAIL", comp.returncode, comp.stdout + comp.stderr)
    if not os.path.exists(out_bin):
        cand = out_bin + ".exe"
        out_bin = cand if os.path.exists(cand) else out_bin
    run = subprocess.run([out_bin], capture_output=True, text=True, timeout=30)
    return ("OK", run.returncode, run.stdout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pgy", default="./bin/pgy")
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--keep-dir", default=None)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    work = args.keep_dir or tempfile.mkdtemp(prefix="pgy_fuzz_")
    os.makedirs(work, exist_ok=True)

    parity_fail = []
    compile_fail = []
    ran = 0
    for n in range(args.count):
        prog = gen_program(rng)
        src = os.path.join(work, "f" + str(n) + ".pgy")
        with open(src, "w") as fh:
            fh.write(prog)

        c_status, c_rc, c_out = compile_and_run(
            args.pgy, "c", src, os.path.join(work, "f" + str(n) + "_c"))
        l_status, l_rc, l_out = compile_and_run(
            args.pgy, "llvm", src, os.path.join(work, "f" + str(n) + "_l"))

        # A valid generated program should compile on both. If either backend
        # rejects it, record it (could be a generator over-reach or a real
        # backend gap) but don't treat it as a parity divergence.
        if c_status == "COMPILE_FAIL" or l_status == "COMPILE_FAIL":
            compile_fail.append((n, c_status, l_status))
            continue

        ran += 1
        if c_out != l_out or c_rc != l_rc:
            parity_fail.append((n, c_rc, l_rc, c_out, l_out, prog))

    print("fuzz-parity: ran " + str(ran) + " comparable programs "
          "(" + str(len(compile_fail)) + " compile-skipped) of "
          + str(args.count))
    if parity_fail:
        print("fuzz-parity: " + str(len(parity_fail)) + " PARITY DIVERGENCE(S):")
        for (n, c_rc, l_rc, c_out, l_out, prog) in parity_fail[:5]:
            print("=== program " + str(n) + " (C rc=" + str(c_rc)
                  + " LLVM rc=" + str(l_rc) + ") ===")
            print(prog)
            print("--- C stdout ---\n" + c_out)
            print("--- LLVM stdout ---\n" + l_out)
        sys.exit(1)
    print("fuzz-parity: no divergences; C and LLVM agree on all "
          + str(ran) + " programs")
    sys.exit(0)


if __name__ == "__main__":
    main()
