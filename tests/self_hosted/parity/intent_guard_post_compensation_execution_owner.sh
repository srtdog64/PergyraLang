#!/usr/bin/env bash
set -euo pipefail

# Executable intent contract: an On action completes before its guard/expect/
# post predicates. Failure runs completed steps in reverse, each step's
# compensate expressions in reverse, and the installed self-host path must
# publish the same completed-step/failure history as native C and LLVM.
# CLOSED `selfhost.intent_declaration_rows` fallback inventory pinned here:
# intent_signature_as_function_row raw_source_intent_signature
# unknown_intent_parameter_success duplicate_intent_parameter_success
# value_participant_as_authority intent_kind_fallback commit_identity_drift
# binding_type_drift zone_alias_drift authorization_cross_carrier
# rollback_identity_drift outcome_bool_collapse variant_spelling_classification
# payload_type_reinfer predecessor_from_source_order completion_after_any_call
# compensate_ast_rescan final_intent_ast_child_rescan missing_source_dir_receipt
# crossed_source_dir_identity unknown_intent_phase orphan_intent_phase
# wrong_intent_phase_step_or_slot duplicate_intent_phase
# check_or_compensate_result_type on_result_type_asymmetry
# missing_intent_phase_graph old_direct_orchestration consumer_plan_revalidation
# expression_graph_reconstruction name_only_payload_declaration_join
# reachable_zero_compensation_scaffold typed_direct_rollback_bypass
# nested_intent_as_action_step nested_intent_fake_placement_materialization
# unconditional_action_authority_requirement

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-compensation] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-compensation" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_guard_post_compensation_execution.pgy"
PROBE_REL="tests/self_hosted/parity/fixture/intent_declaration_rows_negative_probe.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_COMPENSATION_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_guard_post_compensation}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-compensation" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_FROM_MIR_C="$BUILD_DIR/self.from-mir.c"
SELF_DIRECT_C="$BUILD_DIR/self.direct.c"
SELF_EXE="$BUILD_DIR/self.exe"
NATIVE_C_EXE="$BUILD_DIR/native.c.exe"
NATIVE_LLVM_EXE="$BUILD_DIR/native.llvm.exe"
PUBLIC_EXE="$BUILD_DIR/public.exe"
PROBE_C="$BUILD_DIR/intent-declaration-rows-probe.c"
PROBE_EXE="$BUILD_DIR/intent-declaration-rows-probe.exe"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#"$ROOT_DIR/"}" \
    >"$SELF_FROM_MIR_C" 2>"$BUILD_DIR/self.from-mir.err") \
    || { cat "$SELF_FROM_MIR_C" "$BUILD_DIR/self.from-mir.err" >&2; fail "admitted MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$SELF_DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$SELF_DIRECT_C" "$BUILD_DIR/self.direct.err" >&2; fail "direct source C emission failed"; }
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-intent-compensation" "$BUILD_DIR" \
    "$SELF_FROM_MIR_C" "$SELF_DIRECT_C" "emitted_c"

