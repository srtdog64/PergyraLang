#!/usr/bin/env python3
"""Exercise native overload spelling through semantic admission and C/LLVM.

Expected spellings/results are independent of the production table. Both
backends must agree with these results, not merely with each other.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


RULES = [
    ("+", "add", "Add add OperatorAdd operator_add"),
    ("-", "sub", "Sub sub Subtract subtract OperatorSub operator_sub"),
    ("*", "mul", "Mul mul Multiply multiply OperatorMul operator_mul"),
    ("/", "div", "Div div Divide divide OperatorDiv operator_div"),
    ("%", "mod", "Mod mod Modulo modulo OperatorMod operator_mod"),
    ("==", "eq", "Eq eq Equal equal Equals equals OperatorEq operator_eq"),
    ("!=", "ne", "Ne ne NotEqual notEqual NotEquals notEquals OperatorNe operator_ne"),
    ("<", "lt", "Lt lt LessThan lessThan OperatorLt operator_lt"),
    ("<=", "le", "Le le LessEqual lessEqual LessThanOrEqual lessThanOrEqual OperatorLe operator_le"),
    (">", "gt", "Gt gt GreaterThan greaterThan OperatorGt operator_gt"),
    (">=", "ge", "Ge ge GreaterEqual greaterEqual GreaterThanOrEqual greaterThanOrEqual OperatorGe operator_ge"),
]


def role(type_name, method, value, params="rhs: Int"):
    return f"""
subject {type_name} {{}}
ability A{type_name} {{ func {method}(self, {params}) -> Int; }}
role R{type_name} for {type_name} {{
    impl A{type_name} {{
        func {method}(self, {params}) -> Int {{ return {value}; }}
    }}
}}
"""


def fixtures():
    for variant in range(8):
        declarations, calls, want = [], [], []
        for index, (op, _, aliases) in enumerate(RULES):
            names = aliases.split()
            if variant >= len(names):
                continue
            name = f"Box{index}"
            value = 1000 + index * 10 + variant
            declarations.append(role(name, names[variant], value))
            calls.append(f"let result{index}: Int = {name}() {op} 2; Log(result{index});")
            want.append(str(value))
        yield f"role_alias_{variant}", "".join(declarations) + (
            "func Main() -> Void {" + "\n".join(calls) + "}"
        ), "\n".join(want) + "\n"

    declarations, calls, want = [], [], []
    for index, (op, suffix, _) in enumerate(RULES):
        name = f"Value{index}"
        value = 2000 + index
        declarations.append(f"struct {name} {{ value: Int; }}\n"
                            f"func operator_{suffix}_{name}(a: {name}, b: Int) -> Int"
                            f" {{ return {value}; }}")
        calls.append(f"let result{index}: Int = {name}(1) {op} 2; Log(result{index});")
        want.append(str(value))
    yield "free_suffixes", "\n".join(declarations) + (
        "func Main() -> Void {" + "\n".join(calls) + "}"
    ), "\n".join(want) + "\n"

    priority_role = """
ability Arithmetic { func Add(self, rhs: Int) -> Int; }
role IntMath for Int {
    impl Arithmetic { func Add(self, rhs: Int) -> Int { return 321; } }
}
"""
    priority_function = "func operator_add_Int(a: Int, b: Int) -> Int { return 789; }\n"
    priority_main = """
func Main() -> Void { let result: Int = 1 + 2; Log(result); }
"""
    yield "free_precedes_role", priority_role + priority_function + priority_main, "789\n"
    yield "free_declared_first", priority_function + priority_role + priority_main, "789\n"
    yield "builtins", """
