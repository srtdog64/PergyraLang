#!/usr/bin/env bash
# Args() hands the program UTF-8. On Windows main() receives argv in the ANSI
# code page (CP949 on a Korean system), so pgy_runtime_process_utf8_argv
# rereads the UTF-16 command line there; elsewhere argv passes through.
# `한글` must arrive as ED 95 9C EA B8 80 on every platform, and on Windows the
# owner must also compile after <windows.h> and <shellapi.h>.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
LABEL="process-args-utf8"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/process-args-utf8.XXXXXX)"
CC_BIN="${CC:-gcc}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

variants=(plain)
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) variants+=(windows-headers) ;;
esac

expected=$'arg1 ED 95 9C EA B8 80\narg2 61 20 62\narg3 70 6C 61 69 6E'
for variant in "${variants[@]}"; do
    flags=()
    [[ "$variant" == windows-headers ]] && flags+=(-DPGY_TEST_INCLUDE_WINDOWS_H)
    "$CC_BIN" -std=c11 -Wall -Wextra -Werror "${flags[@]}" -Isrc \
        tests/process_args_utf8_runtime.c -o "$WORK/$variant.exe" ||
        fail "$variant variant did not compile"
    actual="$("$WORK/$variant.exe" "한글" "a b" "plain" | tr -d '\r')" ||
        fail "$variant variant exited non-zero"
    [[ "$actual" == "$expected" ]] ||
        { printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
          fail "$variant variant did not deliver UTF-8 arguments"; }
done

echo "[$LABEL] arguments arrive as UTF-8 (${variants[*]}): PASS"
