#!/usr/bin/env bash
# One Pergyra-owned return-type verdict publishes the same exact JSON receipt
# through public MIR, C, and LLVM requests. Other mismatch codes stay closed.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/public_return_type_mismatch_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
RETURN_REL="src/self_hosted/semantic/fixture/bad_return_type.pgy"
UNADMITTED_REL="src/self_hosted/semantic/fixture/bad_issome_non_option.pgy"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-public-return-type-mismatch-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$RETURN_REL") >"$WORK_DIR/direct-text.out" 2>"$WORK_DIR/direct-text.err"
direct_text_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$RETURN_REL") >"$WORK_DIR/direct-json.out" 2>"$WORK_DIR/direct-json.err"
direct_json_rc=$?
set -e
[[ "$direct_text_rc" -ne 0 ]] || fail "direct text verdict unexpectedly succeeded"
for fact in 'Code: return_type_mismatch' 'expected: Float' 'actual: Double'; do
    grep -Fq -- "$fact" "$WORK_DIR/direct-text.out" \
        "$WORK_DIR/direct-text.err" || fail "direct text verdict lost: $fact"
done
[[ "$direct_json_rc" -ne 0 && ! -s "$WORK_DIR/direct-json.err" ]] ||
    fail "direct JSON verdict changed its private channel contract"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/direct-json.out" ||
    fail "direct JSON verdict lost its wire marker"
tail -n +2 "$WORK_DIR/direct-json.out" >"$WORK_DIR/expected.json"
[[ -s "$WORK_DIR/expected.json" ]] || fail "direct JSON receipt is empty"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_TYPE_MISMATCH"' \
    '"cause_ir":"semantic:assignability_check"' \
    '"fix_source":"annotate-or-convert"'; do
    require_text "$WORK_DIR/expected.json" "$fact"
done

run_public_failure() {
    local label="$1" artifact="$2"
    shift 2
    [[ -z "$artifact" ]] || rm -f "$ROOT_DIR/$artifact"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$@") >"$WORK_DIR/$label.out" 2>"$WORK_DIR/$label.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$label.out" ]] ||
        fail "$label did not fail on stderr only"
    cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/$label.err" ||
        fail "$label did not relay the exact Pergyra-owned receipt"
    [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "$label published an artifact"
    ! grep -Fq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/$label.err" ||
        fail "$label leaked the private wire marker"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.err" ||
        fail "$label retried the native pipeline"
}

run_public_failure mir "" --mir --error-format=json "$RETURN_REL"
run_public_failure c "$WORK_REL/invalid-c.bin" \
    --error-format=json --backend=c "$RETURN_REL" \
    -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm "$WORK_REL/invalid-llvm.bin" \
    --error-format=json --backend=llvm "$RETURN_REL" \
    -o "$WORK_REL/invalid-llvm.bin"

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$UNADMITTED_REL") >"$WORK_DIR/unadmitted.out" \
    2>"$WORK_DIR/unadmitted.err"
unadmitted_rc=$?
set -e
[[ "$unadmitted_rc" -ne 0 && ! -s "$WORK_DIR/unadmitted.err" ]] ||
    fail "unrelated semantic mismatch changed its failure channels"
! grep -q '[^[:space:]]' "$WORK_DIR/unadmitted.out" ||
    fail "unrelated semantic mismatch gained a public identity"

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed return-type diagnostic identity"

require_text "$DIAGNOSTIC_OWNER" 'if code == "return_type_mismatch" {'
require_text "$DIAGNOSTIC_OWNER" '"semantic:assignability_check"'
require_text "$DIAGNOSTIC_OWNER" '"annotate-or-convert"'
require_text "$CONTRACT_OWNER" 'return_first.message != return_second.message'
! grep -Eq 'PGY_SEM_TYPE_MISMATCH|semantic:assignability_check|annotate-or-convert' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic return-type authority"

echo "[self-host-public-return-type-mismatch-json-diagnostic] exact semantic identity, MIR/C/LLVM relay, and unrelated-code exclusion: PASS"
