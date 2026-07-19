#!/usr/bin/env bash
# Pergyra-owned gate manifest/result validation and C/LLVM projection parity.

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

LABEL="self-host-gate-dashboard"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/gate_dashboard}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/expected_pass.json"
PASS_RESULTS="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/results_pass.tsv"
UNKNOWN_RESULTS="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/results_unknown.tsv"
DUPLICATE_RESULTS="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/results_duplicate.tsv"
OVER_BUDGET_RESULTS="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/results_over_budget.tsv"
PASS_RESULTS_REL="src/self_hosted/tools/gate_dashboard/results_pass.tsv"
UNKNOWN_RESULTS_REL="src/self_hosted/tools/gate_dashboard/results_unknown.tsv"
DUPLICATE_RESULTS_REL="src/self_hosted/tools/gate_dashboard/results_duplicate.tsv"
OVER_BUDGET_RESULTS_REL="src/self_hosted/tools/gate_dashboard/results_over_budget.tsv"
RUNNER="$ROOT_DIR/tests/self_hosted/parity/gate_dashboard_runner.sh"
mkdir -p "$BUILD_DIR"
for input in "$SOURCE" "$EXPECTED" "$PASS_RESULTS" "$UNKNOWN_RESULTS" \
    "$DUPLICATE_RESULTS" "$OVER_BUDGET_RESULTS" "$RUNNER"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done
grep -Fq 'source "$ROOT_DIR/tests/portable_process_helpers.sh"' "$RUNNER" ||
    { echo "[$LABEL] runner lost portable timeout owner" >&2; exit 1; }
grep -Fq 'pgy_run_with_timeout' "$RUNNER" ||
    { echo "[$LABEL] runner no longer enforces gate budgets" >&2; exit 1; }
grep -Fq '"$budget" "$stdout_log" "$stderr_log"' "$RUNNER" ||
    { echo "[$LABEL] runner does not consume manifest budget" >&2; exit 1; }

compile_dashboard() {
    local backend="$1"
    local bin="$BUILD_DIR/gate_dashboard_${backend}.exe"
    local log="$BUILD_DIR/gate_dashboard_${backend}.compile.log"
    rm -f "$bin" "$BUILD_DIR/gate_dashboard_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_dashboard() {
    local backend="$1"
    local bin="$BUILD_DIR/gate_dashboard_${backend}.exe"
    local raw="$BUILD_DIR/gate_dashboard_${backend}.raw"
    local out="$BUILD_DIR/gate_dashboard_${backend}.out"
    local manifest="$BUILD_DIR/gate_dashboard_${backend}.manifest"

    (cd "$ROOT_DIR" && "$bin" --results \
        "$PASS_RESULTS_REL" >"$raw")
    pgy_selfhost_normalize_text_artifact <"$raw" >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"

    (cd "$ROOT_DIR" && "$bin" --manifest | tr -d '\r' >"$manifest")
    local row_count=0
    while IFS= read -r line; do
        [[ -n "$line" ]] || continue
        if [[ "$line" == schema=* ]]; then
            [[ "$line" == "schema=pgy.selfhost.gate-dashboard.v1" ]] ||
                { echo "[$LABEL] manifest schema drifted" >&2; exit 1; }
            continue
        fi
        IFS=$'\t' read -r gate_id make_target tier budget state blocking owner extra <<<"$line"
        [[ -z "${extra:-}" && -n "$owner" ]] ||
            { echo "[$LABEL] malformed manifest row: $line" >&2; exit 1; }
        grep -Fq "$make_target:" "$ROOT_DIR/Makefile" ||
            { echo "[$LABEL] missing Make target: $make_target" >&2; exit 1; }
        row_count=$((row_count + 1))
    done <"$manifest"
    [[ "$row_count" -gt 0 ]] ||
        { echo "[$LABEL] canonical manifest emitted no gate rows" >&2; exit 1; }

    local fixture fixture_rel diagnostic rc
    for fixture in "$UNKNOWN_RESULTS" "$DUPLICATE_RESULTS"; do
        diagnostic="gate_result_unknown_id"
        fixture_rel="$UNKNOWN_RESULTS_REL"
        if [[ "$fixture" == "$DUPLICATE_RESULTS" ]]; then
            diagnostic="gate_result_duplicate_id"
            fixture_rel="$DUPLICATE_RESULTS_REL"
        fi
        set +e
        (cd "$ROOT_DIR" && "$bin" --results \
            "$fixture_rel" \
            >"$BUILD_DIR/negative.out" 2>"$BUILD_DIR/negative.err")
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] ||
            { echo "[$LABEL] malformed result did not fail closed" >&2; exit 1; }
        grep -Fq "$diagnostic" "$BUILD_DIR/negative.out" "$BUILD_DIR/negative.err" ||
            { echo "[$LABEL] malformed result diagnostic drifted" >&2; exit 1; }
    done

    set +e
    (cd "$ROOT_DIR" && "$bin" --results \
        "$OVER_BUDGET_RESULTS_REL" \
        >"$BUILD_DIR/over_budget.out" 2>"$BUILD_DIR/over_budget.err")
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] ||
        { echo "[$LABEL] over-budget result did not fail" >&2; exit 1; }
    grep -Fq '"over_budget":true' \
        "$BUILD_DIR/over_budget.out" "$BUILD_DIR/over_budget.err" ||
        { echo "[$LABEL] over-budget result was not projected" >&2; exit 1; }
}

compile_dashboard c
run_dashboard c

BACKENDS="${PGY_SELFHOST_GATE_DASHBOARD_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    set +e
    compile_dashboard llvm
    llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -eq 0 ]]; then
        run_dashboard llvm
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/gate_dashboard_c.out" \
            "$BUILD_DIR/gate_dashboard_llvm.out"
    elif [[ "$llvm_rc" -eq 2 ]]; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        exit "$llvm_rc"
    fi
fi

echo "[$LABEL] Pergyra-owned manifest, golden, and negative results ok"
