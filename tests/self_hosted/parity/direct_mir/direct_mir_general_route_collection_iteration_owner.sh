#!/usr/bin/env bash
# The general direct-MIR route walks `for v in xs` over a runtime Array<Int>,
# Array<Bool> or Array<String> local or parameter as a cursor range: the
# header compares the cursor with the live `ArrayLength(xs)`, the first
# operation of the body binds `v = xs[cursor]` (ownership owners see it as
# `let v = xs[i]`), and the backedges advance the cursor. Before 2026-11-19 the
# route took no foreach at all: a parameter iterable died in the literal
# foreach admission and a local one ended the routine input with "an external
# collection plan".
# - A range's bound is the header row's Int expression, evaluated again on
#   every pass as on the native backends: `0..ArrayLength(xs)` with a push in
#   the body, and `0..n - 1`. The route used to admit only a literal or local
#   bound.
# - Each program runs on native C/LLVM, the default LLVM route and the
#   general emitter's C backend with the same stdout and exit status.
#   - The general emitter's LLVM leg links no runtime library, so it cannot
#     resolve the Array index bounds-panic export the default LLVM route links.
#   - The default C route claims the general emitter by its own rule
#     (collection ownership facts, or LocalRef wire facts with every routine
#     claimed); these programs fall to the MIR-to-AST round trip, which refused
#     the parameter iterables and printed garbage for `live_local_push` before
#     this change as after it. One claim rule for both backends is open work.
# - A forged MIR whose loop-init names another binding or iterable than its
#   header is refused with no artifact.
# - The route also takes a one-routine program: it claimed one only for a
#   control-flow or builtin shape, or a record or enum (`single-program-shape`),
#   and refused a `continue` inside a range over a literal.
# - A String binding borrows its element: aliasing it, pushing it into
#   another array and returning it run equal, and the general emitter's C
#   runs them under ASan with no freed-memory access.
# - A String walk whose body pops or sets an Array is refused at the typed
#   `collection_range` stage; either can release the borrowed element (the
#   native backends print freed memory for both).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-general-route-collection-iteration"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_general_route_collection_iteration"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CC="${PGY_SELFHOST_CC:-gcc}"
LEGS=(native-c native-llvm default-llvm direct-c)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v python3 >/dev/null || fail "python3 is unavailable"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/parameter_break_continue.pgy" <<'PGY'
func Tally(xs: Array<Int>) -> Int {
    var total: Int = 0;
    for v in xs {
        if v == 3 { continue; }
        if v > 8 { break; }
        total = total + v;
    }
    return total;
}

func Main() -> Void {
    let xs: Array<Int> = [1, 2, 3, 4, 9, 5];
    Log(Tally(xs));
}
PGY

cat >"$WORK_DIR/nested_parameters.pgy" <<'PGY'
func Pairs(rows: Array<Int>, cols: Array<Int>) -> Int {
    var total: Int = 0;
    for r in rows {
        for c in cols {
            if r < c { total = total + r * c; }
        }
    }
    return total;
}

func Main() -> Void {
    let rows: Array<Int> = [1, 2, 3];
    let cols: Array<Int> = [2, 3];
    Log(Pairs(rows, cols));
}
PGY

# The cursor reads the live collection: elements pushed by the body are
# visited, as on the native backends.
cat >"$WORK_DIR/live_local_push.pgy" <<'PGY'
func Grow(seed: Int) -> Int {
    var xs: Array<Int> = [seed, seed + 1, seed + 2];
    var n: Int = 0;
    for v in xs {
        if n < 5 { ArrayPush(xs, v + 10); }
        n = n + 1;
        Log(v);
    }
    return n;
}

func Main() -> Void {
    Log(Grow(1));
}
PGY

cat >"$WORK_DIR/bool_elements.pgy" <<'PGY'
func CountTrue(flags: Array<Bool>) -> Int {
    var n: Int = 0;
    for f in flags {
        if f { n = n + 1; }
    }
    return n;
}

func Main() -> Void {
    let flags: Array<Bool> = [true, false, true, true];
    Log(CountTrue(flags));
}
PGY

# A range and a foreach in one routine publish LocalRef wire facts.
cat >"$WORK_DIR/range_beside_foreach.pgy" <<'PGY'
func Sum(xs: Array<Int>) -> Int {
    var total: Int = 0;
    for v in xs {
        total = total + v;
    }
    for i in 0..3 {
        total = total + i;
    }
    return total;
}

