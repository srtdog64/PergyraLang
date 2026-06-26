#!/usr/bin/env bash
# Rung 2 parity for the stable subset section checker (2026-05-27).
#
# Pergyra is the origin (src/self_hosted/tools/stable_subset_section_checker/main.pgy).
# Shell grep + canonical anchor list is the parity backend (drift detector).
# This script asserts exit-code parity, JSON shape parity against
# expected/clean.json, and a synthetic missing-section fixture.
# See tests/self_hosted/parity/README.md.

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
        echo "[self-host-parity:stable-subset-section] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:stable-subset-section] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/stable_subset_section_checker/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/stable_subset_section_checker}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/stable_subset_section_checker/expected/clean.json"
MANIFEST_PATH="docs/107_beta_stable_subset.md"

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:stable-subset-section] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_JSON_FILE" ]]; then
    echo "[self-host-parity:stable-subset-section] missing expected JSON: $EXPECTED_JSON_FILE" >&2
    exit 1
fi
if [[ ! -f "$ROOT_DIR/$MANIFEST_PATH" ]]; then
    echo "[self-host-parity:stable-subset-section] missing manifest input: $MANIFEST_PATH" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../lib"
cp "$ROOT_DIR/src/self_hosted/lib/json.pgy" "$PERGYRA_TOOL_BUILD_DIR/../../lib/json.pgy"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:stable-subset-section] clean repo exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.stable-subset-section.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stable-subset-section] schema header missing from Pergyra stdout" >&2
    exit 1
fi

# Shell drift detector - count `^## ` lines.
SHELL_SECTIONS="$(grep -c '^## ' "$ROOT_DIR/$MANIFEST_PATH" || true)"
if [[ -z "$SHELL_SECTIONS" || "$SHELL_SECTIONS" -eq 0 ]]; then
    echo "[self-host-parity:stable-subset-section] shell grep ground truth empty" >&2
    exit 1
fi

if ! grep -Fq "\"sections\":${SHELL_SECTIONS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stable-subset-section] counts.sections parity FAIL (shell=${SHELL_SECTIONS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq '"missing":0' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stable-subset-section] counts.missing parity FAIL (expected 0)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

# Clean JSON shape parity.
PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.stable-subset-section.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:stable-subset-section] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Synthetic missing-section fixture: strip one canonical anchor, expect rc=1.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-stable.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/docs"
mkdir -p "$NEG_ROOT/.tmp"
# Drop the "## 3. Ownership Stable Subset" anchor and its trailing space.
grep -v '^## 3\. Ownership Stable Subset$' \
    "$ROOT_DIR/$MANIFEST_PATH" > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"missing":1' <<<"$NEG_OUT"; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected missing:1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq 'Ownership Stable Subset' <<<"$NEG_OUT"; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected stripped anchor in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:stable-subset-section" "$PERGYRA_TOOL" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:stable-subset-section] rung-2 parity ok (sections=$SHELL_SECTIONS missing-fixture rc=1)"
