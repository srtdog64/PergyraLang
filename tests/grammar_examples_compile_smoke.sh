#!/usr/bin/env bash
set -euo pipefail

# Git Bash launched directly from PowerShell may not inherit /usr/bin in PATH.
# Bootstrap the shell tool path before using dirname/find/sort/wc/sed.
bash_dir="${BASH%/*}"
for path_candidate in "$bash_dir" "$bash_dir/../usr/bin" "/usr/bin" "/mingw64/bin" "/c/Program Files/Git/usr/bin"; do
    if [[ -d "$path_candidate" ]]; then
        PATH="$path_candidate:$PATH"
    fi
done
export PATH

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR" || exit 2

# shellcheck source=tests/pgy_binary_path_helpers.sh
. "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"

PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
OUT_DIR="${PGY_GRAMMAR_EXAMPLES_OUT:-$ROOT_DIR/build/grammar_examples_compile}"
EXPECTED_COUNT=17

PGY_BIN="$(pgy_path_for_bash_tool "$PGY_BIN")"

fail() {
    echo "[grammar-examples] $*" >&2
    exit 1
}

if [[ ! -x "$PGY_BIN" ]]; then
    fail "missing executable compiler: $PGY_BIN"
fi

run_pgy() {
    if pgy_binary_expects_windows_paths "$PGY_BIN"; then
        if ! command -v powershell.exe >/dev/null 2>&1; then
            "$PGY_BIN" "$@"
            return $?
        fi

        local win_pgy
        local ps_args=""
        local arg
        local converted
        win_pgy="$(pgy_path_for_windows_tool "$PGY_BIN")"
        for arg in "$@"; do
            converted="$arg"
            case "$arg" in
                -*)
                    ;;
                *)
                    if [[ -e "$arg" || "$arg" == */* || "$arg" == *\\* ]]; then
                        converted="$(pgy_path_for_windows_tool "$arg")"
                    fi
                    ;;
            esac
            ps_args="${ps_args} $(pgy_powershell_quote "$converted")"
        done
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
            "& $(pgy_powershell_quote "$win_pgy") $ps_args"
        return $?
    fi

    "$PGY_BIN" "$@"
}

mkdir -p "$OUT_DIR"
source_list="$OUT_DIR/sources.txt"
find grammar -type f -name '*.pgy' | sort >"$source_list"

count="$(wc -l <"$source_list" | tr -d '[:space:]')"
if [[ "$count" != "$EXPECTED_COUNT" ]]; then
    fail "grammar example count drifted: $count != $EXPECTED_COUNT"
fi

while IFS= read -r src; do
    [[ -n "$src" ]] || continue
    src_abs="$ROOT_DIR/$src"
    out_name="$(printf '%s' "$src" | sed 's#[/\\:]#_#g').c"
    out_path="$OUT_DIR/$out_name"
    run_pgy --native-pipeline --ast "$src_abs" >/dev/null
    run_pgy "$src_abs" --emit-c -o "$out_path" >/dev/null
    if [[ ! -s "$out_path" ]]; then
        fail "empty emitted C for $src"
    fi
done <"$source_list"

echo "[grammar-examples] $EXPECTED_COUNT syntax examples parse and emit C"
