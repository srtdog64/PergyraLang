#!/usr/bin/env bash
# The self-hosted front end refuses an Array<String> element release, or a
# whole reassignment of the array, while a String that borrows an element of
# that array is read afterwards. A `for` binding borrows the element it reads.
# - An owned pop or set (collection receipt effects 5 and 4) frees one
#   element; a call that hands the array to a formal whose callee pops or
#   deep-drops it may free any (the formal's element requirement).
# - Before 2026-11-20 the receipt marked such a pop an owned String pop while
#   `let name = names[i]` still borrowed the popped element, and the general
#   direct-MIR route ran the program with heap-use-after-free on C and LLVM.
#   An owned set, a pop through an `inout` callee, a loan taken as an earlier
#   argument of the popping call, a loan a callee hands back, and a loan a
#   loop reads again after the release did the same.
# Each refused program fails at the front end: `--emit-mir-json-verified`
# writes no MIR, and the default C and LLVM routes refuse with the boundary
# and publish no executable. Each admitted neighbour (the read before the
# pop, a fresh copy, a pop of another array, a set whose value reads the
# loan, a loan read through a `ref` formal) prints the same output on the
# default C and LLVM routes and, through the general emitter's C under ASan,
# reads no freed memory. A loan a landed compiler Clone copies where its
# argument is taken stays admitted and runs clean on the default C route.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="element-release-loan-refusal"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/element_release_loan_refusal"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CC="${PGY_SELFHOST_CC:-gcc}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# The reported program: the loop local borrows names[i], the pop frees the
# last element, then the local is read.
cat >"$WORK_DIR/pop_index_alias.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    ArrayPush(names, ToString(seed + 2));
    var n: Int = 0;
    var i: Int = 0;
    while i < ArrayLength(names) {
        let name: String = names[i];
        ArrayPop(names);
        Log(name);
        n = n + 1;
        i = i + 1;
    }
    return n;
}
func Main() -> Void { Log(Drain(1)); }
PGY

# The loan is declared before the loop and read at the top of each pass.
cat >"$WORK_DIR/pop_loop_carried.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let top: String = names[1];
    var i: Int = 0;
    while i < 2 {
        Log(top);
        ArrayPop(names);
        i = i + 1;
    }
    return i;
}
func Main() -> Void { Log(Drain(1)); }
PGY

# The loop rebinds the outer local to the last element, pops it, and reads
# the local at the top of the next pass.
cat >"$WORK_DIR/pop_rebound_loop.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var cur: String = "start";
    var n: Int = 0;
    while ArrayLength(names) > 0 {
        Log(cur);
        cur = names[ArrayLength(names) - 1];
        ArrayPop(names);
        n = n + 1;
    }
    return n;
}
func Main() -> Void { Log(Drain(1)); }
PGY

# An owned set frees the element it replaces.
cat >"$WORK_DIR/set_index_alias.pgy" <<'PGY'
func Swap(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let name: String = names[0];
    ArraySet(names, 0, ToString(99));
    Log(name);
    return ArrayLength(names);
}
func Main() -> Void { Log(Swap(5)); }
PGY

# The callee pops through its inout formal.
cat >"$WORK_DIR/inout_callee_pop.pgy" <<'PGY'
func PopOne(inout xs: Array<String>) -> Void {
    ArrayPop(xs);
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = names[1];
    PopOne(names);
    Log(last);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# The loan is an earlier argument of the call that pops through its callee.
cat >"$WORK_DIR/argument_before_release.pgy" <<'PGY'
func PopLen(inout xs: Array<String>) -> Int {
    ArrayPop(xs);
    return ArrayLength(xs);
}
func Show(text: String, n: Int) -> Int { Log(text); return n; }
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = names[1];
    return Show(last, PopLen(names));
}
func Main() -> Void { Log(Drain(1)); }
PGY

# A callee that returns its argument hands the element loan back.
cat >"$WORK_DIR/returned_loan_pop.pgy" <<'PGY'
func Same(text: String) -> String { return text; }
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = Same(names[1]);
    ArrayPop(names);
    Log(last);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# A callee whose String local is fresh text on every definition returns
