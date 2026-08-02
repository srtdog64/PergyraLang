#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:canonical-mir-verified-projection"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
pgy_require_runnable_binary_here "$LABEL:pgy" "$PGY" || fail "pgy is not runnable"
pgy_require_runnable_binary_here "$LABEL:driver" "$DRIVER" || fail "self driver is not runnable"

DRIVER_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
CANONICAL_OWNER="$ROOT_DIR/src/self_hosted/compiler/canonical_mir_execution_owner.pgy"
BODY_CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_contract_owner.pgy"
SOURCE="src/self_hosted/mir_lower/fixture/let_log.pgy"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/canonical_mir_verified_projection}"

function_body() {
    local owner="$1" name="$2"
    sed -n "/^func ${name}(/,/^}/p" "$owner"
}

verified_projection="$(function_body "$DRIVER_OWNER" DriverRung2MirProjectionFromVerifiedFactsObserved)"
canonical_projection="$(function_body "$CANONICAL_OWNER" CanonicalizeMirArtifactWithAdmittedTopology)"
grep -Fq 'SemanticAstBodyTypeBundleAdmissionReceiptReadyFor(' <<<"$verified_projection" \
    || fail "verified projection does not validate its receipt"
grep -Fq 'SelfMirProgramFactsFromReadyArtifactObserved(' <<<"$verified_projection" \
    || fail "verified projection does not consume ready body facts"
if grep -Fq 'VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved(' <<<"$verified_projection"; then
    fail "verified projection reruns semantic verification"
fi
[[ "$(grep -Fc 'DriverRung2MirProjectionFromVerifiedFactsObserved(' <<<"$canonical_projection")" -eq 1 ]] \
    || fail "canonical projection must consume exactly one verified receipt"
if grep -Eq 'DriverRung2MirProjectionFromAdmittedAnalysisObserved\(|VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved\(' \
    <<<"$canonical_projection"; then
    fail "canonical projection retained a semantic re-verification path"
fi
grep -Fq 'call_return_type_names[' "$BODY_CONTRACT_OWNER" \
    || fail "stale call-return row negative contract disappeared"
grep -Fq '] = "stale";' "$BODY_CONTRACT_OWNER" \
    || fail "stale call-return row falsifier disappeared"

mkdir -p "$WORK_DIR"
MIR="$WORK_DIR/let-log.mir.json"
CANONICAL="$WORK_DIR/let-log.canonical.mir.json"
CANONICAL_AGAIN="$WORK_DIR/let-log.canonical-again.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE") \
    | tr -d '\r' >"$MIR"
MIR_ARG="$(pgy_selfhost_path_relative_to_root "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --self-driver --canonicalize-mir-json "$MIR_ARG") \
    | tr -d '\r' >"$CANONICAL"
cmp -s "$MIR" "$CANONICAL" || fail "launcher canonicalization changed verified let-log MIR"
CANONICAL_ARG="$(pgy_selfhost_path_relative_to_root "$CANONICAL")"
(cd "$ROOT_DIR" && "$DRIVER" --canonicalize-mir-json "$CANONICAL_ARG") \
    | tr -d '\r' >"$CANONICAL_AGAIN"
cmp -s "$CANONICAL" "$CANONICAL_AGAIN" \
    || fail "repeated canonicalization is not a fixpoint"

echo "[$LABEL] PASS"