func Main() -> Void {
    Log(10 + 3); Log(10 - 3); Log(10 * 3); Log(10 / 3); Log(10 % 3);
    Log(10 == 3); Log(10 != 3); Log(10 < 3); Log(10 <= 3);
    Log(10 > 3); Log(10 >= 3); Log(true && false); Log(true || false);
}
""", "13\n7\n30\n3\n1\nfalse\ntrue\nfalse\nfalse\ntrue\ntrue\nfalse\ntrue\n"

    for method in ("ADD", "AddExtra", "_Add", "Subtract", "operator_Add"):
        yield "reject_" + method, role("Box", method, 123) + """
func Main() -> Void { Log(Box() + 2); }
""", None
    for label, params, op in (
        ("wrong_rhs", "rhs: Bool", "+"),
        ("wrong_arity", "rhs: Int, extra: Int", "+"),
        ("unsupported_op", "rhs: Int", "&&"),
    ):
        yield "reject_" + label, role("Box", "Add", 123, params) + (
            f"func Main() -> Void {{ Log(Box() {op} 2); }}"
        ), None
    yield "reject_free_wrong_suffix", """
struct Box { value: Int; }
func operator_Add_Box(a: Box, b: Int) -> Int { return 123; }
func Main() -> Void { Log(Box(1) + 2); }
""", None


def check_result(label, backend, result, exe, want):
    if want is None:
        if result.returncode == 0 or exe.exists():
            raise AssertionError(f"{label}/{backend}: invalid overload admitted")
        diagnostics = json.loads(result.stderr)
        if not any(row.get("stage") == "semantic"
                   and row.get("code") == "PGY_SEM_BINOP_TYPE_MISMATCH"
                   for row in diagnostics):
            raise AssertionError(f"{label}/{backend}: wrong rejection: {result.stderr}")
    else:
        if result.returncode != 0 or not exe.is_file():
            raise AssertionError(f"{label}/{backend}: compile failed: {result.stderr}")
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        if run.returncode != 0 or run.stdout != want:
            raise AssertionError(f"{label}/{backend}: got {run.returncode}, "
                                 f"{run.stdout!r}; expected {want!r}; {run.stderr}")


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pgy", default=os.environ.get("PGY_BIN", root / "bin/pgy"))
    parser.add_argument("--backends", default="c llvm")
    parser.add_argument("--case", help="run one named falsifier; unknown names fail")
    args = parser.parse_args()
    pgy = Path(args.pgy).resolve()
    if not pgy.is_file():
        raise SystemExit(f"missing compiler: {pgy}")
    backends = args.backends.split()
    if not backends or any(backend not in ("c", "llvm") for backend in backends):
        raise SystemExit("request at least one backend: c llvm")
    passed = rejected = 0
    failures = []
    selected = [(label, source, want) for label, source, want in fixtures()
                if args.case is None or args.case == label]
    if not selected:
        raise SystemExit(f"unknown fixture: {args.case}")
    with tempfile.TemporaryDirectory(prefix="pgy-operator-rules-") as temp:
        work = Path(temp)
        for label, source, want in selected:
            path = work / (label + ".pgy")
            path.write_text(source, encoding="utf-8")
            for backend in backends:
                exe = work / f"{label}-{backend}.exe"
                result = subprocess.run(
                    [str(pgy), str(path), "--native-pipeline", "--backend=" + backend,
                     "--error-format=json", "-o", str(exe)],
                    cwd=root, capture_output=True, text=True, timeout=60,
                )
                try:
                    check_result(label, backend, result, exe, want)
                except (AssertionError, json.JSONDecodeError) as error:
                    failures.append(f"{label}/{backend}: {error}")
                else:
                    passed += want is not None
                    rejected += want is None
        if failures:
            raise SystemExit("\n".join(failures) +
                             f"\n[operator-rules] {len(failures)} failed; "
                             f"{passed} runtime, {rejected} rejection cases passed")
        scope = args.case or "72 role aliases + 11 free suffixes"
        print(f"[operator-rules] {passed} exact-output runs, {rejected} semantic rejections "
              f"PASS ({', '.join(backends)}; {scope})")


if __name__ == "__main__":
    main()
