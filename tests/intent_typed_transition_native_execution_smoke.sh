#!/usr/bin/env bash
set -euo pipefail

# Native execution gate for the admitted MIR intent transition plan.  This is
# deliberately not self-host evidence: the self-host driver remains a separate
# substitution rung until it consumes this plan through its production entry.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[intent-typed-transition-native] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "intent-typed-transition-native" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python is required"
    fi
fi

FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
BUILD_DIR="${PGY_INTENT_TYPED_NATIVE_BUILD_DIR:-$ROOT_DIR/.tmp/native/intent_typed_transition}"
MIR_JSON="$BUILD_DIR/intent.mir.json"
EMITTED_C="$BUILD_DIR/intent.c"
MULTI_FIXTURE="$BUILD_DIR/intent_multiple_compensation.pgy"
mkdir -p "$BUILD_DIR"

run_backend() {
    local source="$1"
    local backend="$2"
    local output="$3"
    local binary="$BUILD_DIR/$(basename "$source" .pgy).$backend.exe"
    local raw="$output.raw"

    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" --run \
        -o "$(pgy_path_for_compiler "$PGY" "$binary")") \
        >"$raw" 2>"$output.err" \
        || { cat "$raw" "$output.err" >&2; fail "$backend execution failed"; }
    tr -d '\r' <"$raw" \
        | sed -e '/^pgy: compiled/d' -e '/^pgy: wrote/d' >"$output"
}

