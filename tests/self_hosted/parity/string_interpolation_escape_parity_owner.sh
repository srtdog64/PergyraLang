#!/usr/bin/env bash
# Native and the default self-host route read one interpolated string the same
# way. An opener preceded by an odd run of backslashes (`\${`, `\{` in `$"..."`)
# is literal text and the backslash is dropped; an even run escapes only the
# backslashes (docs/grammar/01_syntax.md). The expected lines are written from
# that rule, not from either compiler's output.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-string-interpolation-escape"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/string_interpolation_escape"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURE="tests/self_hosted/fixtures/string_interpolation_escape_parity.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf '%s\n' 'a${x}b' 'c{x}d' 'e\5f' 'g5h' 'i\${x}j' '5k{x}' \
    >"$WORK_DIR/expected.out"

for leg in native-c native-llvm default-c default-llvm; do
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
    esac
    binary_rel="$WORK_REL/$leg.exe"
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$FIXTURE" "${flags[@]}" -o "$binary_rel") \
        >"$WORK_DIR/$leg.compile.log" 2>&1 || {
            cat "$WORK_DIR/$leg.compile.log" >&2
            fail "$leg did not compile the escape fixture"
        }
    "$ROOT_DIR/$binary_rel" | tr -d '\r' >"$WORK_DIR/$leg.out"
    cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/$leg.out" || {
        diff -u "$WORK_DIR/expected.out" "$WORK_DIR/$leg.out" >&2 || true
        fail "$leg read an interpolation escape differently"
    }
done

echo "[$LABEL] native and default route C/LLVM agree on escaped openers: PASS"
