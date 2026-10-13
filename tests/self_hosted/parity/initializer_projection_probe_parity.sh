#!/usr/bin/env bash
# CLOSED initializer verdict fallbacks:
# semantic_normalization_char_at
# semantic_normalization_trim_copy
# semantic_validation_trim_copy
# Focused C/LLVM proof for semantic initializer type projection into MIR.
# Registry ratchets: source_initializer_type_rescan and
# backend_initializer_type_guess are forbidden in every last consumer.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:initializer-projection"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/initializer_projection}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/expected.txt"
DIRECT_CALL_EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/direct_call_expected.txt"
NOMINAL_CALL_EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/nominal_call_expected.txt"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$DIRECT_CALL_EXPECTED" \
    "$NOMINAL_CALL_EXPECTED"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    # The probe binary is harness infrastructure: the llvm build rides the
    # native pipeline because the delegated DirectMirLlvm projector is a
    # bounded classifier (replacement subject:
    # self-host-default-llvm-replacement-test-smoke).
    local native_subject=""
    [[ "$backend" == "llvm" ]] && native_subject="--native-pipeline"
    rm -f "$bin" "$BUILD_DIR/probe_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend="$backend" \
        $native_subject \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_positive_probe_capture() {
    local backend="$1"
    local bin="$2"
    local raw="$3"
    shift 3
    if ! (cd "$ROOT_DIR" && "$bin" "$@" >"$raw"); then
        echo "[$LABEL] backend=$backend positive probe failed: ${*:-<default>}" >&2
        [[ ! -s "$raw" ]] || cat "$raw" >&2
        exit 1
    fi
}

