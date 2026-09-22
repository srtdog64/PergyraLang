#!/usr/bin/env bash
# ToInt and ToFloat parse text (docs/205 section 11.2). The runtime reads the
# argument as a C string, so a number used to compile and crash at run time.
# Both compilers now take only a String: a numeric argument is refused before
# any binary is written, and text still converts on native C/LLVM and the
# default C route.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="text-conversion-argument"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/text_conversion_argument"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

printf '13\nfloat ok\n' >"$WORK_DIR/expected"
for leg in native-c native-llvm default-c; do
    out_rel="$WORK_REL/text-$leg.exe"
    compile "$FIXTURES/text_conversion_argument.pgy" "$leg" "$out_rel" ||
        { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile text conversions"; }
    "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/text-$leg.out" ||
        fail "$leg text conversion binary failed"
    cmp -s "$WORK_DIR/expected" "$WORK_DIR/text-$leg.out" ||
        { diff -u "$WORK_DIR/expected" "$WORK_DIR/text-$leg.out" >&2 || true
          fail "$leg converted the text to another value"; }
done

# The native front end and the self-host front end each refuse a number.
for name in int float; do
    for leg in native-c default-c; do
        out_rel="$WORK_REL/number-$name-$leg.exe"
        if compile "$FIXTURES/text_conversion_number_${name}_negative.pgy" "$leg" "$out_rel"; then
            fail "$leg accepted a number passed to To${name^}"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for the refused To${name^} call"
        grep -Eiq "type mismatch|String" "$ROOT_DIR/$out_rel.log" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg refused To${name^} for another reason"; }
    done
done

echo "[$LABEL] ToInt/ToFloat convert text on native C/LLVM and the default C route and refuse a number on both front ends: PASS"
