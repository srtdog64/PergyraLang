#!/usr/bin/env bash
# Rung 2 parity for the production C size checker (2026-05-27).
# Sister parity for tool 8. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - count parity vs shell `wc -l` ground truth
#   - synthetic over-cap fixture (701-line .c): rc=1, c_over_cap finding
# See self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
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
        echo "[self-host-parity:production-c-size] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:production-c-size] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/self_hosted/tools/production_c_size_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/production_c_size_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/self_hosted/tools/production_c_size_checker/expected/clean.json"
MANIFEST_FILE="$ROOT_DIR/self_hosted/tools/production_c_size_checker/fixture/c_files_manifest.txt"
MANIFEST_REL="self_hosted/tools/production_c_size_checker/fixture/c_files_manifest.txt"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$MANIFEST_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:production-c-size] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:production-c-size] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.production-c-size.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-c-size] schema header missing" >&2
    exit 1
fi

# Shell ground truth.
SHELL_C="$(wc -l < "$MANIFEST_FILE" | tr -d ' ')"
SHELL_VIOLATIONS=0
SHELL_MAX=0
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    if [[ ! -f "$ROOT_DIR/$path" ]]; then
        continue
    fi
    n="$(wc -l < "$ROOT_DIR/$path" | tr -d ' ')"
    if [[ "$n" -gt "$SHELL_MAX" ]]; then
        SHELL_MAX="$n"
    fi
    if [[ "$n" -gt 600 ]]; then
        SHELL_VIOLATIONS=$((SHELL_VIOLATIONS + 1))
    fi
done < "$MANIFEST_FILE"

if ! grep -Fq "\"c_files\":${SHELL_C}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-c-size] c_files parity FAIL (shell=${SHELL_C})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"violations\":${SHELL_VIOLATIONS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-c-size] violations parity FAIL (shell=${SHELL_VIOLATIONS})" >&2
    exit 1
fi
if ! grep -Fq "\"max_lines\":${SHELL_MAX}" <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-c-size] max_lines parity FAIL (shell=${SHELL_MAX})" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.production-c-size.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:production-c-size] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Synthetic over-cap fixture - 701-line .c appended to a tmp manifest.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-pcs.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/runtime"
{
    for k in $(seq 1 701); do
        echo "/* synthetic line $k */"
    done
} > "$NEG_ROOT/src/runtime/pgy_runtime_synthetic_c_drift.c"
mkdir -p "$NEG_ROOT/$(dirname "$MANIFEST_REL")"
{
    cat "$MANIFEST_FILE"
    echo "src/runtime/pgy_runtime_synthetic_c_drift.c"
} > "$NEG_ROOT/$MANIFEST_REL"
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    [[ -f "$ROOT_DIR/$path" ]] || continue
    mkdir -p "$NEG_ROOT/$(dirname "$path")"
    cp "$ROOT_DIR/$path" "$NEG_ROOT/$path"
done < "$MANIFEST_FILE"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:production-c-size] over-cap fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"c_over_cap"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:production-c-size] over-cap fixture expected c_over_cap finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy_runtime_synthetic_c_drift.c' <<<"$NEG_OUT"; then
    echo "[self-host-parity:production-c-size] over-cap fixture expected synthetic path in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

echo "[self-host-parity:production-c-size] rung-2 parity ok (c_files=$SHELL_C violations=$SHELL_VIOLATIONS max=$SHELL_MAX; over-cap fixture rc=1)"
