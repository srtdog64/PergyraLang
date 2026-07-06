#!/usr/bin/env bash
# Regenerate semantic parity expected files from the tool itself.
# The self-hosted semantic checker is the single source of truth for its own
# verdicts; this script compiles it and records each fixture verdict as the
# committed expected block. Run after changing diagnostic rendering, reason/fix
# tables, or fixtures, then review the diff before committing.
#
#     make pgy
#     tests/self_hosted/parity/regen_expected.sh
#
# It writes expected/<base>.diag for every fixture/<base>.pgy and removes any
# stale expected/<base>.json or .txt left from earlier rendering eras.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v tail >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[regen-expected] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic}"
TOOL_BIN="$BUILD_DIR/main_regen.exe"
HARNESS_PATHS_FILE="$BUILD_DIR/semantic_harness_paths.txt"
TOOL_SOURCE=""
TOOL_ARG=""
FIXTURE_DIR=""
FIXTURE_DIR_REL=""
EXPECTED_DIR=""

mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "regen-expected" \
    "$BUILD_DIR" \
    "semantic-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 7 ]]; then
    echo "[regen-expected] TestHarness manifest expected 7 semantic paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_DIR_REL="${harness_paths[2]}"
EXPECTED_DIR="$ROOT_DIR/${harness_paths[3]}"

if [[ ! -f "$TOOL_SOURCE" ]]; then
    echo "[regen-expected] missing TestHarness semantic source: $TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -d "$FIXTURE_DIR" || ! -d "$EXPECTED_DIR" ]]; then
    echo "[regen-expected] missing TestHarness fixture/expected directory" >&2
    exit 1
fi

echo "[regen-expected] compiling semantic tool..."
(cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$TOOL_BIN")" >/dev/null)

count=0
for source in "$FIXTURE_DIR"/*.pgy; do
    base="$(basename "$source" .pgy)"
    expected_file="$EXPECTED_DIR/${base}.diag"
    verdict="$(cd "$ROOT_DIR" && "$TOOL_BIN" \
        "${FIXTURE_DIR_REL}/${base}.pgy" 2>/dev/null | tr -d '\r')"
    line_break=$'\n'
    if [[ -f "$expected_file" ]] && grep -q $'\r' "$expected_file"; then
        line_break=$'\r\n'
        verdict="${verdict//$'\n'/$'\r\n'}"
    fi
    if [[ -f "$expected_file" && -s "$expected_file" && -z "$(tail -c 1 "$expected_file")" ]]; then
        printf '%s%s' "$verdict" "$line_break" > "$expected_file"
    else
        printf '%s' "$verdict" > "$expected_file"
    fi
    rm -f "$EXPECTED_DIR/${base}.json" "$EXPECTED_DIR/${base}.txt"
    count=$((count + 1))
done

echo "[regen-expected] wrote $count expected/*.diag (removed any stale *.json/*.txt)"
