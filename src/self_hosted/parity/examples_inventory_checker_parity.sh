#!/usr/bin/env bash
# Rung 2 parity for the examples inventory checker.
# Third consumer of TextScan.CountLines; triggered the lift to lib.
# Asserts: clean exit, JSON byte-equal, DirWalk-owned count drift fixture rc=1.
# See src/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/src/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:examples-inventory] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:examples-inventory] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/examples_inventory_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/examples_inventory_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/examples_inventory_checker/expected/clean.json"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:examples-inventory] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

# Mirror src/self_hosted/lib/ -> .tmp/lib/ for the tool's
# `import "../../lib/..."` resolution.
LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:examples-inventory] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.examples-inventory.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] schema header missing" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.examples-inventory.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:examples-inventory] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Synthetic count-drift fixture - mirror top-level examples/*.pgy and omit one.
# DirWalk remains the inventory owner; the expected_examples contract detects
# the drift without a separate manifest source of truth.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-exi.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/examples"
mkdir -p "$NEG_ROOT/.tmp"

DROP_TARGET=""
while IFS= read -r source; do
    rel="examples/$(basename "$source")"
    if [[ -z "$DROP_TARGET" ]]; then
        DROP_TARGET="$rel"
        continue
    fi
    cp "$source" "$NEG_ROOT/$rel"
done < <(find "$ROOT_DIR/examples" -maxdepth 1 -type f -name '*.pgy' | sort)

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:examples-inventory] count-drift fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"inventory_count_drift"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:examples-inventory] count-drift fixture expected inventory_count_drift finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:examples-inventory" "$PERGYRA_TOOL" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:examples-inventory] rung-2 parity ok (DirWalk examples=121 missing=0 empty=0 max=544; count-drift fixture rc=1)"
