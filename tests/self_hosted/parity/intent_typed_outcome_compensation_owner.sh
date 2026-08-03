#!/usr/bin/env bash
set -euo pipefail

# SUBSTITUTING consumer rung: native v2 MIR transition facts are admitted once
# and executed by the self-host MIR -> C path. Success, current-step failure,
# predecessor-only reverse compensation, duplicate expressions, and zero
# compensation are all executable observations rather than source-text probes.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-typed-compensation] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-typed-compensation" "$PGY" || fail "PGY_BIN is not runnable"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
PYTHON_BIN="${PYTHON_BIN:-python}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
TARGET_MUTATOR="$ROOT_DIR/tests/self_hosted/parity/intent_execution_graph_target_mutation_owner.py"
ZERO_MUTATOR="$ROOT_DIR/tests/self_hosted/parity/intent_execution_zero_compensation_mutation_owner.py"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_TYPED_COMPENSATION_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_typed_compensation}"
mkdir -p "$BUILD_DIR"
if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-typed-compensation" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2_$$.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")") \
        >"$BUILD_DIR/driver.compile.log" 2>&1 \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "fresh driver build failed"; }
fi

emit_and_run_self() {
    local label="$1"
    local source="$2"
    local mir="$BUILD_DIR/$label.mir.json"
    local c="$BUILD_DIR/$label.self.c"
    local exe="$BUILD_DIR/$label.self.exe"
    local run="$BUILD_DIR/$label.self.run"
    local mir_rel

    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
        "$(pgy_path_for_compiler "$PGY" "$source")" --backend=c) \
        >"$mir" 2>"$BUILD_DIR/$label.mir.err" \
        || { cat "$BUILD_DIR/$label.mir.err" >&2; fail "$label MIR emission failed"; }
    mir_rel="$(realpath --relative-to="$ROOT_DIR" "$mir")"
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "$mir_rel" >"$c" 2>"$BUILD_DIR/$label.self.err") \
        || { cat "$BUILD_DIR/$label.self.err" >&2; fail "$label self admission failed"; }
    "$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
        "$c" -o "$exe"
    "$exe" | tr -d '\r' >"$run"
}

cat >"$BUILD_DIR/expected.run" <<'EXPECTED'
success.kind=committed
success.payload=20
success.state=20
success.a_calls=1
success.b_calls=1
success.undo_a=0
success.undo_b=0
first_failure.kind=failed-a
first_failure.payload=101
first_failure.state=0
first_failure.a_calls=1
first_failure.b_calls=0
first_failure.undo_a=0
first_failure.undo_b=0
second_failure.kind=failed-b
second_failure.payload=202
second_failure.state=0
second_failure.a_calls=1
second_failure.b_calls=1
second_failure.undo_a=1
second_failure.undo_b=0
EXPECTED

emit_and_run_self base "$FIXTURE"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/base.self.run" \
    || { diff -u "$BUILD_DIR/expected.run" "$BUILD_DIR/base.self.run" >&2; fail "base execution drifted"; }

"$PYTHON_BIN" - "$BUILD_DIR/base.mir.json" <<'PY'
import json
from pathlib import Path
import sys

document = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
plan = document["intent_execution"]
assert plan["schema"] == "pgy.selfhost.mir-intent-execution-plan.v2", plan
assert isinstance(plan["plan_digest"], int) and plan["plan_digest"] != 0
assert len(plan["steps"]) == 2, plan["steps"]
assert len(plan["terminals"]) == 3, plan["terminals"]
steps = {row["step_name"]: row for row in plan["steps"]}
assert set(steps) == {"A", "B"}, steps
assert not steps["A"]["has_predecessor"], steps["A"]
assert steps["B"]["predecessor_transition_id"] == steps["A"]["transition_id"], steps
for row in plan["steps"]:
    assert row["success_payload_decl_syntax_id"] > 0, row
    assert row["failure_payload_decl_syntax_id"] > 0, row
for row in plan["terminals"]:
    assert row["source_payload_decl_syntax_id"] > 0, row
    assert row["result_payload_decl_syntax_id"] == row["source_payload_decl_syntax_id"], row
PY

sed '/compensate: actor.UndoA();/d' "$FIXTURE" \
    >"$BUILD_DIR/zero-compensation.pgy"
emit_and_run_self zero-compensation "$BUILD_DIR/zero-compensation.pgy"

"$PYTHON_BIN" - "$BUILD_DIR/base.mir.json" \
    "$BUILD_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
out = Path(sys.argv[2])
mutations = {}
def main_named_call(document):
    return next(
        row for routine in document["routines"] if routine["name"] == "Main"
        for block in routine["blocks"] for row in block["instructions"] if
        row.get("id") == 6 and row.get("arg0") == "Observe")
