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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[regen-expected] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/semantic/fixture"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic}"
TOOL="$BUILD_DIR/main.pgy"
TOOL_BIN="$BUILD_DIR/main_regen.exe"

mkdir -p "$BUILD_DIR"
cp "$TOOL_SOURCE" "$TOOL"
LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

echo "[regen-expected] compiling semantic tool..."
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$TOOL_BIN")" >/dev/null)

count=0
for source in "$FIXTURE_DIR"/*.pgy; do
    base="$(basename "$source" .pgy)"
    verdict="$(cd "$ROOT_DIR" && "$TOOL_BIN" \
        "src/self_hosted/semantic/fixture/${base}.pgy" 2>/dev/null | tr -d '\r')"
    printf '%s\n' "$verdict" > "$EXPECTED_DIR/${base}.diag"
    rm -f "$EXPECTED_DIR/${base}.json" "$EXPECTED_DIR/${base}.txt"
    count=$((count + 1))
done

echo "[regen-expected] wrote $count expected/*.diag (removed any stale *.json/*.txt)"
