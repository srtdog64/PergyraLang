#!/usr/bin/env bash
# Rung 2 parity for the examples inventory checker (2026-05-27).
# Third consumer of TextScan.CountLines; triggered the lift to lib.
# Asserts: clean exit, JSON byte-equal, count parity vs shell, missing-
# example fixture rc=1.
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
MANIFEST_FILE="$ROOT_DIR/src/self_hosted/tools/examples_inventory_checker/fixture/examples_manifest.txt"
MANIFEST_REL="src/self_hosted/tools/examples_inventory_checker/fixture/examples_manifest.txt"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$MANIFEST_FILE"; do
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

# Shell ground truth.
SHELL_EXAMPLES="$(wc -l < "$MANIFEST_FILE" | tr -d ' ')"
SHELL_MISSING=0
SHELL_EMPTY=0
SHELL_MAX=0
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    if [[ ! -f "$ROOT_DIR/$path" ]]; then
        SHELL_MISSING=$((SHELL_MISSING + 1))
        continue
    fi
    n="$(wc -l < "$ROOT_DIR/$path" | tr -d ' ')"
    if [[ "$n" -gt "$SHELL_MAX" ]]; then
        SHELL_MAX="$n"
    fi
    if [[ "$n" -eq 0 ]]; then
        SHELL_EMPTY=$((SHELL_EMPTY + 1))
    fi
done < "$MANIFEST_FILE"

if ! grep -Fq "\"examples\":${SHELL_EXAMPLES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] examples parity FAIL (shell=${SHELL_EXAMPLES})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"missing\":${SHELL_MISSING}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] missing parity FAIL (shell=${SHELL_MISSING})" >&2
    exit 1
fi
if ! grep -Fq "\"empty\":${SHELL_EMPTY}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] empty parity FAIL (shell=${SHELL_EMPTY})" >&2
    exit 1
fi
if ! grep -Fq "\"max_lines\":${SHELL_MAX}" <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] max_lines parity FAIL (shell=${SHELL_MAX})" >&2
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

# Synthetic missing-example fixture - mirror examples + omit one,
# manifest still references it so the tool detects the missing entry.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-exi.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/examples"
mkdir -p "$NEG_ROOT/.tmp"
mkdir -p "$NEG_ROOT/$(dirname "$MANIFEST_REL")"
# Pick the first manifest entry to omit.
DROP_TARGET="$(head -n 1 "$MANIFEST_FILE")"
{
    cat "$MANIFEST_FILE"
} > "$NEG_ROOT/$MANIFEST_REL"
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    if [[ "$path" == "$DROP_TARGET" ]]; then
        continue
    fi
    [[ -f "$ROOT_DIR/$path" ]] || continue
    mkdir -p "$NEG_ROOT/$(dirname "$path")"
    cp "$ROOT_DIR/$path" "$NEG_ROOT/$path"
done < "$MANIFEST_FILE"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:examples-inventory] missing-example fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"missing_example"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:examples-inventory] missing-example fixture expected missing_example finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

echo "[self-host-parity:examples-inventory] rung-2 parity ok (examples=$SHELL_EXAMPLES missing=0 empty=0 max=$SHELL_MAX; missing-example fixture rc=1)"
