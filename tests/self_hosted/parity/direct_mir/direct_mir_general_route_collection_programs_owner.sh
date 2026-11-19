#!/usr/bin/env bash
# The general direct-MIR route runs the one-routine collection programs that
# per-program-shape plans used to claim: an empty Array<String> filled by
# ArrayPush, ArrayPop on Array<Int> and Array<String>, an initialized
# Array<Int> summed and set in a while loop, a read-only maximum, a fresh
# ArrayReverse result and a dynamic Array<Int> push. Those plans (the String
# array push, collection pop and Array<Int> program families) and the
# one-routine scalar CFG route that carried them were deleted on 2026-11-20.
# Each program runs on native C/LLVM, the default LLVM route and the general
# emitter's C backend with the same stdout and exit status, and the general
# emitter's C runs under ASan with no freed-memory access.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-general-route-collection-programs"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_general_route_collection_programs"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="src/self_hosted/codegen/fixture"
CC="${PGY_SELFHOST_CC:-gcc}"
LEGS=(native-c native-llvm default-llvm direct-c)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# compile NAME LEG: builds WORK_DIR/NAME-LEG.exe, returns the compiler status.
compile() {
    local name="$1" leg="$2" status=0
    if [[ "$leg" == direct-c ]]; then
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURES/$name.pgy" \
                -o "$WORK_REL/$name.mir.json" &&
            "$DRIVER" --mir-json-backend=c "$WORK_REL/$name.mir.json" \
                -o "$WORK_REL/$name-direct.c" &&
            ! grep -Fq 'CODEGEN ERROR' "$WORK_REL/$name-direct.c" &&
            "$CC" -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread \
                "$WORK_REL/$name-direct.c" -o "$WORK_REL/$name-$leg.exe" -lm) \
            >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
        return "$status"
    fi
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$FIXTURES/$name.pgy" $(leg_flags "$leg") \
        -o "$WORK_REL/$name-$leg.exe") \
        >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
    return "$status"
}

# expect_run NAME STDOUT: every leg prints STDOUT and exits 0, and the
# general emitter's C prints it under ASan. The general emitter does not
# retire every temporary yet, so leak detection stays off.
expect_run() {
    local name="$1" expected_out="$2" leg status
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        compile "$name" "$leg" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2; fail "$leg did not compile $name"; }
        status=0
        "$WORK_DIR/$name-$leg.exe" >"$WORK_DIR/$name-$leg.out" \
            2>"$WORK_DIR/$name-$leg.err" </dev/null || status=$?
        [[ "$status" == 0 ]] || fail "$leg $name exited $status"
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name printed other output"; }
    done
    status=0
    (cd "$ROOT_DIR" && "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
        -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread "$WORK_REL/$name-direct.c" \
        -o "$WORK_REL/$name-asan.exe" -lm) >"$WORK_DIR/$name-asan.compile.log" 2>&1 ||
        { cat "$WORK_DIR/$name-asan.compile.log" >&2; fail "$name did not compile with ASan"; }
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$WORK_DIR/$name-asan.exe" </dev/null \
        2>"$WORK_DIR/$name-asan.err" | tr -d '\r' >"$WORK_DIR/$name-asan.out" || status=$?
    [[ "$status" == 0 ]] || { cat "$WORK_DIR/$name-asan.err" >&2; fail "$name failed under ASan"; }
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-asan.out" || fail "$name printed other output under ASan"
}

expect_run str_array_push $'abbccc\n3'
expect_run array_pop $'30\n2\n2\na'
expect_run array_sum $'60\n99\n3'
expect_run array_max '9'
expect_run array_reverse $'3\n2\n1\n3'
expect_run array_push $'30\n5'

echo "[$LABEL] 6 collection programs equal on four legs and under ASan: PASS"
