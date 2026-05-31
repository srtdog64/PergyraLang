#!/usr/bin/env bash
# Rung 2 parity for the stdlib dispatch inventory checker (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy).
# Shell grep is the parity backend. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - count parity vs shell grep on both dispatch tables
#   - synthetic drift fixture (delete one LLVM entry): rc=1, count_drift
#     finding
# See src/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:stdlib-dispatch-inventory] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:stdlib-dispatch-inventory] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/stdlib_dispatch_inventory_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/stdlib_dispatch_inventory_checker/expected/clean.json"
C_DISPATCH="src/codegen/transpiler_expr_stdlib_scalar_builtin.c"
LLVM_DISPATCH="src/codegen/llvm_expr_stdlib_scalar_io_calls.c"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$ROOT_DIR/$C_DISPATCH" "$ROOT_DIR/$LLVM_DISPATCH"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:stdlib-dispatch-inventory] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

# Mirror src/self_hosted/lib/ -> .tmp/lib/ so the tool's
# `import "../../lib/..."` resolves under the copied source.
LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.stdlib-dispatch-inventory.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] schema header missing" >&2
    exit 1
fi

# Shell drift detector. C dispatch is split across two tables
# (TRANSPILER_SCALAR_OP_ main family + TranspilerScalarUnarySpec math
# family), so sum both anchor counts to match the Pergyra tool.
SHELL_C_MAIN="$(grep -c ', TRANSPILER_SCALAR_OP_' "$ROOT_DIR/$C_DISPATCH" || true)"
SHELL_C_MATH="$(grep -cE '^        \{ \"' "$ROOT_DIR/$C_DISPATCH" || true)"
SHELL_C=$((SHELL_C_MAIN + SHELL_C_MATH))
SHELL_LLVM="$(grep -c '"stdlib ' "$ROOT_DIR/$LLVM_DISPATCH" || true)"
if [[ -z "$SHELL_C_MAIN" || -z "$SHELL_LLVM" ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] shell ground truth empty" >&2
    exit 1
fi
# Tolerate small drift (post-beta cleanup). Match Pergyra tool's
# drift_tolerance band so shell and tool agree on the baseline.
SHELL_DRIFT_TOLERANCE=5
SHELL_RAW_DRIFT=$(( SHELL_C - SHELL_LLVM ))
if [[ "$SHELL_RAW_DRIFT" -lt 0 ]]; then
    SHELL_RAW_DRIFT=$(( -SHELL_RAW_DRIFT ))
fi
if [[ "$SHELL_RAW_DRIFT" -gt "$SHELL_DRIFT_TOLERANCE" ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] shell drift exceeds tolerance (c=$SHELL_C llvm=$SHELL_LLVM diff=$SHELL_RAW_DRIFT tol=$SHELL_DRIFT_TOLERANCE)" >&2
    exit 1
fi

if ! grep -Fq "\"c_entries\":${SHELL_C}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] c_entries parity FAIL (shell=${SHELL_C})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"llvm_entries\":${SHELL_LLVM}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] llvm_entries parity FAIL (shell=${SHELL_LLVM})" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.stdlib-dispatch-inventory.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Synthetic drift fixture - strip multiple LLVM entries to exceed the
# tolerance band, expect rc=1.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-sdi.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/codegen"
cp "$ROOT_DIR/$C_DISPATCH" "$NEG_ROOT/$C_DISPATCH"
# Delete enough `"stdlib ` lines in LLVM dispatch to push raw drift above
# the tool's tolerance band (currently 5). Strip 12 to be safely past.
awk 'BEGIN{stripped=0} /"stdlib /{ if(stripped<12){stripped++; next} } {print}' \
    "$ROOT_DIR/$LLVM_DISPATCH" > "$NEG_ROOT/$LLVM_DISPATCH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] drift fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"count_drift"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] drift fixture expected count_drift finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

echo "[self-host-parity:stdlib-dispatch-inventory] rung-2 parity ok (c=$SHELL_C llvm=$SHELL_LLVM; drift-fixture rc=1)"
