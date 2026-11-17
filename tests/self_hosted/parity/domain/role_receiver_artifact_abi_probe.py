"""Compile erased slots against emitted C and test caller-cell mutation."""
import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("artifact", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--cc", default="gcc")
parser.add_argument("--labels", nargs="+", required=True)
parser.add_argument("--main-output", required=True)
parser.add_argument("--mutators", type=int, default=0)
parser.add_argument("--llvm-artifact", type=Path)
parser.add_argument("--clang", default="clang")
args = parser.parse_args()
source = args.artifact.read_text(encoding="utf-8")
signature = re.compile(
    r"^\s*(?:static\s+)?(const\s+char\s*\*|int(?:32|64)_t)\s*"
    r"(\w+)\(void\s*\*\s*(\w+)(?:,\s*(int(?:32|64)_t)\s+(\w+))?\)\s*\{",
    re.MULTILINE,
)
# Mask strings/comments so a diagnostic literal cannot terminate a C body.
masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',
                lambda match: " " * len(match[0]), source, flags=re.DOTALL)
calls = []
label_count = 0
mutation_count = 0
for match in signature.finditer(source):
    cursor, depth = match.end(), 1
    while depth and cursor < len(masked):
        depth += (masked[cursor] == "{") - (masked[cursor] == "}")
        cursor += 1
    if depth:
        raise SystemExit("unterminated emitted receiver body")
    result, function, receiver, argument_type, _ = match.groups()
    body = source[match.end():cursor - 1]
    binding = re.search(rf"\b(\w+)\s*\*\s*(\w+)\s*=\s*\(\1\s*\*\)\s*{receiver}\s*;", body)
    if binding is None:
        raise SystemExit("erased routine has no concrete receiver binding")
    concrete = binding[1]
    if result.startswith("const") and argument_type is None:
        calls += [f"{concrete} label_cell_{label_count} = {{0}};",
                  f"const char *(*label_slot_{label_count})(void *) = {function};",
                  f"puts(label_slot_{label_count}(&label_cell_{label_count}));"]
        label_count += 1
    elif argument_type is not None:
        layout = re.search(rf"\bstruct\s+{concrete}\s*\{{([^{{}}]*)\}}", source)
        if layout is None:
            raise SystemExit("receiver layout is absent")
        fields = re.findall(r"\bint(?:32|64)_t\s+(\w+)\s*;", layout[1])
        if len(fields) != 1:
            raise SystemExit("probe requires its fixture's single integer field")
        i = mutation_count
        calls += [f"{concrete} cell_{i} = {{0}};",
                  f"{result} (*mutation_slot_{i})(void *, {argument_type}) = {function};",
                  f"if (mutation_slot_{i}(&cell_{i}, 41) != 41 || cell_{i}.{fields[0]} != 41) return 71;"]
        mutation_count += 1
if label_count != len(args.labels) or mutation_count != args.mutators:
    raise SystemExit(f"nonvacuity failure: {label_count} labels, {mutation_count} mutators")
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(
    "#define main pgy_role_probe_original_main\n" + source +
    "\n#undef main\nint main(void) {\n" + "\n".join(calls) +
    "\nreturn pgy_role_probe_original_main();\n}\n", encoding="utf-8")
binary = args.output.with_suffix(".exe")
subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(args.output), "-o", str(binary)], check=True)
actual = subprocess.run([str(binary)], check=True, text=True, capture_output=True).stdout.splitlines()
if not actual or actual[-1] != args.main_output or Counter(actual[:-1]) != Counter(args.labels):
    raise SystemExit(f"receiver ABI output mismatch: {actual!r}")
print(f"erased receiver slots: {label_count} labels, {mutation_count} caller-cell mutations: PASS")

if args.llvm_artifact is not None:
    ir = args.llvm_artifact.read_text(encoding="utf-8")
    mutators = list(re.finditer(
        r"^define internal (i32|i64) @([\w.]+)\(ptr %[\w.]+, (i32|i64) %[\w.]+\) \{",
        ir, re.MULTILINE))
    if not mutators or len(mutators) != args.mutators:
        raise SystemExit("LLVM mutator nonvacuity failure")
    declarations, checks = [], []
    for i, match in enumerate(mutators):
        result, symbol, argument = match.groups()
        body = ir[match.end():ir.index("\n}", match.end())]
        receiver = re.search(r"getelementptr (%[\w.]+), ptr %", body)
        layout = None if receiver is None else re.search(
            rf"^{re.escape(receiver[1])} = type \{{ (i32|i64) \}}$", ir, re.MULTILINE)
        if layout is None:
            raise SystemExit("LLVM probe requires its fixture's single integer field")
        result_c, argument_c, field_c = (f"int{t[1:]}_t" for t in (result, argument, layout[1]))
        declarations.append(f"extern {result_c} pgy_role_probe_{i}(void *, {argument_c});")
        checks += [f"struct {{ {field_c} value; }} cell_{i} = {{0}};",
                   f"{result_c} (*slot_{i})(void *, {argument_c}) = pgy_role_probe_{i};",
                   f"if (slot_{i}(&cell_{i}, 41) != 41 || cell_{i}.value != 41) return 71;"]
    # Export under portable C names so the target owns symbol decoration.
    # The tested function bodies and admitted layouts stay unchanged.
    for match in reversed(mutators):
        ir = ir[:match.start()] + ir[match.start():].replace("define internal ", "define ", 1)
    for i, match in enumerate(mutators):
        ir = ir.replace(f"@{match[2]}(", f"@pgy_role_probe_{i}(")
    ir = ir.replace("@main(", "@pgy_role_original_main(")
    ir_path = args.output.with_suffix(".ll")
    ir_path.write_text(ir, encoding="utf-8")
    wrapper = args.output.with_name(args.output.stem + "-llvm.c")
    wrapper.write_text("#include <stdint.h>\n" + "\n".join(declarations) +
                       "\nint main(void) {\n" + "\n".join(checks) + "\nreturn 0;\n}\n", encoding="utf-8")
    executable = wrapper.with_suffix(".exe")
    subprocess.run([args.clang, "-O0", "-x", "ir", str(ir_path), "-x", "c",
                    str(wrapper), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    print(f"LLVM erased receiver slots: {len(mutators)} caller-cell mutations: PASS")