# fresh text: its result borrows nothing, and the pop frees nothing it reads.
cat >"$WORK_DIR/fresh_result_pop.pgy" <<'PGY'
func Label(text: String, n: Int) -> String {
    var out: String = "";
    var i: Int = 0;
    while i < n {
        out = Concat(out, text);
        i = i + 1;
    }
    return out;
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = Label(names[1], 2);
    ArrayPop(names);
    Log(last);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# One definition of the returned local is the argument: the loan comes back.
cat >"$WORK_DIR/joined_local_result_pop.pgy" <<'PGY'
func Pick(text: String, n: Int) -> String {
    var out: String = "";
    if n > 0 { out = text; }
    return out;
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = Pick(names[1], 1);
    ArrayPop(names);
    Log(last);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# An inout call rebinds the returned local to the argument, with no
# assignment to it in the callee's own body.
cat >"$WORK_DIR/rebound_local_result_pop.pgy" <<'PGY'
func Fill(inout slot: String, text: String) -> Int {
    slot = text;
    return 0;
}
func Pick(text: String) -> String {
    var out: String = "";
    let filled: Int = Fill(out, text);
    return out;
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let last: String = Pick(names[1]);
    ArrayPop(names);
    Log(last);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# UnwrapOption of a fixed Option local takes the loan of the call that made
# it: here none, so a set of another array frees nothing the String reads
# (src/self_hosted/tools/completeness_impact_planner, CI run 44836830024).
cat >"$WORK_DIR/unwrapped_option_set.pgy" <<'PGY'
func Pick(row: String) -> Option<String> {
    return Some(Concat(row, "!"));
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    let bump: Int = seed + 5;
    let row: String = ToString(bump);
    let picked_opt: Option<String> = Pick(row);
    let picked: String = UnwrapOption(picked_opt);
    ArraySet(names, 0, Concat(names[0], picked));
    Log(picked);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# A reassigned Option local is not fixed: its first call no longer says
# what it holds (here an element loan a second call wraps).
cat >"$WORK_DIR/reassigned_option_pop.pgy" <<'PGY'
func Pick(row: String) -> Option<String> {
    return Some(Concat(row, "!"));
}
func Wrap(text: String) -> Option<String> {
    return Some(text);
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let seed_text: String = ToString(seed);
    let row: String = names[1];
    var picked_opt: Option<String> = Pick(seed_text);
    picked_opt = Wrap(row);
    let picked: String = UnwrapOption(picked_opt);
    ArrayPop(names);
    Log(picked);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# The call took a String local that was reassigned after it: that local's
# loan now is not the one it handed the call.
cat >"$WORK_DIR/unfixed_argument_option_pop.pgy" <<'PGY'
func Wrap(text: String) -> Option<String> {
    return Some(text);
}
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var row: String = names[1];
    let wrapped: Option<String> = Wrap(row);
    row = "x";
    let text: String = UnwrapOption(wrapped);
    ArrayPop(names);
    Log(text);
    return ArrayLength(names);
}
func Main() -> Void { Log(Drain(1)); }
PGY

# The top of an inout stack is read after it is popped.
cat >"$WORK_DIR/inout_formal_top_pop.pgy" <<'PGY'
func Drop(inout stack: Array<String>) -> Int {
    let top: String = stack[ArrayLength(stack) - 1];
    ArrayPop(stack);
    Log(top);
    return ArrayLength(stack);
}
func Main() -> Void {
    var stack: Array<String> = [];
    ArrayPush(stack, ToString(7));
    ArrayPush(stack, ToString(8));
    Log(Drop(stack));
}
PGY

# Neighbours: the loan is read before the pop; a fresh copy outlives it.
cat >"$WORK_DIR/pop_after_last_read.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    ArrayPush(names, ToString(seed + 2));
    var n: Int = 0;
    while ArrayLength(names) > 0 {
        let name: String = names[ArrayLength(names) - 1];
        Log(name);
        ArrayPop(names);
        n = n + 1;
    }
    return n;
}
func Main() -> Void { Log(Drain(1)); }
PGY

cat >"$WORK_DIR/pop_fresh_copy.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var n: Int = 0;
    while ArrayLength(names) > 0 {
        let name: String = Concat(names[ArrayLength(names) - 1], "");
        ArrayPop(names);
        Log(name);
        n = n + 1;
    }
    return n;
}
func Main() -> Void { Log(Drain(1)); }
PGY

# More neighbours: a pop of another array, an owned set whose stored value
# reads the loan before the replaced element is freed, and a loan read
# through a `ref` formal while an `inout` formal is set and a local popped.
cat >"$WORK_DIR/other_array_pop.pgy" <<'PGY'
func Drain(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    var other: Array<String> = [];
    ArrayPush(other, ToString(seed + 5));
    let kept: String = names[0];
    ArrayPop(other);
    Log(kept);
    return ArrayLength(other);
}
func Main() -> Void { Log(Drain(1)); }
PGY

cat >"$WORK_DIR/set_reads_loan.pgy" <<'PGY'
func Swap(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    let name: String = names[0];
    ArraySet(names, 0, Concat(name, "x"));
    Log(names[0]);
    return ArrayLength(names);
}
func Main() -> Void { Log(Swap(5)); }
PGY

cat >"$WORK_DIR/ref_formal_loan.pgy" <<'PGY'
func Fill(ref labels: Array<String>, inout out: Array<String>) -> Int {
    let owner: String = labels[0];
    var names: Array<String> = [];
    ArrayPush(names, ToString(1));
    var i: Int = 0;
    while i < ArrayLength(out) {
        ArraySet(out, i, Concat(owner, ToString(i)));
        if ArrayLength(names) > 0 { ArrayPop(names); }
        i = i + 1;
    }
    Log(owner);
    return ArrayLength(out);
}
func Main() -> Void {
    let labels: Array<String> = ["x", "y"];
    var out: Array<String> = [];
    ArrayPush(out, ToString(7));
    ArrayPush(out, ToString(8));
    Log(Fill(labels, out));
    Log(out[0]);
    Log(out[1]);
}
PGY

# A String field of a record built from a loan is an earlier argument of the
# call that hands the array to an `own` callee that drops it. The compiler
# lands a Clone where that argument is taken, so the release frees nothing it
# reads.
cat >"$WORK_DIR/copied_argument_before_release.pgy" <<'PGY'
struct TextRecord { text: String; }
func OwnedRows() -> Array<String> {
    var rows: Array<String> = [];
    ArrayPushOwnedString(rows, Concat("owned", " value"));
    return rows;
}
func MakeRecord(text: String) -> TextRecord { return TextRecord(text); }
func Retire(own rows: Array<String>) -> Int {
    ArrayDropOwnedStrings(rows);
    return 64;
}
func Inspect(text: String, count: Int) -> Int { return StringLength(text) + count; }
func Main() -> Void {
    let rows: Array<String> = OwnedRows();
    let record: TextRecord = MakeRecord(rows[0]);
    Log(Inspect(record.text, Retire(rows)));
}
PGY

# A `for` binding over an Array<String> local borrows the element it reads
# (decision 2026-11-20_foreach_string_binding_lends_its_element). Pushed into
# another Array, the loan goes with it; the owned set frees the element, and
# out[0] is read. Native copies at the push and prints 5 99 2; before
# 2026-11-24 the self-host front end recorded no loan for the binding and both
# default routes printed freed memory.
cat >"$WORK_DIR/foreach_pushed_then_set.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var out: Array<String> = [];
    for name in names {
        ArrayPush(out, name);
    }
    ArraySet(names, 0, ToString(99));
    Log(out[0]);
    return ArrayLength(out);
}
func Main() -> Void { Log(Gather(5)); }
PGY

# The binding kept in an outer String (native refuses it too).
cat >"$WORK_DIR/foreach_kept_then_set.pgy" <<'PGY'
func Keep(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var kept: String = "";
    for name in names {
        kept = name;
    }
    ArraySet(names, 1, ToString(99));
    Log(kept);
    return 1;
}
func Main() -> Void { Log(Keep(5)); }
PGY

# Reassigning an Array<String> local releases the storage it held, so a loan
# of that storage read afterwards reads freed memory (both default routes
# before 2026-11-24, for an index loan and for a pushed foreach binding).
cat >"$WORK_DIR/reassigned_under_loan.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    let first: String = names[0];
    names = [];
    ArrayPush(names, ToString(7));
    Log(first);
    return ArrayLength(names);
}
func Main() -> Void { Log(Gather(5)); }
PGY

