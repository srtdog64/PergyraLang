#!/usr/bin/env bash
# Rung 2 parity for the runtime boundary checker (2026-06-16).
#
# Pergyra is the origin (src/self_hosted/tools/runtime_boundary_checker/main.pgy).
# Shell grep over the same required terms is the parity backend.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:runtime-boundary] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:runtime-boundary] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/runtime_boundary_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/runtime_boundary_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/runtime_boundary_checker/expected/clean.json"

required_terms=(
    "src/self_hosted/runtime/README.md|The native runtime kernel remains C"
    "src/self_hosted/runtime/README.md|Portable runtime policy can move to Pergyra"
    "src/self_hosted/runtime/README.md|full runtime replacement"
    "src/self_hosted/PROGRESS.md|native runtime kernel stays C"
    "src/self_hosted/PROGRESS.md|Runtime-adjacent Pergyra tools count as soft self-host evidence"
    "src/self_hosted/README.md|runtime/                        -- native runtime kernel stays C"
    "docs/self_hosted/05_compiler_core_gap_analysis.md|runtime in one jump"
    "docs/self_hosted/05_compiler_core_gap_analysis.md|runtime replacement"
)

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:runtime-boundary] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_JSON_FILE" ]]; then
    echo "[self-host-parity:runtime-boundary] missing expected JSON: $EXPECTED_JSON_FILE" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

missing=0
for pair in "${required_terms[@]}"; do
    rel="${pair%%|*}"
    term="${pair#*|}"
    if [[ ! -f "$ROOT_DIR/$rel" ]]; then
        echo "[self-host-parity:runtime-boundary] missing input: $rel" >&2
        exit 1
    fi
    if ! grep -Fq "$term" "$ROOT_DIR/$rel"; then
        echo "[self-host-parity:runtime-boundary] shell missing term in $rel: $term" >&2
        missing=$((missing + 1))
    fi
done
if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null | tr -d '\r')"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:runtime-boundary] clean repo exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.runtime-boundary.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:runtime-boundary] schema header missing" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.runtime-boundary.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:runtime-boundary] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-runtime-boundary.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT

mkdir -p "$NEG_ROOT/src/self_hosted/runtime" "$NEG_ROOT/src/self_hosted" "$NEG_ROOT/docs/self_hosted"
cp "$ROOT_DIR/src/self_hosted/PROGRESS.md" "$NEG_ROOT/src/self_hosted/PROGRESS.md"
cp "$ROOT_DIR/src/self_hosted/README.md" "$NEG_ROOT/src/self_hosted/README.md"
cp "$ROOT_DIR/docs/self_hosted/05_compiler_core_gap_analysis.md" "$NEG_ROOT/docs/self_hosted/05_compiler_core_gap_analysis.md"
grep -v 'Portable runtime policy can move to Pergyra' \
    "$ROOT_DIR/src/self_hosted/runtime/README.md" \
    > "$NEG_ROOT/src/self_hosted/runtime/README.md"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1 | tr -d '\r')"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"missing":1' <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected missing:1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq 'Portable runtime policy can move to Pergyra' <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected stripped term in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:runtime-boundary" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:runtime-boundary] rung-2 parity ok (required=${#required_terms[@]} missing-fixture rc=1)"