run_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local raw="$BUILD_DIR/probe_${backend}.raw"
    local out="$BUILD_DIR/probe_${backend}.out"
    run_positive_probe_capture "$backend" "$bin" "$raw"
    pgy_selfhost_normalize_text_artifact <"$raw" >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"

    local direct_raw="$BUILD_DIR/probe_${backend}.direct_call.raw"
    local direct_out="$BUILD_DIR/probe_${backend}.direct_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$direct_raw" --direct-call-positive
    pgy_selfhost_normalize_text_artifact <"$direct_raw" >"$direct_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$direct_out" \
        "run_output"

    local nested_raw="$BUILD_DIR/probe_${backend}.direct_call_nested.raw"
    local nested_out="$BUILD_DIR/probe_${backend}.direct_call_nested.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$nested_raw" --direct-call-nested-positive
    pgy_selfhost_normalize_text_artifact <"$nested_raw" >"$nested_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$nested_out" \
        "run_output"

    local nested_call_raw="$BUILD_DIR/probe_${backend}.direct_call_nested_call.raw"
    local nested_call_out="$BUILD_DIR/probe_${backend}.direct_call_nested_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$nested_call_raw" \
        --direct-call-nested-call-positive
    pgy_selfhost_normalize_text_artifact <"$nested_call_raw" \
        >"$nested_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$nested_call_out" \
        "run_output"

    local scalar_call_raw="$BUILD_DIR/probe_${backend}.scalar_call.raw"
    local scalar_call_out="$BUILD_DIR/probe_${backend}.scalar_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$scalar_call_raw" --scalar-call-positive
    pgy_selfhost_normalize_text_artifact <"$scalar_call_raw" \
        >"$scalar_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$scalar_call_out" \
        "run_output"

    local namespace_raw="$BUILD_DIR/probe_${backend}.namespace_call.raw"
    local namespace_out="$BUILD_DIR/probe_${backend}.namespace_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$namespace_raw" --namespace-call-positive
    pgy_selfhost_normalize_text_artifact <"$namespace_raw" \
        >"$namespace_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$namespace_out" \
        "run_output"

    local member_raw="$BUILD_DIR/probe_${backend}.member_call.raw"
    local member_out="$BUILD_DIR/probe_${backend}.member_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$member_raw" --member-call-positive
    pgy_selfhost_normalize_text_artifact <"$member_raw" >"$member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$member_out" \
        "run_output"

    local generic_member_raw="$BUILD_DIR/probe_${backend}.generic_member_call.raw"
    local generic_member_out="$BUILD_DIR/probe_${backend}.generic_member_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$generic_member_raw" \
        --generic-member-call-positive
    pgy_selfhost_normalize_text_artifact <"$generic_member_raw" \
        >"$generic_member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" \
        "$generic_member_out" "run_output"

    local chained_member_raw="$BUILD_DIR/probe_${backend}.chained_member_call.raw"
    local chained_member_out="$BUILD_DIR/probe_${backend}.chained_member_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$chained_member_raw" \
        --chained-member-call-positive
    pgy_selfhost_normalize_text_artifact <"$chained_member_raw" \
        >"$chained_member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" \
        "$chained_member_out" "run_output"

    local nominal_call_raw="$BUILD_DIR/probe_${backend}.nominal_call.raw"
    local nominal_call_out="$BUILD_DIR/probe_${backend}.nominal_call.out"
    run_positive_probe_capture \
        "$backend" "$bin" "$nominal_call_raw" \
        --nominal-return-call-positive
    pgy_selfhost_normalize_text_artifact <"$nominal_call_raw" \
        >"$nominal_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$NOMINAL_CALL_EXPECTED" \
        "$nominal_call_out" "run_output"

    local environment_case environment_mode environment_expected environment_raw environment_out
    for environment_case in \
        'outer_read|--environment-outer-read-positive|environment-outer-read=Int' \
        'nested_exit|--environment-nested-exit-positive|environment-nested-exit=Int' \
        'destructure|--environment-destructure-atomic-positive|environment-destructure=atomic'; do
        environment_mode="${environment_case#*|}"
        environment_mode="${environment_mode%%|*}"
        environment_expected="${environment_case##*|}"
        environment_raw="$BUILD_DIR/probe_${backend}.environment_${environment_case%%|*}.raw"
        environment_out="$BUILD_DIR/probe_${backend}.environment_${environment_case%%|*}.out"
        run_positive_probe_capture \
            "$backend" "$bin" "$environment_raw" "$environment_mode"
        pgy_selfhost_normalize_text_artifact <"$environment_raw" >"$environment_out"
        grep -Fxq "$environment_expected" "$environment_out" || {
            echo "[$LABEL] backend=$backend $environment_mode output drifted" >&2
            cat "$environment_out" >&2
            exit 1
        }
    done

    local mode rc diagnostic
    for mode in missing-initializer-row missing-inferred-type \
        unknown-scalar-operand scalar-type-mismatch \
        direct-call-callee-mismatch direct-call-argument-mismatch \
        direct-call-nested-mismatch direct-call-nested-call-mismatch \
        scalar-call-mismatch namespace-call-target-mismatch \
        member-call-target-mismatch nominal-return-call-target-mismatch \
        missing-carried-member-target \
        missing-carried-direct-target \
        missing-carried-generic-member-target \
        missing-carried-chained-member-target \
        environment-self-reference environment-sibling-leak; do
        diagnostic='matching initializer and iteration type facts'
        if [[ "$mode" == "unknown-scalar-operand" ]]; then
            diagnostic='undefined_symbol'
        elif [[ "$mode" == "scalar-type-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "direct-call-callee-mismatch" ]]; then
            diagnostic='let_type_mismatch'
        elif [[ "$mode" == "direct-call-argument-mismatch" ]]; then
            diagnostic='call_arg_type_mismatch'
        elif [[ "$mode" == "direct-call-nested-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "direct-call-nested-call-mismatch" ]]; then
            diagnostic='call_arg_type_mismatch'
        elif [[ "$mode" == "scalar-call-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "namespace-call-target-mismatch" ]]; then
            diagnostic='call_target_unresolved'
        elif [[ "$mode" == "member-call-target-mismatch" ]]; then
            diagnostic='let_type_mismatch'
        elif [[ "$mode" == "nominal-return-call-target-mismatch" ]]; then
            diagnostic='call_target_unresolved'
        elif [[ "$mode" == "missing-carried-direct-target" || \
                "$mode" == "missing-carried-member-target" || \
                "$mode" == "missing-carried-generic-member-target" || \
                "$mode" == "missing-carried-chained-member-target" ]]; then
            diagnostic='MIR producer requires matching semantic artifact facts'
        elif [[ "$mode" == "environment-self-reference" || \
                "$mode" == "environment-sibling-leak" ]]; then
            diagnostic='undefined_symbol'
        fi
        set +e
        (cd "$ROOT_DIR" && "$bin" "--$mode" \
            >"$BUILD_DIR/$backend.$mode.out" \
            2>"$BUILD_DIR/$backend.$mode.err")
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] ||
            { echo "[$LABEL] backend=$backend $mode did not fail closed" >&2; exit 1; }
        grep -Fq "$diagnostic" \
            "$BUILD_DIR/$backend.$mode.out" "$BUILD_DIR/$backend.$mode.err" ||
            { echo "[$LABEL] backend=$backend $mode diagnostic drifted" >&2; exit 1; }
    done
}

compile_probe c
run_probe c

BACKENDS="${PGY_SELFHOST_INITIALIZER_PROJECTION_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    set +e
    compile_probe llvm
    llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -eq 0 ]]; then
        run_probe llvm
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.out" "$BUILD_DIR/probe_llvm.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call.out" \
            "$BUILD_DIR/probe_llvm.direct_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call_nested.out" \
            "$BUILD_DIR/probe_llvm.direct_call_nested.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call_nested_call.out" \
            "$BUILD_DIR/probe_llvm.direct_call_nested_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.scalar_call.out" \
            "$BUILD_DIR/probe_llvm.scalar_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.namespace_call.out" \
            "$BUILD_DIR/probe_llvm.namespace_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.member_call.out" \
            "$BUILD_DIR/probe_llvm.member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.generic_member_call.out" \
            "$BUILD_DIR/probe_llvm.generic_member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.chained_member_call.out" \
            "$BUILD_DIR/probe_llvm.chained_member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.nominal_call.out" \
            "$BUILD_DIR/probe_llvm.nominal_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.environment_outer_read.out" \
            "$BUILD_DIR/probe_llvm.environment_outer_read.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.environment_nested_exit.out" \
            "$BUILD_DIR/probe_llvm.environment_nested_exit.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.environment_destructure.out" \
            "$BUILD_DIR/probe_llvm.environment_destructure.out"
    elif [[ "$llvm_rc" -eq 2 ]]; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        exit "$llvm_rc"
    fi
fi

echo "[$LABEL] semantic initializer projection parity ok"
