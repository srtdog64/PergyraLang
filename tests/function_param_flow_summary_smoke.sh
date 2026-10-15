#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_BIN="$(pgy_select_optional_exe_binary "$PGY_BIN")"
pgy_require_runnable_binary_here "function-param-flow-summary" "$PGY_BIN"

PYTHON_BIN="${PYTHON_BIN:-python}"
FIXTURE="$ROOT_DIR/tests/cases/function_param_flow_summary/main.pgy"

"$PYTHON_BIN" - "$ROOT_DIR" "$PGY_BIN" "$FIXTURE" <<'PY'
import os
import json
import pathlib
import re
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1])
pgy = sys.argv[2]
fixture = sys.argv[3]

# Verify the owner's admitted artifacts and refusal behavior below. Source
# spelling is not an oracle for summary identity or bounded evaluation.

env = os.environ.copy()
env["PGY_DEBUG_FUNCTION_PARAM_FLOW"] = "1"
env["PGY_DEBUG_RESOURCE_FLOW_FACTS"] = "1"
# Semantic-only paired controls: no unsafe negative is emitted or executed.
paired = (
    ("stdlib_input_retention_valid", True),
    ("stdlib_trim_result_return_valid", True),
    ("stdlib_trim_result_retain_invalid", False),
    ("stdlib_trim_opaque_invalid", False),
    ("private_array_push_local_read_valid", True),
    ("private_array_push_return_escape_invalid", False),
    ("private_array_push_wrapper_escape_invalid", False),
    ("physical_return_default_string_valid", True),
    ("physical_return_default_record_valid", True),
    ("physical_return_opaque_invalid", False),
    ("physical_return_slot_invalid", False),
    ("physical_return_inout_invalid", False),
    ("default_string_string_return_invalid", False),
    ("default_string_option_return_invalid", False),
    ("direct_ref_string_loan_valid", True),
    ("return_loan_direct_slot_invalid", False),
    ("return_loan_staged_slot_invalid", False),
    ("return_loan_wrap_slot_invalid", False),
    ("return_loan_inout_invalid", False),
    ("return_loan_mutated_literal_opaque_slot_invalid", False),
    ("return_loan_mutated_literal_opaque_inout_invalid", False),
    ("return_loan_mutated_literal_assignment_invalid", False),
    ("return_loan_mutated_literal_bodyful_inout_invalid", False),
    ("return_loan_literal_slot_valid", True),
    ("return_loan_read_static_local_valid", True),
    ("direct_ref_string_retaining_literal_valid", True),
    ("direct_ref_string_retaining_concat_invalid", False),
    ("direct_ref_string_local_retain_invalid", False),
    ("direct_ref_string_slot_invalid", False),
    ("direct_ref_string_channel_invalid", False),
    ("direct_ref_string_opaque_literal_invalid", False),
    ("direct_ref_string_opaque_concat_invalid", False),
    ("direct_ref_string_alias_literal_invalid", False),
    ("direct_ref_string_alias_concat_invalid", False),
    ("direct_ref_string_alias_named_invalid", False),
    ("recursive_record_wrapper_control_invalid", False),
)
for name, valid in paired:
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10.0, check=False)
    if valid:
        if checked.returncode != 0 or '"pgy.mir.v1"' not in checked.stdout.decode(errors="replace"):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical-return positive {name} failed")
    else:
        output = (checked.stdout + checked.stderr).decode(errors="replace")
        try:
            diagnostics = json.loads(output)
        except json.JSONDecodeError:
            raise SystemExit(f"physical-return negative {name} lost structured diagnostics: {output}")
        errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
        if checked.returncode != 1 or '"pgy.mir.v1"' in output or not errors or any(
                row.get("code") != "PGY_SEM_BORROW_ESCAPE" or row.get("cause_ir") != "semantic:borrow_escape"
                for row in errors):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical-return negative {name} lost its exact borrow refusal")
        retained_sites = {
            "direct_ref_string_local_retain_invalid": (8, 20),
            "direct_ref_string_slot_invalid": (7, 30),
            "direct_ref_string_channel_invalid": (9, 25),
            "direct_ref_string_retaining_concat_invalid": (4, 16),
            "direct_ref_string_opaque_literal_invalid": (3, 5),
            "direct_ref_string_opaque_concat_invalid": (3, 14),
            "direct_ref_string_alias_literal_invalid": (4, 5),
            "direct_ref_string_alias_concat_invalid": (4, 14),
            "direct_ref_string_alias_named_invalid": (5, 14),
            "recursive_record_wrapper_control_invalid": (11, 6),
        }
        if name in retained_sites and not any(
                (row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                == retained_sites[name] for row in errors):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"ref-retaining negative {name} lost its owned refusal site")
