#!/usr/bin/env bash
# A program may hold several `extern "C"` blocks, in one module or one per
# imported module. The self-host MIR consumer indexed every declaration by
# name and refused a repeated name, so a second block, also named by its ABI
# `C`, made the default route fail with "self-host C emission rejected invalid
# machine-layer facts" while the native pipeline compiled it.
# - two blocks in one file and two imported modules with a block each compile
#   and run on native C, native LLVM and the default C route;
# - the default route's emitted C declares every prototype.
# The default LLVM route refuses extern declarations before emission, with one
# block or several, so it is not a leg here.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="extern-block-identity"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/extern_block_identity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures/extern_blocks"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# expect_run NAME EXPECTED
expect_run() {
    local name="$1" expected="$2" leg out_rel
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    for leg in native-c native-llvm default-c; do
        out_rel="$WORK_REL/$name-$leg.exe"
        # shellcheck disable=SC2046
        (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$FIXTURES/$name.pgy" $(leg_flags "$leg") -o "$out_rel") \
            >"$ROOT_DIR/$out_rel.log" 2>&1 ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg printed another value for $name"; }
    done
}

# expect_prototypes NAME SYMBOL...
expect_prototypes() {
    local name="$1" symbol
    shift
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$FIXTURES/$name.pgy" --emit-c -o "$WORK_REL/$name.c") \
        >"$WORK_DIR/$name.emit.log" 2>&1 ||
        { cat "$WORK_DIR/$name.emit.log" >&2; fail "default route did not emit C for $name"; }
    for symbol in "$@"; do
        grep -Eq "^[a-z0-9_ ]+ \\*?$symbol\\(" "$WORK_DIR/$name.c" ||
            fail "default route C for $name does not declare $symbol"
    done
}

expect_run two_blocks 'two extern blocks'
expect_run two_modules $'module a\nmodule b'
expect_prototypes two_blocks pgy_fixture_extern_a pgy_fixture_extern_b
expect_prototypes two_modules pgy_fixture_module_a pgy_fixture_module_b

echo "[$LABEL] two extern blocks in one file and one per imported module compile and run on native C, native LLVM and the default C route, with every prototype declared: PASS"
