#!/usr/bin/env bash
# Statement result types and expression place kinds come only from their
# semantic fact owners. The probe runs each owner's contract; a contract fails
# when a missing or unverified fact is answered instead of refused (a condition
# that is not Bool, an Exit argument of the wrong type, a drifted ArraySet
# argument graph, an unverified or wrong-kind statement row, a place kind
# forced to Unknown). The contracts are backend-neutral semantic checks, so the
# probe is built once, on the C backend.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:statement-type-place"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
PROBE="$ROOT_DIR/tests/self_hosted/fixtures/statement_type_place_contract_ready.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/statement_type_place_contract}"

pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
[[ -f "$PROBE" ]] || { echo "[$LABEL] probe is missing" >&2; exit 1; }

mkdir -p "$BUILD_DIR"
PROBE_ARG="$(pgy_path_for_compiler "$PGY" "$PROBE")"
EXE="$BUILD_DIR/probe.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_ARG" --backend=c -o "$(pgy_path_for_compiler "$PGY" "$EXE")") \
    >"$BUILD_DIR/compile.log" 2>&1 || {
    echo "[$LABEL] probe compile failed" >&2
    tail -20 "$BUILD_DIR/compile.log" >&2
    exit 1
}
set +e
(cd "$ROOT_DIR" && "$EXE") 2>"$BUILD_DIR/run.err" | tr -d '\r' >"$BUILD_DIR/run.out"
rc=${PIPESTATUS[0]}
set -e
[[ "$rc" -eq 0 ]] || {
    echo "[$LABEL] contract $rc failed (1 statement types, 2 statement query, 3 leaf place, 4 body bundle place)" >&2
    exit 1
}
grep -Fxq 'statement-type-place-ready' "$BUILD_DIR/run.out" || {
    echo "[$LABEL] probe printed an unexpected result" >&2
    cat "$BUILD_DIR/run.out" >&2
    exit 1
}

echo "[$LABEL] statement type and place contracts refuse every missing fact"