"$PYTHON_BIN" - "$SELF_DIRECT_C" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
start = text.index("bool RunWorkflow(")
end = text.index("\n}\n\n", start)
body = text[start:end]
assert body.count("__intent_step_completed[0] = true;") == 1, body
assert body.count("__intent_step_completed[1] = true;") == 1, body
assert body.count("goto __intent_cleanup;") >= 3, body
assert body.count("if (__intent_failed)") == 1, body
assert body.count("pgy_intent_trace_step_export(__intent_handle") == 2, body
assert body.count("pgy_intent_trace_step_ok_export(__intent_handle") == 2, body
assert body.count("pgy_intent_trace_fail_export(__intent_handle") >= 3, body
assert body.count("pgy_intent_exit_export(__intent_handle)") >= 1, body
cleanup = body.index("__intent_cleanup:")
second = body.index("WorkflowActor_UndoBSecond", cleanup)
first = body.index("WorkflowActor_UndoBFirst", second)
undo_a = body.index("WorkflowActor_UndoA", first)
assert cleanup < second < first < undo_a, body
PY

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_DIRECT_C" -o "$SELF_EXE"
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --native-pipeline --backend=c -o "$NATIVE_C_EXE" \
    >"$BUILD_DIR/native.c.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.c.compile.log" >&2; fail "native C compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --native-pipeline --backend=llvm -o "$NATIVE_LLVM_EXE" \
    >"$BUILD_DIR/native.llvm.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.llvm.compile.log" >&2; fail "native LLVM compile failed"; }
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
public_arg="$(pgy_path_for_compiler "$PGY" "$PUBLIC_EXE")"
(cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$FIXTURE_REL" --backend=c \
    -o "$public_arg" >"$BUILD_DIR/public.compile.out" \
    2>"$BUILD_DIR/public.compile.err") \
    || { cat "$BUILD_DIR/public.compile.err" >&2; fail "installed public C compile failed"; }
! grep -Fq '[pipeline timing]' "$BUILD_DIR/public.compile.err" ||
    fail "installed public route re-entered the native compiler"

(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --native-pipeline --emit-c \
    -o "$PROBE_C" >"$BUILD_DIR/probe.compile.out" \
    2>"$BUILD_DIR/probe.compile.err") \
    || { cat "$BUILD_DIR/probe.compile.err" >&2; fail "negative probe C emission failed"; }
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$PROBE_C" -o "$PROBE_EXE"
for mode in actor-crosswire using-crosswire authority-crosswire \
    target-crosswire guard-crosswire post-crosswire expect-crosswire \
    compensation-crosswire step-range-crosswire missing-source-view \
    artifact-epoch-crosswire; do
    (cd "$ROOT_DIR" && "$PROBE_EXE" "$FIXTURE_REL" "$mode" \
        >"$BUILD_DIR/$mode.out" 2>"$BUILD_DIR/$mode.err") \
        || { cat "$BUILD_DIR/$mode.err" >&2; fail "$mode probe failed"; }
    cat "$BUILD_DIR/$mode.out" "$BUILD_DIR/$mode.err" | \
        grep -Fxq "rejected:$mode" || fail "$mode was admitted"
done

"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/self.run"
"$NATIVE_C_EXE" | tr -d '\r' >"$BUILD_DIR/native.c.run"
"$NATIVE_LLVM_EXE" | tr -d '\r' >"$BUILD_DIR/native.llvm.run"
"$PUBLIC_EXE" | tr -d '\r' >"$BUILD_DIR/public.run"
printf '%s\n' \
    'success.ok=true' \
    'success.state=13' \
    'success.trace=12' \
    'success.a_calls=1' \
    'success.b_calls=1' \
    'success.undo_a=0' \
    'success.undo_b_first=0' \
    'success.undo_b_second=0' \
    'first_guard.ok=false' \
    'first_guard.state=0' \
    'first_guard.trace=15' \
    'first_guard.a_calls=1' \
    'first_guard.b_calls=0' \
    'first_guard.undo_a=1' \
    'first_guard.undo_b_first=0' \
    'first_guard.undo_b_second=0' \
    'guard.ok=false' \
    'guard.state=0' \
    'guard.trace=12345' \
    'guard.a_calls=1' \
    'guard.b_calls=1' \
    'guard.undo_a=1' \
    'guard.undo_b_first=1' \
    'guard.undo_b_second=1' \
    'expect.ok=false' \
    'expect.state=0' \
    'expect.trace=12345' \
    'expect.a_calls=1' \
    'expect.b_calls=1' \
    'expect.undo_a=1' \
    'expect.undo_b_first=1' \
    'expect.undo_b_second=1' \
    'post.ok=false' \
    'post.state=0' \
    'post.trace=12345' \
    'post.a_calls=1' \
    'post.b_calls=1' \
    'post.undo_a=1' \
    'post.undo_b_first=1' \
    'post.undo_b_second=1' \
    'post.history.count=2' \
    'post.history.name0=ForwardA' \
    'post.history.phase0=ok' \
    'post.history.ok0=true' \
    'post.history.name1=ForwardB' \
    'post.history.phase1=fail' \
    'post.history.ok1=false' \
    'post.history.failure1=post:ForwardB' \
    'post.last.failed=true' \
    'post.last.failure=post:ForwardB' \
    'post.active.after=0' >"$BUILD_DIR/expected.run"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-intent-compensation" "$BUILD_DIR" \
    "$BUILD_DIR/expected.run" "$BUILD_DIR/self.run" "run_output"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-intent-compensation" "$BUILD_DIR" \
    "$BUILD_DIR/self.run" "$BUILD_DIR/native.c.run" "run_output"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-intent-compensation" "$BUILD_DIR" \
    "$BUILD_DIR/self.run" "$BUILD_DIR/native.llvm.run" "run_output"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-intent-compensation" "$BUILD_DIR" \
    "$BUILD_DIR/self.run" "$BUILD_DIR/public.run" "run_output"

echo "[self-host-intent-compensation] guard/expect/post failure + ordered compensation + history parity: PASS"
