#!/usr/bin/env bash
# Run the Pergyra-origin parser against every examples/*.pgy file and
# count how many produce byte-equal output vs `pgy --ast`. This is a
# coverage probe — not a parity gate. Output is a count summary plus
# lists of matching and failing files. Read-only beyond source.txt.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "[scale-probe] missing pgy: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parser_scale}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
SOURCE_OVERRIDE="$ROOT_DIR/src/self_hosted/parser/fixture/source.txt"

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

echo "[scale-probe] compiling parser..."
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")" -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main.exe")" >/dev/null)

cleanup() { rm -f "$SOURCE_OVERRIDE"; }
trap cleanup EXIT

shopt -s nullglob
TOTAL=0
MATCH=0
DIFFER=0
P_FAIL=0
C_SKIP=0
MATCH_LIST=()
FAIL_LIST=()

for src in "$ROOT_DIR"/examples/*.pgy; do
    rel="${src#$ROOT_DIR/}"
    TOTAL=$((TOTAL + 1))

    printf '%s' "$rel" > "$SOURCE_OVERRIDE"

    LIVE="$(cd "$ROOT_DIR" && "$PGY" --ast "$rel" 2>/dev/null)"
    LIVE_RC=$?
    if [[ "$LIVE_RC" -ne 0 || -z "$LIVE" ]]; then
        C_SKIP=$((C_SKIP + 1))
        continue
    fi

    PERGYRA="$(cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main.exe" 2>/dev/null \
        | tr -d '\r')"
    P_RC=$?
    if [[ "$P_RC" -ne 0 ]]; then
        P_FAIL=$((P_FAIL + 1))
        FAIL_LIST+=("$rel (pergyra exit $P_RC)")
        continue
    fi

    LIVE_NORM="$(printf '%s' "$LIVE" | tr -d '\r')"
    if [[ "$PERGYRA" == "$LIVE_NORM" ]]; then
        MATCH=$((MATCH + 1))
        MATCH_LIST+=("$rel")
    else
        DIFFER=$((DIFFER + 1))
        FAIL_LIST+=("$rel (byte-drift)")
    fi
done

echo "[scale-probe] total=$TOTAL match=$MATCH differ=$DIFFER pergyra-fail=$P_FAIL c-skip=$C_SKIP"
if [[ "${1:-}" == "--failing" ]]; then
    for f in "${FAIL_LIST[@]}"; do echo "  - $f"; done
elif [[ ${#MATCH_LIST[@]} -gt 0 ]]; then
    echo "[scale-probe] matching files:"
    for f in "${MATCH_LIST[@]}"; do
        echo "  + $f"
    done
fi