func Main() -> Void {
    let xs: Array<Int> = [1, 2, 3];
    Log(Sum(xs));
}
PGY

cat >"$WORK_DIR/range_length_bound.pgy" <<'PGY'
func Grow(seed: Int) -> Int {
    var xs: Array<Int> = [seed, seed + 1, seed + 2];
    var n: Int = 0;
    for i in 0..ArrayLength(xs) {
        if i < 3 { ArrayPush(xs, i); }
        n = n + xs[i];
    }
    return n;
}

func Main() -> Void {
    Log(Grow(1));
}
PGY

cat >"$WORK_DIR/range_expression_bound.pgy" <<'PGY'
func Sum(n: Int) -> Int {
    var total: Int = 0;
    for i in 0..n - 1 {
        total = total + i;
    }
    return total;
}

func Main() -> Void {
    Log(Sum(5));
    Log(Sum(1));
}
PGY

cat >"$WORK_DIR/one_routine_continue.pgy" <<'PGY'
func Main() -> Void {
    var total: Int = 0;
    for i in 0..10 {
        if i % 2 == 0 {
            continue;
        }
        total = total + i;
    }
    let names: Array<String> = ["a", "bb"];
    for name in names {
        total = total + StringLength(name);
    }
    Log(ToString(total));
}
PGY

cat >"$WORK_DIR/string_elements.pgy" <<'PGY'
func Width(names: Array<String>) -> Int {
    var total: Int = 0;
    var last: String = "";
    for name in names {
        total = total + StringLength(name);
        last = name;
    }
    Log(last);
    return total;
}

func Main() -> Void {
    let names: Array<String> = ["ab", "cde"];
    Log(Width(names));
    Log(names[0]);
}
PGY

cat >"$WORK_DIR/string_push_borrowed.pgy" <<'PGY'
func Copy(names: Array<String>) -> Int {
    var out: Array<String> = [];
    for name in names {
        ArrayPush(out, name);
    }
    for kept in out {
        Log(kept);
    }
    return ArrayLength(out);
}

func Main() -> Void {
    let names: Array<String> = ["x", "yy"];
    Log(Copy(names));
    Log(names[1]);
}
PGY

cat >"$WORK_DIR/string_return_borrowed.pgy" <<'PGY'
func Find(names: Array<String>, wanted: Int) -> String {
    for name in names {
        if StringLength(name) == wanted { return name; }
    }
    return "none";
}

func Main() -> Void {
    let names: Array<String> = ["a", "bcd", "ef"];
    Log(Find(names, 2));
    Log(Find(names, 9));
    Log(names[2]);
}
PGY

cat >"$WORK_DIR/string_concat_local.pgy" <<'PGY'
func Join(seed: String) -> String {
    let names: Array<String> = ["a", "bb", "ccc"];
    var acc: String = seed;
    for name in names {
        acc = Concat(acc, name);
    }
    return acc;
}

func Main() -> Void {
    Log(Join(">"));
}
PGY

cat >"$WORK_DIR/string_pop_while_bound.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    ArrayPush(names, ToString(seed + 2));
    var n: Int = 0;
    for name in names {
        ArrayPop(names);
        Log(name);
        n = n + 1;
    }
    return n;
}

func Main() -> Void {
    Log(Drain(1));
}
PGY

cat >"$WORK_DIR/string_set_while_bound.pgy" <<'PGY'
func Swap(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var n: Int = 0;
    for name in names {
        ArraySet(names, 0, ToString(99));
        Log(name);
        n = n + 1;
    }
    return n;
}

func Main() -> Void {
    Log(Swap(5));
}
PGY

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
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
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
        "$PGY" "$WORK_REL/$name.pgy" $(leg_flags "$leg") \
        -o "$WORK_REL/$name-$leg.exe") \
        >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
    return "$status"
}

# expect_run NAME STDOUT: every leg prints STDOUT and exits 0.
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
}

expect_run parameter_break_continue '7'
expect_run nested_parameters '11'
expect_run live_local_push $'1\n2\n3\n11\n12\n13\n21\n22\n8'
expect_run bool_elements '3'
expect_run range_beside_foreach '9'
expect_run range_length_bound '9'
expect_run range_expression_bound $'6\n0'
expect_run string_elements $'cde\n5\nab'
expect_run string_push_borrowed $'x\nyy\n2\nyy'
expect_run string_return_borrowed $'ef\nnone\nef'
expect_run string_concat_local '>abbccc'
expect_run one_routine_continue '28'

