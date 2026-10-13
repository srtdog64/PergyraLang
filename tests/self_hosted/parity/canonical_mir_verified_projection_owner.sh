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

SOURCE="src/self_hosted/mir_lower/fixture/let_log.pgy"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/canonical_mir_verified_projection}"

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
