#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:generic-specialization-identity-epoch"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC_BIN="${CC:-cc}"
pgy_require_runnable_binary_here "$LABEL:driver" "$DRIVER" \
    || fail "self driver is not runnable"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

SOURCE="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/generic_specialization_identity_epoch}"
mkdir -p "$WORK_DIR"
BASE="$WORK_DIR/base.mir.json"
BAD="$WORK_DIR/bad-ordinal.mir.json"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE") \
    | tr -d '\r' >"$BASE"
grep -Fq '"callable":"Identity"' "$BASE" \
    || fail "mixed intent/generic specialization row is missing"
BASE_ARG="$(pgy_selfhost_path_relative_to_root "$BASE")"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$BASE_ARG") >"$WORK_DIR/program.c"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$WORK_DIR/program.c" -o "$WORK_DIR/program.exe"
"$WORK_DIR/program.exe" | tr -d '\r' >"$WORK_DIR/program.run"
printf '%s\n' 'accepted=true' 'calls=1' 'rejected=false' 'calls=2' \
    >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/program.run" \
    || fail "mixed intent/generic raw MIR execution drifted"

pgy_replace_first_literal "$BASE" "$BAD" \
    '"source_call_ordinal":0' '"source_call_ordinal":999'
BAD_ARG="$(pgy_selfhost_path_relative_to_root "$BAD")"
set +e
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$BAD_ARG") \
    >"$WORK_DIR/bad.out" 2>"$WORK_DIR/bad.err"
bad_rc=$?
set -e
[[ "$bad_rc" -ne 0 ]] || fail "invalid generic call ordinal was accepted"
grep -Fq 'generic specialization identity is unknown' \
    "$WORK_DIR/bad.out" "$WORK_DIR/bad.err" \
    || fail "invalid generic call ordinal diagnostic drifted"
if grep -Fq '#include' "$WORK_DIR/bad.out" "$WORK_DIR/bad.err"; then
    fail "invalid generic call ordinal emitted a partial C artifact"
fi

echo "[$LABEL] PASS"