cat >"$WORK_DIR/foreach_pushed_then_reassigned.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    var out: Array<String> = [];
    for name in names {
        ArrayPush(out, name);
    }
    names = [];
    Log(out[0]);
    return ArrayLength(out);
}
func Main() -> Void { Log(Gather(5)); }
PGY

# Neighbours: no release after the push, a reassignment after the last read,
# and a reassignment of another array.
cat >"$WORK_DIR/foreach_pushed_kept.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    ArrayPush(names, ToString(seed + 1));
    var out: Array<String> = [];
    for name in names {
        ArrayPush(out, name);
    }
    Log(out[0]);
    Log(names[1]);
    return ArrayLength(out);
}
func Main() -> Void { Log(Gather(5)); }
PGY

cat >"$WORK_DIR/read_then_reassigned.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    let first: String = names[0];
    Log(first);
    names = [];
    ArrayPush(names, ToString(7));
    Log(names[0]);
    return ArrayLength(names);
}
func Main() -> Void { Log(Gather(5)); }
PGY

cat >"$WORK_DIR/other_array_reassigned.pgy" <<'PGY'
func Gather(seed: Int) -> Int {
    var names: Array<String> = [];
    ArrayPush(names, ToString(seed));
    var other: Array<String> = [];
    ArrayPush(other, ToString(8));
    let first: String = names[0];
    other = [];
    Log(first);
    return ArrayLength(other);
}
func Main() -> Void { Log(Gather(5)); }
PGY