print("[function-param-flow-summary] physical-return input/store pairs: PASS")
for name, code, cause in (
    ("stdlib_trim_arity_invalid", "PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch"),
    ("stdlib_toint_arity_invalid", "PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch"),
    ("stdlib_trim_shadow_invalid", "PGY_SEM_REDECLARATION", "semantic:function:builtin_name_reserved"),
    ("stdlib_toint_shadow_invalid", "PGY_SEM_REDECLARATION", "semantic:function:builtin_name_reserved"),
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    diagnostics = json.loads(output)
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"]
    if checked.returncode != 1 or '"pgy.mir.v1"' in output or not any(
            row.get("code") == code and row.get("cause_ir") == cause for row in errors):
        raise SystemExit(f"stdlib identity/arity negative {name} lost {code}/{cause}")
runtime_dir = pathlib.Path(tempfile.mkdtemp(prefix="stdlib-input-retention."))
for name, expected in (
    ("stdlib_input_retention_valid", "12\n2\n0\n-2147483648\n\n"),
    ("stdlib_trim_result_return_valid", "heap text\n"),
    ("private_array_push_local_read_valid", "1\n"),
    ("return_loan_literal_slot_valid", "true\nstatic literal\n"),
    ("return_loan_read_static_local_valid", "14\n14\nstatic literal\n"),
):
    for backend in ("c", "llvm"):
        executable = runtime_dir / (name + "-" + backend + ".exe")
        source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
        compiled = subprocess.run(
            [pgy, "--native-pipeline", "--backend=" + backend, "--opt=dev", str(source), "-o", str(executable)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60.0, check=False)
        if compiled.returncode != 0:
            sys.stderr.buffer.write(compiled.stderr)
            raise SystemExit(f"stdlib input reader {name} failed to compile for {backend}")
        observed = subprocess.run([str(executable)], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=10.0, check=False)
        if observed.returncode != 0 or observed.stderr or observed.stdout.decode().replace("\r\n", "\n") != expected:
            raise SystemExit(f"stdlib input reader {name} runtime mismatch for {backend}: {observed.stdout!r}")
print("[function-param-flow-summary] stdlib input retention C/LLVM and identity/arity controls: PASS")
try:
    run = subprocess.run(
        [pgy, "--native-pipeline", "--hir", fixture],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        timeout=10.0,
        check=False,
    )
except subprocess.TimeoutExpired:
    raise SystemExit("recursive summary fixture exceeded 10 seconds")

if run.returncode != 0:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(f"recursive summary fixture failed with {run.returncode}")

resource_fixture = root / "tests/cases/slot_contract/positive/plain_read_write_release/main.pgy"
resource_run = subprocess.run(
    [pgy, "--native-pipeline", "--hir", str(resource_fixture)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=env,
    timeout=10.0,
    check=False,
)
if resource_run.returncode != 0:
    sys.stderr.buffer.write(resource_run.stderr)
    raise SystemExit(
        f"resource-flow HIR fixture failed with {resource_run.returncode}"
    )
hir_symbols = re.findall(
    rb"resource-flow-symbols=(\d+)", resource_run.stdout
)
if not hir_symbols or max(map(int, hir_symbols)) < 1:
    sys.stderr.buffer.write(resource_run.stdout)
    raise SystemExit("HIR did not carry ResourceFlowUniverse symbols")

match = re.search(
    rb"pgy: function-param-flow entries=(\d+) body_evaluations=(\d+) "
    rb"cache_hits=(\d+) recursion_hits=(\d+) fixed_point_passes=(\d+)",
    run.stderr,
)
if match is None:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit("function parameter flow summary telemetry is missing")

entries, evaluations, cache_hits, recursion_hits, passes = map(
    int, match.groups()
)
if entries < 1:
    raise SystemExit("expected at least one demanded summary")
if recursion_hits < 1:
    raise SystemExit("mutual recursion was not detected")
if evaluations <= entries:
    raise SystemExit("recursive component did not perform a fixed-point revisit")
if evaluations > 8 or passes > 6:
    raise SystemExit(
        "recursive component exceeded its bounded fixture budget: "
        f"evaluations={evaluations} passes={passes}"
    )
sparse = re.search(
    rb"pgy: function-param-flow-sparse functions=(\d+) "
    rb"statement_visits=(\d+) program_points=(\d+)",
    run.stderr,
)
if sparse is None:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit("function parameter flow sparse telemetry is missing")
indexed_functions, statement_visits, program_points = map(int, sparse.groups())
if indexed_functions < 1 or statement_visits <= 0:
    raise SystemExit("sparse owner did not index the demanded function")
if program_points >= statement_visits:
    raise SystemExit(
        "summary evaluator did not reduce the demanded program points: "
        f"statement_visits={statement_visits} program_points={program_points}"
    )
print(
    "[function-param-flow-summary] "
    f"entries={entries} evaluations={evaluations} cache_hits={cache_hits} "
    f"recursion_hits={recursion_hits} passes={passes} "
    f"statement_visits={statement_visits} program_points={program_points}"
)

# The work cap belongs to one demanded closure, not the entire compilation.
# Independent fresh queries may exceed it in aggregate; one oversized closure
# must still fail. Neither case has a deep stack or recursive call edges.
(root / ".tmp").mkdir(exist_ok=True)
pressure_dir = pathlib.Path(tempfile.mkdtemp(
    prefix="function-param-flow-budget.", dir=root / ".tmp"
))
pressure_env = os.environ.copy()
pressure_env["PGY_DEBUG_FUNCTION_PARAM_FLOW"] = "1"
pressure_env.pop("PGY_DEBUG_RESOURCE_FLOW_FACTS", None)
for shape, count in (("independent", 4097), ("single-closure", 4096)):
    leaves = "\n".join(
        f"func Leaf{i}(ref values: Array<Int>) -> Int {{ return values[0]; }}"
        for i in range(count)
    )
    calls = "\n".join(f"    Leaf{i}(values);" for i in range(count))
    if shape == "independent":
        source_text = leaves + (
            "\nfunc Main() -> Void {\n    let values: Array<Int> = [7];\n"
            + calls + "\n}\n"
        )
    else:
        source_text = (
            "func Entry(ref values: Array<Int>) -> Int { return FanOut(values); }\n"
            "func FanOut(ref values: Array<Int>) -> Int {\n" + calls
            + "\n    return 0;\n}\n" + leaves
            + "\nfunc Main() -> Void { let values: Array<Int> = [7]; Entry(values); }\n"
        )
    source_path = pressure_dir / f"{shape}.pgy"
    source_path.write_text(source_text, encoding="utf-8")
    try:
        pressure = subprocess.run(
            [pgy, "--native-pipeline", "--hir", str(source_path)],
            stdout=subprocess.PIPE if shape == "single-closure" else subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            env=pressure_env, timeout=30.0, check=False,
        )
    except subprocess.TimeoutExpired:
        raise SystemExit(f"{shape} demand-budget control exceeded 30 seconds")
    (pressure_dir / f"{shape}.err").write_bytes(pressure.stderr)
    telemetry = re.search(
        rb"function-param-flow entries=(\d+) body_evaluations=(\d+) "
        rb"cache_hits=(\d+) recursion_hits=(\d+)", pressure.stderr,
    )
    if telemetry is None or int(telemetry[4]) != 0:
        raise SystemExit(f"{shape} control lost its nonrecursive telemetry")
    work = re.search(rb"function-param-flow-work work_units=(\d+) work_budget=(\d+)",
                     pressure.stderr)
    if work is None or int(work[2]) != 4096 or int(work[1]) > int(work[2]):
        raise SystemExit(f"{shape} control lost its actual demand-episode work boundary")
    if shape == "independent":
        if pressure.returncode != 0 or tuple(map(int, telemetry.groups()[:2])) != (count, count):
            raise SystemExit("independent demands consumed an unrelated episode's budget")
    elif pressure.returncode != 1 or pressure.stdout or int(work[1]) != 4096 or (
        b"recursive summary work budget exceeded" not in pressure.stderr
    ):
        raise SystemExit("one oversized demand closure escaped the unchanged work cap")
print(f"[function-param-flow-summary] independent/oversized demand controls: PASS; {pressure_dir}")
PY

ESCAPE_FIXTURE="$ROOT_DIR/tests/cases/function_param_flow_summary/escape_negative.pgy"
ESCAPE_ERR="${TMPDIR:-$ROOT_DIR/.tmp}/function_param_flow_escape_negative.err"
mkdir -p "$(dirname "$ESCAPE_ERR")"
if "$PGY_BIN" --native-pipeline --hir "$ESCAPE_FIXTURE" >/dev/null 2>"$ESCAPE_ERR"; then
    echo "escape-negative fixture unexpectedly passed" >&2
    cat "$ESCAPE_ERR" >&2
    exit 1
fi
for expected in \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through return" \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through channel send" \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through helper/function call"; do
    if ! grep -Fq "$expected" "$ESCAPE_ERR"; then
        echo "escape-negative fixture missed diagnostic: $expected" >&2
        cat "$ESCAPE_ERR" >&2
        exit 1
    fi
done

echo "[function-param-flow-summary] recursive fixed point, refusal and demand-budget controls passed"
