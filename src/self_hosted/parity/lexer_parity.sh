#!/usr/bin/env bash
# Rung 1 parity for the minimal Pergyra-origin lexer (2026-05-27).
# Asserts: clean exit, byte-equal stdout vs committed C-lexer fixtures, and
# live-drift guard vs `pgy --tokens` for each source pair.
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
        echo "[self-host-parity:lexer] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lexer] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/lexer/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lexer}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/lexer/fixture"
SOURCE_OVERRIDE="$FIXTURE_DIR/source.txt"

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:lexer] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

# Sources to lex + their committed fixtures. Each entry is
# "<source path relative to repo root>:<fixture filename>". The Pergyra
# binary reads `fixture/source.txt` to decide which source to lex, so
# the same binary handles multiple sources without `Args()`.
SOURCE_PAIRS=(
    "examples/hello.pgy:hello_tokens.txt"
    "examples/array_literal.pgy:array_literal_tokens.txt"
    "examples/break_continue.pgy:break_continue_tokens.txt"
    "examples/basic.pgy:basic_tokens.txt"
    "examples/heap.pgy:heap_tokens.txt"
    "examples/binary_search.pgy:binary_search_tokens.txt"
)

# Always restore source.txt on exit so committed state stays clean
# (no fixture/source.txt should be tracked).
cleanup_source_override() {
    rm -f "$SOURCE_OVERRIDE"
}
trap cleanup_source_override EXIT

ANY_DRIFT_GUARD_RAN="no"

for pair in "${SOURCE_PAIRS[@]}"; do
    src="${pair%%:*}"
    fix="${pair##*:}"
    expected_file="$FIXTURE_DIR/$fix"

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[self-host-parity:lexer] missing source: $src" >&2
        exit 1
    fi
    if [[ ! -f "$expected_file" ]]; then
        echo "[self-host-parity:lexer] missing fixture: $expected_file" >&2
        exit 1
    fi

    # Set the source override so the Pergyra binary lexes this file.
    printf '%s' "$src" > "$SOURCE_OVERRIDE"

    set +e
    PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null \
        | tr -d '\r' \
        | sed '/^pgy: compiled /d')"
    P_RC=$?
    set -e
    if [[ "$P_RC" -ne 0 ]]; then
        echo "[self-host-parity:lexer] $src: clean exit-code FAIL (pergyra=$P_RC)" >&2
        printf '%s\n' "$PERGYRA_OUT" >&2
        exit 1
    fi

    EXPECTED_OUT="$(tr -d '\r' < "$expected_file")"
    if [[ "$PERGYRA_OUT" != "$EXPECTED_OUT" ]]; then
        echo "[self-host-parity:lexer] $src: fixture byte-drift" >&2
        diff <(printf '%s\n' "$EXPECTED_OUT") <(printf '%s\n' "$PERGYRA_OUT") | head -20 >&2
        exit 1
    fi

    # Live C-lexer drift guard for this source pair.
    set +e
    LIVE_OUT="$(cd "$ROOT_DIR" && "$PGY" --tokens "$src" 2>/dev/null)"
    LIVE_RC=$?
    set -e
    if [[ "$LIVE_RC" -eq 0 && -n "$LIVE_OUT" ]]; then
        LIVE_NORM="$(printf '%s' "$LIVE_OUT" | tr -d '\r')"
        if [[ "$LIVE_NORM" != "$EXPECTED_OUT" ]]; then
            echo "[self-host-parity:lexer] $src: committed fixture drifted from live pgy --tokens" >&2
            exit 1
        fi
        ANY_DRIFT_GUARD_RAN="yes"
    fi
done

echo "[self-host-parity:lexer] rung-1 parity ok (${#SOURCE_PAIRS[@]} sources byte-equal; live-drift=$ANY_DRIFT_GUARD_RAN)"
