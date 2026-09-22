#!/usr/bin/env bash
# The destructure temporary never takes a name the routine already declares
# (docs/205 F2), and only a destructure whose initializer graph root is not a
# leaf gets one (F1). The fixture declares `_pgy_destructure_first` and
# `_pgy_destructure_first_1`; native C/LLVM and the default C route, which
# rebuilds the AST through mir_lower, must print the same values. The default
# LLVM route refuses every destructure today, so it is not a leg.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-destructure-temporary-hygiene"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/destructure_temporary_hygiene"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURE="tests/self_hosted/fixtures/destructure_temporary_hygiene.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf 'hello\nworld\nfoo\nuser\nuser1\n60\n' >"$WORK_DIR/expected"

for leg in native-c native-llvm default-c; do
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
    esac
    out_rel="$WORK_REL/$leg.exe"
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$FIXTURE" "${flags[@]}" -o "$out_rel") \
        >"$WORK_DIR/$leg.log" 2>&1 ||
        { cat "$WORK_DIR/$leg.log" >&2; fail "$leg did not compile the fixture"; }
    "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$leg.out" ||
        fail "$leg binary failed"
    cmp -s "$WORK_DIR/expected" "$WORK_DIR/$leg.out" ||
        { diff -u "$WORK_DIR/expected" "$WORK_DIR/$leg.out" >&2 || true
          fail "$leg printed other values"; }
done

echo "[$LABEL] a user local named like the temporary keeps its value on native C/LLVM and the default C route: PASS"