document = copy.deepcopy(base)
document["intent_execution"]["plan_digest"] += 1
mutations["plan-digest"] = document
for name in ("graph-digest-zero", "graph-digest-missing", "graph-extra-field"):
    document = copy.deepcopy(base)
    step = document["intent_execution"]["steps"][0]
    routine = next(row for row in document["routines"] if row.get("source_syntax_id") == step["routine_syntax_id"])
    block = next(row for row in routine["blocks"] if row["id"] == step["outcome_instruction_block_id"])
    graph = next(row["expr0_graph"] for row in block["instructions"] if row["id"] == step["outcome_instruction_id"])
    if name == "graph-digest-zero":
        graph["digest"] = 0
    elif name == "graph-digest-missing":
        graph.pop("digest")
    else:
        graph["foreign"] = True
    mutations[name] = document
document = copy.deepcopy(base)
named_call = main_named_call(document)
named_call.pop("expr0_graph")
mutations["named-call-graph-missing"] = document
document = copy.deepcopy(base)
named_call = main_named_call(document)
call_node = next(
    node for node in named_call["expr0_graph"]["nodes"]
    if node.get("kind") == "call" and
    node.get("call_target_name") == "Observe"
)
call_node["call_target_name"] = ""
mutations["named-call-target-missing"] = document
for name, document in mutations.items():
    (out / f"negative-{name}.mir.json").write_text(json.dumps(
        document, separators=(",", ":")), encoding="utf-8")
PY
"$PYTHON_BIN" "$TARGET_MUTATOR" "$BUILD_DIR/base.mir.json" "$BUILD_DIR"
"$PYTHON_BIN" "$ZERO_MUTATOR" "$BUILD_DIR/zero-compensation.mir.json" "$BUILD_DIR"
for negative in "$BUILD_DIR"/negative-*.mir.json; do
    stem="${negative%.mir.json}"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "$(realpath --relative-to="$ROOT_DIR" "$negative")" \
        >"$stem.out" 2>"$stem.err"); then
        fail "$(basename "$stem") was accepted"
    fi
    if grep -Eq '^#include|^typedef|WorkflowOutcome RunWorkflow\(' \
        "$stem.out" "$stem.err"; then
        fail "$(basename "$stem") emitted a partial C artifact"
    fi
done

sed '/compensate: actor.UndoA();/a\        compensate: actor.UndoB();' \
    "$FIXTURE" >"$BUILD_DIR/multiple.pgy"
emit_and_run_self multiple "$BUILD_DIR/multiple.pgy"
sed -e 's/^second_failure.state=0$/second_failure.state=-10/' \
    -e 's/^second_failure.undo_b=0$/second_failure.undo_b=1/' \
    "$BUILD_DIR/expected.run" >"$BUILD_DIR/multiple.expected.run"
cmp -s "$BUILD_DIR/multiple.expected.run" "$BUILD_DIR/multiple.self.run" \
    || { diff -u "$BUILD_DIR/multiple.expected.run" "$BUILD_DIR/multiple.self.run" >&2; fail "reverse compensation order drifted"; }

# The old text-unique mirror join rejected this legal shape. Exact graph
# occurrence identity must admit both identical source spellings.
sed '/compensate: actor.UndoA();/a\        compensate: actor.UndoA();' \
    "$FIXTURE" >"$BUILD_DIR/duplicate-expression.pgy"
emit_and_run_self duplicate-expression "$BUILD_DIR/duplicate-expression.pgy"
sed -e 's/^second_failure.state=0$/second_failure.state=-10/' \
    -e 's/^second_failure.undo_a=1$/second_failure.undo_a=2/' \
    "$BUILD_DIR/expected.run" >"$BUILD_DIR/duplicate-expression.expected.run"
cmp -s "$BUILD_DIR/duplicate-expression.expected.run" \
    "$BUILD_DIR/duplicate-expression.self.run" \
    || { diff -u "$BUILD_DIR/duplicate-expression.expected.run" "$BUILD_DIR/duplicate-expression.self.run" >&2; fail "duplicate expression multiset drifted"; }

sed -e 's/^second_failure.state=0$/second_failure.state=10/' \
    -e 's/^second_failure.undo_a=1$/second_failure.undo_a=0/' \
    "$BUILD_DIR/expected.run" >"$BUILD_DIR/zero-compensation.expected.run"
cmp -s "$BUILD_DIR/zero-compensation.expected.run" \
    "$BUILD_DIR/zero-compensation.self.run" \
    || { diff -u "$BUILD_DIR/zero-compensation.expected.run" "$BUILD_DIR/zero-compensation.self.run" >&2; fail "zero compensation cleanup drifted"; }

echo "[self-host-intent-typed-compensation] v2 plan + predecessor compensation variants: PASS"
