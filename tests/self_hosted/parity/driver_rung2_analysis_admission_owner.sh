#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:driver-rung2-analysis-admission"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" \
    || fail "PGY_BIN is not runnable"

OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
CANONICAL_OWNER="$ROOT_DIR/src/self_hosted/compiler/canonical_mir_execution_owner.pgy"
PROBE_SRC="$ROOT_DIR/tests/self_hosted/fixtures/driver_rung2_mutable_analysis_reject.pgy"
DIRECT_PROBE="$ROOT_DIR/src/self_hosted/tools/mir_json_instruction_writer_probe/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/driver_rung2_analysis_admission"
PROBE_BIN="$BUILD_DIR/mutable-analysis-reject.exe"
PROBE_SRC_ARG="$(pgy_selfhost_path_relative_to_root "$PROBE_SRC")"
PROBE_BIN_ARG="$(pgy_selfhost_path_relative_to_root "$PROBE_BIN")"

function_body() {
    local owner="$1" name="$2"
    sed -n "/^func ${name}(/,/^}/p" "$owner"
}

require_in_function() {
    local owner="$1" function_name="$2" term="$3"
    function_body "$owner" "$function_name" | grep -Fq -- "$term" \
        || fail "$function_name is missing: $term"
}

reject_in_function() {
    local owner="$1" function_name="$2" term="$3"
    if function_body "$owner" "$function_name" | grep -Fq -- "$term"; then
        fail "$function_name retained forbidden term: $term"
    fi
}

require_in_function "$OWNER" VerifyArtifactAnalysisForDriverRung2Observed \
    'SemanticAstArtifactAnalysisMatches('
require_in_function "$OWNER" VerifyArtifactAnalysisForDriverRung2Observed \
    'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('
require_in_function "$OWNER" VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved \
    'SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved('
reject_in_function "$OWNER" VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved \
    'SemanticAstArtifactAnalysisMatches('

require_in_function "$OWNER" DriverRung2MirProjectionFromAnalysisObserved \
    'SemanticAstArtifactAnalysisMatches('
require_in_function "$OWNER" DriverRung2MirProjectionFromAnalysisObserved \
    'DriverRung2MirProjectionFromAdmittedAnalysisObserved('
require_in_function "$OWNER" DriverRung2MirProjectionFromAdmittedAnalysisObserved \
    'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('
reject_in_function "$OWNER" DriverRung2MirProjectionFromAdmittedAnalysisObserved \
    'VerifyArtifactAnalysisForDriverRung2Observed('
require_in_function "$OWNER" DriverRung2MirProjectionFromVerifiedFactsObserved 'SemanticAstBodyTypeBundleAdmissionReceiptReadyFor('
require_in_function "$OWNER" DriverRung2MirProjectionFromVerifiedFactsObserved 'SelfMirProgramFactsFromReadyArtifactObserved('
reject_in_function "$OWNER" DriverRung2MirProjectionFromVerifiedFactsObserved 'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('

for raw_consumer in \
    CompileArtifactAnalysisToMirJsonVerifiedObserved; do
    require_in_function "$OWNER" "$raw_consumer" \
        'DriverRung2MirProjectionFromAnalysisObserved('
    reject_in_function "$OWNER" "$raw_consumer" \
        'DriverRung2MirProjectionFromAdmittedAnalysisObserved('
done
reject_in_function "$OWNER" CompileArtifactAnalysisToMirJsonFileVerifiedObserved CompileArtifactAnalysisToMirJsonFileVerifiedObserved

for fresh_consumer in \
    CompileArtifactToMirJsonVerified \
    CompileSourceToMirJsonPressureObserved; do
    require_in_function "$OWNER" "$fresh_consumer" 'SemanticAstArtifactAnalyzeTyped('
    require_in_function "$OWNER" "$fresh_consumer" \
        'DriverRung2MirProjectionFromAdmittedAnalysisObserved('
    reject_in_function "$OWNER" "$fresh_consumer" \
        'DriverRung2MirProjectionFromAnalysisObserved('
done
require_in_function "$OWNER" VerifyArtifactForMirProduction \
    'SemanticAstArtifactAnalyzeTyped('
require_in_function "$OWNER" VerifyArtifactForMirProduction \
    'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('
require_in_function "$CANONICAL_OWNER" CanonicalizeMirJsonVerified \
    'SemanticAstArtifactAnalyzeWithExpressionGraph('
require_in_function "$CANONICAL_OWNER" CanonicalizeMirJsonVerified \
    'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('
require_in_function "$CANONICAL_OWNER" CanonicalizeMirArtifactWithAdmittedTopology 'DriverRung2MirProjectionFromVerifiedFactsObserved('
reject_in_function "$CANONICAL_OWNER" CanonicalizeMirArtifactWithAdmittedTopology 'DriverRung2MirProjectionFromAdmittedAnalysisObserved('
reject_in_function "$CANONICAL_OWNER" CanonicalizeMirArtifactWithAdmittedTopology 'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('
require_in_function "$OWNER" CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved \
    'SemanticAstArtifactAnalyzeWithExpressionGraph('
require_in_function "$OWNER" CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved \
    'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved('

grep -Fq -- 'DriverRung2MirProjectionFromAnalysisObserved(' "$DIRECT_PROBE" \
    || fail "external analysis probe no longer uses the checked raw boundary"
[[ "$(wc -l < "$OWNER")" -le 500 ]] \
    || fail "driver_rung2_owner.pgy exceeds its 500-line cap"

mkdir -p "$BUILD_DIR"
(cd "$ROOT_DIR" && "$PGY" \
    "$PROBE_SRC_ARG" --backend=c -o "$PROBE_BIN_ARG" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { cat "$BUILD_DIR/compile.log" >&2; fail "mutable analysis probe build failed"; }
PROBE_BIN="$(pgy_select_optional_exe_binary "$PROBE_BIN")"
pgy_require_runnable_binary_here "$LABEL:probe" "$PROBE_BIN" \
    || fail "mutable analysis probe is not runnable"

set +e
(cd "$ROOT_DIR" && "$PROBE_BIN" \
    >"$BUILD_DIR/run.out" 2>"$BUILD_DIR/run.err")
status=$?
set -e
[[ "$status" -ne 0 ]] || fail "raw driver accepted same-shape mutable analysis"
grep -Fq -- 'driver rung-2 semantic analysis deep proof failed' \
    "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" \
    || { cat "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" >&2; fail "raw boundary diagnostic is missing"; }
if grep -Fq -- '[driver-pressure-stage] body-types:start' \
    "$BUILD_DIR/run.out" "$BUILD_DIR/run.err"; then
    fail "raw boundary reached body materialization before rejecting analysis"
fi

echo "[$LABEL] PASS"
