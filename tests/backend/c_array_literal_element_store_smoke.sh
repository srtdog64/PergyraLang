#!/usr/bin/env bash
# Native C stores array literal elements into the pre-sized backing in order
# and publishes the length once (transpiler_expr_composite_literal_emit.c),
# with no pgy_array_push_T per element. Int (non-constant), String, nested
# Array<Int>, a struct, an element that calls a function and an element whose
# index panics run as native LLVM does (output, exit code, panic class), under
# AddressSanitizer with no memory error; the Int literal loop is also free of
# leaks under LeakSanitizer.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="c-array-literal-element-store"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/backend/c_array_literal_element_store"
WORK_DIR="$ROOT_DIR/$WORK_REL"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CLANG" >/dev/null 2>&1 || fail "missing clang: $CLANG"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"
cat >"$WORK_DIR/values.pgy" <<'PGY'
struct Pair {
    left: Int;
    right: String;
}

func Twice(x: Int) -> Int {
    return x * 2;
}

func Main() -> Int {
    var i: Int = 0;
    var total: Int = 0;
    while i < 3 {
        let ints: Array<Int> = [i % 17, 11, Twice(i), i + 7];
        let words: Array<String> = ["a" + ToString(i), "b", ToString(Twice(i))];
        let nested: Array<Array<Int>> = [[i, 1], [Twice(i), 5, i], [2]];
        let pairs: Array<Pair> = [Pair(i, "p"), Pair(Twice(i), "q")];
        total = total + ints[3] + ArrayLength(words) + nested[1][2] + pairs[1].left;
        Log(words[0]);
        Log(words[2]);
        Log(pairs[1].right);
        Log(nested[1][0]);
        i = i + 1;
    }
    Log(total);
    return 0;
}
PGY
cat >"$WORK_DIR/panic.pgy" <<'PGY'
func Main() -> Int {
    let base: Array<Int> = [1, 2, 3];
    var k: Int = 1;
    Log(base[k]);
    let data: Array<Int> = [k, base[k + 5], 4];
    Log(data[0]);
    return 0;
}
PGY
cat >"$WORK_DIR/ints.pgy" <<'PGY'
func Main() -> Int {
    var i: Int = 0;
    var total: Int = 0;
    while i < 1000 {
        let data: Array<Int> = [i % 17, 11, i % 23, 7];
        total = total + data[i % 4];
        i = i + 1;
    }
    Log(total);
    return 0;
}
PGY

observe() {
    local bin="$1" stem="$2" rc=0
    (ulimit -c 0; "$bin" </dev/null >"$WORK_DIR/$stem.stdout" 2>"$WORK_DIR/$stem.stderr") 2>/dev/null || rc=$?
    { tr -d '\r' <"$WORK_DIR/$stem.stdout"; echo "rc=$rc"
      grep -o 'class=[a-z-]*' "$WORK_DIR/$stem.stderr" || true; } >"$WORK_DIR/$stem.observed"
}
count() { { grep -oE -- "$2" "$1" || true; } | wc -l | tr -d ' '; }
sanitize=(-O0 -g -std=gnu11 -fwrapv -fno-strict-aliasing -fsanitize=address,undefined
    -fno-omit-frame-pointer -pthread -I"$ROOT_DIR/src/runtime" -I"$ROOT_DIR/src")

for stem in values panic ints; do
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$WORK_REL/$stem.pgy" --emit-c -o "$WORK_REL/$stem.c") \
        >"$WORK_DIR/$stem.emit.log" 2>&1 || fail "$stem C emission failed"
    for backend in c llvm; do
        (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$WORK_REL/$stem.pgy" --backend=$backend \
            -o "$WORK_REL/$stem.$backend.exe") >"$WORK_DIR/$stem.$backend.log" 2>&1 ||
            fail "$stem $backend build failed"
        observe "$WORK_DIR/$stem.$backend.exe" "$stem.$backend"
    done
    cmp -s "$WORK_DIR/$stem.c.observed" "$WORK_DIR/$stem.llvm.observed" || fail "$stem C runs unlike LLVM"
    "$CLANG" "${sanitize[@]}" "$WORK_DIR/$stem.c" -lm -o "$WORK_DIR/$stem.asan" \
        >"$WORK_DIR/$stem.asan.log" 2>&1 || fail "$stem C did not build under sanitizers"
    leaks=0; [[ "$stem" == ints ]] && leaks=1
    ASAN_OPTIONS="detect_leaks=$leaks" observe "$WORK_DIR/$stem.asan" "$stem.asan"
    ! grep -Eq 'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:' "$WORK_DIR/$stem.asan.stderr" ||
        fail "$stem C failed under sanitizers: $(grep -Em1 'ERROR|runtime error' "$WORK_DIR/$stem.asan.stderr")"
    cmp -s "$WORK_DIR/$stem.c.observed" "$WORK_DIR/$stem.asan.observed" || fail "$stem sanitized C runs differently"
    [[ "$(count "$WORK_DIR/$stem.c" 'pgy_array_push_')" == 0 ]] || fail "$stem C pushes literal elements"
done
grep -Fq 'class=out-of-bounds' "$WORK_DIR/panic.c.observed" || fail "the panicking element did not panic"
# values: ints 4, words 3, the outer nested 3, [i, 1] 2, [Twice(i), 5, i] 3 and
# pairs 2 elements are stored one by one; [2] keeps the constant copy.
[[ "$(count "$WORK_DIR/values.c" '_pgy_arr_[0-9]+\.data\[[0-9]+\] = ')" == 17 ]] ||
    fail "values C stored $(count "$WORK_DIR/values.c" '_pgy_arr_[0-9]+\.data\[[0-9]+\] = ') elements, expected 17"
[[ "$(count "$WORK_DIR/panic.c" '_pgy_arr_[0-9]+\.data\[[0-9]+\] = ')" == 3 ]] || fail "panic C did not store its 3 elements"
[[ "$(count "$WORK_DIR/ints.c" '_pgy_arr_[0-9]+\.length = 4; ')" == 1 ]] || fail "ints C did not publish its length once"
echo "[$LABEL] Int, String, nested Array, struct, call and panicking literal elements are stored in order with one length store, run as native LLVM, clean under ASan/UBSan, Int loop leak-free: PASS"