DEFAULT_LEGS=(default-c default-llvm)
flags_for() {
    case "$1" in
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $1" ;;
    esac
}

# refused NAME BOUNDARY: the front end refuses NAME with BOUNDARY on the
# direct MIR producer and on both default routes, and writes no artifact.
# The self-host driver prints its diagnostic on stdout, so both streams are
# read.
refused() {
    local name="$1" boundary="$2" leg log out_rel status
    log="$WORK_DIR/$name.mir.log"
    status=0
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
        -o "$WORK_REL/$name.mir.json") >"$log" 2>&1 || status=$?
    [[ "$status" != 0 ]] || fail "the MIR producer admitted $name"
    [[ ! -e "$WORK_DIR/$name.mir.json" ]] || fail "the MIR producer wrote MIR for $name"
    tr -d '\r' <"$log" | grep -Fxq "Code: borrow_boundary_escape" &&
        tr -d '\r' <"$log" | grep -Fq -- "$boundary" ||
        { cat "$log" >&2; fail "the MIR producer refused $name without $boundary"; }
    for leg in "${DEFAULT_LEGS[@]}"; do
        flags_for "$leg"
        out_rel="$WORK_REL/$name-$leg.exe"
        log="$WORK_DIR/$name-$leg.log"
        if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" "${flags[@]}" -o "$out_rel") >"$log" 2>&1; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg published an artifact for $name"
        tr -d '\r' <"$log" | grep -Fxq "Code: borrow_boundary_escape" &&
            tr -d '\r' <"$log" | grep -Fq -- "$boundary" ||
            { cat "$log" >&2; fail "$leg refused $name without $boundary"; }
    done
}

# admitted NAME STDOUT: both default routes print STDOUT; the general
# emitter's C, built with ASan, prints it with no freed-memory access. The
# general emitter does not retire every temporary yet, so leak detection
# stays off.
admitted() {
    local name="$1" expected="$2" leg status
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    for leg in "${DEFAULT_LEGS[@]}"; do
        flags_for "$leg"
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" "${flags[@]}" -o "$WORK_REL/$name-$leg.exe") \
            >"$WORK_DIR/$name-$leg.log" 2>&1 ||
            { cat "$WORK_DIR/$name-$leg.log" >&2; fail "$leg refused $name"; }
        status=0
        "$WORK_DIR/$name-$leg.exe" </dev/null 2>"$WORK_DIR/$name-$leg.err" |
            tr -d '\r' >"$WORK_DIR/$name-$leg.out" || status=$?
        [[ "$status" == 0 ]] || fail "$leg $name exited $status"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg $name printed other output"; }
    done
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
            -o "$WORK_REL/$name.mir.json" &&
        "$DRIVER" --mir-json-backend=c "$WORK_REL/$name.mir.json" -o "$WORK_REL/$name-direct.c" &&
        ! grep -Fq 'CODEGEN ERROR' "$WORK_REL/$name-direct.c" &&
        "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread "$WORK_REL/$name-direct.c" \
            -o "$WORK_REL/$name-asan.exe" -lm) >"$WORK_DIR/$name-direct.log" 2>&1 ||
        { cat "$WORK_DIR/$name-direct.log" >&2; fail "$name did not build on the general emitter with ASan"; }
    status=0
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$WORK_DIR/$name-asan.exe" </dev/null \
        2>"$WORK_DIR/$name-asan.err" | tr -d '\r' >"$WORK_DIR/$name-asan.out" || status=$?
    [[ "$status" == 0 ]] || { cat "$WORK_DIR/$name-asan.err" >&2; fail "$name failed under ASan"; }
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-asan.out" ||
        fail "$name printed other output under ASan"
}