(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" --backend=c) \
    >"$MIR_JSON" 2>"$BUILD_DIR/intent.mir.err" \
    || { cat "$BUILD_DIR/intent.mir.err" >&2; fail "MIR JSON emission failed"; }
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" --backend=c --emit-c \
    -o "$(pgy_path_for_compiler "$PGY" "$EMITTED_C")") \
    >"$BUILD_DIR/intent.emit.out" 2>"$BUILD_DIR/intent.emit.err" \
    || { cat "$BUILD_DIR/intent.emit.err" >&2; fail "C emission failed"; }

"$PYTHON_BIN" - "$MIR_JSON" "$EMITTED_C" <<'PY'
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
a = steps["A"]
b = steps["B"]
assert a["transition_id"] == a["step_syntax_id"] != 0, a
assert not a["has_predecessor"], a
assert b["transition_id"] == b["step_syntax_id"] != 0, b
assert b["has_predecessor"], b
assert b["predecessor_transition_id"] == a["transition_id"], b
assert b["predecessor_step_syntax_id"] == a["step_syntax_id"], b
assert b["predecessor_step_name"] == "A", b
for row in (a, b):
    assert row["outcome_enum_syntax_id"] != 0, row
    assert row["success_payload_decl_syntax_id"] != 0, row
    assert row["failure_payload_decl_syntax_id"] != 0, row
    assert row["success_variant_index"] != row["failure_variant_index"], row
    assert row["success_payload_name"], row
    assert row["failure_payload_name"], row
assert [row["call_target_name"] for row in a["compensations"]] == ["UndoA"], a

terminal_by_role_source = {
    (row["role"], row["source_transition_id"]): row
    for row in plan["terminals"]
}
assert set(terminal_by_role_source) == {
    ("success", b["transition_id"]),
    ("failure", a["transition_id"]),
    ("failure", b["transition_id"]),
}, terminal_by_role_source
for row in plan["terminals"]:
    assert row["terminal_transition_id"] != 0, row
    assert row["result_enum_syntax_id"] != 0, row
    assert row["source_payload_decl_syntax_id"] != 0, row
    assert row["result_payload_decl_syntax_id"] == \
        row["source_payload_decl_syntax_id"], row
    assert row["result_variant_index"] >= 0, row
    assert row["expression_syntax_id"] != 0, row

text = Path(sys.argv[2]).read_text(encoding="utf-8")
start = text.rindex("RunWorkflow(WorkflowZone *work")
end = text.index("\n}\n\n", start)
body = text[start:end]
assert "__intent_result" not in body, body
assert "goto __intent_transition_" in body, body
assert "outcome_a.tag == (WorkflowOutcomeA_Tag)0" in body, body
assert "outcome_a.tag == (WorkflowOutcomeA_Tag)1" in body, body
assert "outcome_b.tag == (WorkflowOutcomeB_Tag)0" in body, body
assert "outcome_b.tag == (WorkflowOutcomeB_Tag)1" in body, body
assert body.count("__intent_step_completed[0] = true;") == 1, body
assert body.count("__intent_step_completed[1] = true;") == 1, body
failed_b = body.index("return WorkflowOutcome_WorkflowFailedB(problem_b);")
assert body.rfind("WorkflowActor_UndoA(actor);", 0, failed_b) >= 0, body
assert "WorkflowActor_UndoB(actor);" not in body[:failed_b], body
PY

run_backend "$FIXTURE" c "$BUILD_DIR/c.run"
run_backend "$FIXTURE" llvm "$BUILD_DIR/llvm.run"
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
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/c.run" \
    || { diff -u "$BUILD_DIR/expected.run" "$BUILD_DIR/c.run" >&2; fail "C output drifted"; }
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/llvm.run" \
    || { diff -u "$BUILD_DIR/expected.run" "$BUILD_DIR/llvm.run" >&2; fail "LLVM output drifted"; }

# Two compensations on predecessor A distinguish reverse list order from both
# source order and a stale single-compensation consumer.  UndoB then UndoA
# leaves state=-10; the opposite order would leave state=0.
sed '/compensate: actor.UndoA();/a\        compensate: actor.UndoB();' \
    "$FIXTURE" >"$MULTI_FIXTURE"
run_backend "$MULTI_FIXTURE" c "$BUILD_DIR/multi.c.run"
run_backend "$MULTI_FIXTURE" llvm "$BUILD_DIR/multi.llvm.run"
sed -e 's/^second_failure.state=0$/second_failure.state=-10/' \
    -e 's/^second_failure.undo_b=0$/second_failure.undo_b=1/' \
    "$BUILD_DIR/expected.run" >"$BUILD_DIR/multi.expected.run"
cmp -s "$BUILD_DIR/multi.expected.run" "$BUILD_DIR/multi.c.run" \
    || { diff -u "$BUILD_DIR/multi.expected.run" "$BUILD_DIR/multi.c.run" >&2; fail "C reverse compensation order drifted"; }
cmp -s "$BUILD_DIR/multi.expected.run" "$BUILD_DIR/multi.llvm.run" \
    || { diff -u "$BUILD_DIR/multi.expected.run" "$BUILD_DIR/multi.llvm.run" >&2; fail "LLVM reverse compensation order drifted"; }

for owner in \
    "$ROOT_DIR/src/codegen/transpiler_intent_typed_execution.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_typed_execution.c"; do
    grep -Fq 'row->outcome_expression' "$owner" \
        || fail "typed consumer lost sealed outcome expression read: $owner"
    grep -Fq 'terminal->expression' "$owner" \
        || fail "typed consumer lost sealed terminal expression read: $owner"
    grep -Fq 'predecessor_transition_id' "$owner" \
        || fail "typed consumer lost exact predecessor identity read: $owner"
    grep -Fq 'variant_index' "$owner" \
        || fail "typed consumer lost sealed numeric variant identity read: $owner"
    grep -Fq 'for (size_t i = predecessor->compensation_count; i > 0; i--)' "$owner" \
        || fail "typed consumer lost reverse compensation traversal: $owner"
    if grep -Eq 'ast_intent_(step|terminal)|compensations\[0\]|compensation_count[[:space:]]*>[[:space:]]*1' "$owner"; then
        fail "typed consumer reintroduced AST/name/first-row fallback: $owner"
    fi
done

echo "[intent-typed-transition-native] PASS"