# expect_asan NAME: the general emitter's C of NAME, built with ASan, prints
# the expected output with no freed-memory access. The general emitter does
# not retire every temporary yet, so leak detection stays off.
expect_asan() {
    local name="$1" status=0
    (cd "$ROOT_DIR" && "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
        -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread "$WORK_REL/$name-direct.c" \
        -o "$WORK_REL/$name-asan.exe" -lm) >"$WORK_DIR/$name-asan.compile.log" 2>&1 ||
        { cat "$WORK_DIR/$name-asan.compile.log" >&2; fail "$name did not compile with ASan"; }
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$WORK_DIR/$name-asan.exe" </dev/null \
        2>"$WORK_DIR/$name-asan.err" | tr -d '\r' >"$WORK_DIR/$name-asan.out" || status=$?
    [[ "$status" == 0 ]] || { cat "$WORK_DIR/$name-asan.err" >&2; fail "$name failed under ASan"; }
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-asan.out" || fail "$name printed other output under ASan"
}
for name in string_elements string_push_borrowed string_return_borrowed string_concat_local; do
    expect_asan "$name"
done

# forge NAME FIELD VALUE OUT: rewrites FIELD of the first Tally loop-init row.
forge() {
    python3 - "$WORK_DIR/$1.mir.json" "$2" "$3" "$WORK_DIR/$4" <<'PY'
import json, sys
source, field, value, out = sys.argv[1:5]
mir = json.load(open(source))
routine = next(r for r in mir["routines"] if r["name"] == "Tally")
row = next(i for b in routine["blocks"] for i in b["instructions"]
           if i["kind"] == "loop-init" and i.get("source_type") == "AST_FOR_LOOP")
row[field] = value
json.dump(mir, open(out, "w"))
PY
}

# expect_refused MIR BACKEND MESSAGE: the driver exits nonzero with MESSAGE
# and writes no artifact.
expect_refused() {
    local mir="$1" backend="$2" message="$3" status=0
    rm -f "$WORK_DIR/refused.out"
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend="$backend" "$WORK_REL/$mir" \
        -o "$WORK_REL/refused.out") >"$WORK_DIR/$mir.$backend.log" 2>&1 || status=$?
    [[ "$status" != 0 ]] || fail "$mir on $backend was admitted"
    [[ ! -s "$WORK_DIR/refused.out" ]] || fail "$mir on $backend wrote an artifact"
    grep -Fq "$message" "$WORK_DIR/$mir.$backend.log" ||
        { cat "$WORK_DIR/$mir.$backend.log" >&2; fail "$mir on $backend: expected '$message'"; }
}

forge parameter_break_continue arg0 w forged_binding.mir.json
forge parameter_break_continue expr1 ys forged_iterable.mir.json
for backend in c llvm; do
    expect_refused forged_binding.mir.json "$backend" "range instruction/CFG join is invalid"
    expect_refused forged_iterable.mir.json "$backend" "collection iteration rows disagree"
done

# The self-host driver prints a route refusal as a CODEGEN ERROR artifact.
for name in string_pop_while_bound string_set_while_bound; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
        -o "$WORK_REL/$name.mir.json") >"$WORK_DIR/$name.mir.log" 2>&1 ||
        fail "$name did not lower to MIR"
    for backend in c llvm; do
        status=0
        rm -f "$WORK_DIR/$name.$backend.out"
        (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend="$backend" "$WORK_REL/$name.mir.json" \
            -o "$WORK_REL/$name.$backend.out") >"$WORK_DIR/$name.$backend.log" 2>&1 ||
            status=$?
        cat "$WORK_DIR/$name.$backend.log" "$WORK_DIR/$name.$backend.out" \
            >"$WORK_DIR/$name.$backend.all" 2>/dev/null || true
        grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$name.$backend.all" &&
            grep -Fq 'collection_range' "$WORK_DIR/$name.$backend.all" ||
            { cat "$WORK_DIR/$name.$backend.all" >&2
              fail "$name on $backend was not refused at collection_range (status $status)"; }
    done
done

echo "[$LABEL] 12 programs equal on four legs, 4 String walks under ASan; 2 forged MIR and 2 releasing String walks refused: PASS"
