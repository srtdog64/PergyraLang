#!/usr/bin/env bash
# Rung 2 parity for the stable subset section checker (2026-05-27).
#
# Pergyra is the origin (src/self_hosted/tools/stable_subset_section_checker/main.pgy).
# The Pergyra expected JSON artifact is the clean-output oracle. This script
# asserts exit-code parity, JSON shape parity against expected/clean.json, and a
# synthetic missing-section fixture.
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

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/stable_subset_section_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/stable_subset_section_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:stable-subset-section" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "stable-subset-section-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 6 ]]; then
    echo "[self-host-parity:stable-subset-section] TestHarness manifest expected 6 stable-subset rows, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
MANIFEST_PATH="${harness_paths[2]}"
MISSING_SECTION_ANCHOR="${harness_paths[3]}"
MISSING_FINDING_FIELD="${harness_paths[4]}"
MISSING_FINDING_VALUE="${harness_paths[5]}"

if [[ -z "$MISSING_SECTION_ANCHOR" || -z "$MISSING_FINDING_FIELD" || -z "$MISSING_FINDING_VALUE" ]]; then
    echo "[self-host-parity:stable-subset-section] invalid TestHarness missing-section row" >&2
    exit 1
fi

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

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/stable_subset_section_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/stable_subset_section_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:stable-subset-section] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:stable-subset-section" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>/dev/null)"
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

# Clean JSON shape parity.
PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.stable-subset-section.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:stable-subset-section" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

# Synthetic missing-section fixture: strip one canonical anchor, expect rc=1.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-stable.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/docs"
mkdir -p "$NEG_ROOT/.tmp"
# Drop the TestHarness-owned anchor.
grep -Fv -- "$MISSING_SECTION_ANCHOR" \
    "$ROOT_DIR/$MANIFEST_PATH" > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>&1)"
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
if ! grep -Fq "\"${MISSING_FINDING_FIELD}\":${MISSING_FINDING_VALUE}" <<<"$NEG_OUT"; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected ${MISSING_FINDING_FIELD}:${MISSING_FINDING_VALUE}" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq "$MISSING_SECTION_ANCHOR" <<<"$NEG_OUT"; then
    echo "[self-host-parity:stable-subset-section] missing-section fixture expected stripped anchor in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:stable-subset-section" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$MANIFEST_PATH"
echo "[self-host-parity:stable-subset-section] rung-2 parity ok (expected-json clean; missing-fixture rc=1)"
