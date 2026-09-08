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
import pathlib
import re
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1])
pgy = sys.argv[2]
fixture = sys.argv[3]

owner = (root / "src/semantic/function_param_flow_summary.c").read_text(
    encoding="utf-8"
)
required_owner_terms = (
    "ast_node_stable_id(function_decl)",
    "function_param_flow_key_hash",
    "FUNCTION_PARAM_FLOW_COMPUTING",
    "FUNCTION_PARAM_FLOW_COMPLETE",
    "FUNCTION_PARAM_FLOW_WORK_BUDGET",
    "work_units",
    "ast_contains_identifier_ref",
    "slot_param_summary_in_program_points",
    "recursive summary work budget exceeded",
    "function_param_flow_summary_demand",
)
for term in required_owner_terms:
    if term not in owner:
        raise SystemExit(f"summary owner is missing {term!r}")
if "slot_param_summary_in_program(" in owner:
    raise SystemExit("summary owner reopened the full function-body transfer")

for rel in (
    "src/semantic/slot_analyzer_access.c",
    "src/semantic/slot_analyzer_escape.c",
):
    text = (root / rel).read_text(encoding="utf-8")
    if "function_param_flow_summary_demand(" not in text:
        raise SystemExit(f"{rel} does not consume the summary owner")
    if "slot_param_summary_in_program(" in text:
        raise SystemExit(f"{rel} reopened a callee body summary")

access = (root / "src/semantic/slot_analyzer_access.c").read_text(
    encoding="utf-8"
)
if re.search(r"slot_access_mask_for_named_symbol\s*\(\s*body\b", access):
    raise SystemExit("access propagation reopened a callee body")

env = os.environ.copy()
env["PGY_DEBUG_FUNCTION_PARAM_FLOW"] = "1"
env["PGY_DEBUG_RESOURCE_FLOW_FACTS"] = "1"
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
if not re.search(r"#define FUNCTION_PARAM_FLOW_WORK_BUDGET 4096u\b", owner):
    raise SystemExit("review the demand-budget controls before changing the cap")
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
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
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
    if shape == "independent":
        if pressure.returncode != 0 or tuple(map(int, telemetry.groups()[:2])) != (count, count):
            raise SystemExit("independent demands consumed an unrelated episode's budget")
    elif pressure.returncode == 0 or int(telemetry[2]) != 4096 or (
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

echo "[function-param-flow-summary] demanded recursive fixed point and no-reopen gates passed"