# copied_where_taken NAME STDOUT: the front end admits NAME, and the default C
# route, which carries the landed Clone, prints STDOUT under ASan with no
# freed-memory access. The direct MIR routes refuse a landed Clone they
# cannot carry.
copied_where_taken() {
    local name="$1" expected="$2" status=0
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
        -o "$WORK_REL/$name.mir.json") >"$WORK_DIR/$name.mir.log" 2>&1 ||
        { cat "$WORK_DIR/$name.mir.log" >&2; fail "the MIR producer refused $name"; }
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" --emit-c -o "$WORK_REL/$name-default.c" &&
        "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/runtime -pthread "$WORK_REL/$name-default.c" \
            -o "$WORK_REL/$name-default-asan.exe" -lm) >"$WORK_DIR/$name-default.log" 2>&1 ||
        { cat "$WORK_DIR/$name-default.log" >&2; fail "$name did not build on the default C route with ASan"; }
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$WORK_DIR/$name-default-asan.exe" </dev/null \
        2>"$WORK_DIR/$name-default-asan.err" | tr -d '\r' >"$WORK_DIR/$name-default-asan.out" || status=$?
    [[ "$status" == 0 ]] || { cat "$WORK_DIR/$name-default-asan.err" >&2; fail "$name failed under ASan"; }
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-default-asan.out" ||
        fail "$name printed other output under ASan"
}

refused pop_index_alias indexed_string_use_after_element_release
refused set_index_alias indexed_string_use_after_element_release
refused inout_callee_pop indexed_string_use_after_element_release
refused argument_before_release indexed_string_use_after_element_release
refused returned_loan_pop indexed_string_use_after_element_release
refused joined_local_result_pop indexed_string_use_after_element_release
refused rebound_local_result_pop indexed_string_use_after_element_release
refused reassigned_option_pop indexed_string_use_after_element_release
refused unfixed_argument_option_pop indexed_string_use_after_element_release
refused inout_formal_top_pop indexed_string_use_after_element_release
refused pop_loop_carried indexed_string_loop_use_after_element_release
refused pop_rebound_loop indexed_string_loop_use_after_element_release
refused foreach_pushed_then_set indexed_string_use_after_element_release
refused foreach_kept_then_set indexed_string_use_after_element_release
refused reassigned_under_loan indexed_string_use_after_deep_drop
refused foreach_pushed_then_reassigned indexed_string_use_after_deep_drop
admitted pop_after_last_read $'3\n2\n1\n3'
admitted pop_fresh_copy $'2\n1\n2'
admitted fresh_result_pop $'22\n1'
admitted unwrapped_option_set $'6!\n1'
admitted other_array_pop $'1\n0'
admitted set_reads_loan $'5x\n2'
admitted ref_formal_loan $'x\n2\nx0\nx1'
admitted foreach_pushed_kept $'5\n6\n2'
admitted read_then_reassigned $'5\n7\n1'
admitted other_array_reassigned $'5\n0'
copied_where_taken copied_argument_before_release '75'

echo "[$LABEL] 16 element releases or reassignments under a live String loan refused on the MIR producer and both default routes; 10 neighbours equal on both routes and clean under ASan; a loan copied where it is taken admitted: PASS"
