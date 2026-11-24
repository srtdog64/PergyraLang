#!/usr/bin/env bash
# The installed self-host driver's default C and LLVM routes must run every
# program below the way native does, or refuse it. Each program was admitted
# by the default LLVM route with output unlike native's on 2026-11-19
# (docs/truth/evidence/2026-11-19_scalar_cfg_mutable_declaration.md):
# - Log of a String: the direct LLVM emitters always added a line break; the
#   runtime adds one only when the text does not end with one, and a literal
#   with a line break is normalized as a banner (log_trailing_newline);
# - an owned String set stored the borrowed literal and the deep drop freed
#   it (array_result_owned_named_inout_shallow_set: abort);
# - a by-value Array formal handed to an inout callee was read from its entry
#   value, not from the slot the callee wrote (grown_by_inout_callee_last_use);
# - a compiler Clone the source analysis selected at a binding's initializer
#   had no MIR carrier and was dropped (copy_then_set);
# - ArraySort, ArrayReverse and an inout callee's ArraySet write in place but
#   selected no Clone, so a copy changed with its source (four *_landed_valid
#   programs, reverse_after_copy and inout_set_after_copy);
# - a borrowed String element pushed without a copy outlived the deep drop of
#   its Array (named_string_identity_loop_reassignment, pushed_element_dropped).
# On 2026-11-24 default C ran four more programs with other output, and LLVM
# refused them only at an unrelated route check: an indexed assignment was no
# write to the storage a copy shared, an ArraySet through a copy's path left
# the copy unfollowed, and a write through a by-value formal's member path
# did not move the formal (local_copy_source_element_set,
# by_value_lend_result_written_argument_copied and the two
# default_aggregate_move_field_* programs). The inline programs below the
# 2026-11-24 line hold the same writes on smaller values; a write through a
# copy handed to a call (copy_passed_then_index_set) may be refused.
# MUST_EQUAL programs must compile and print native's output on both default
# routes. For the others a leg may refuse (no artifact) but never run with
# other output; on the drivers before this gate each ran with other output.
# by_value_cursor_set_through_recursive_parser keeps native's sharing: a
# by-value Array formal handed whole to an inout callee that only sets an
# element is no move (OPEN 2026-10-23_open_by_value_array_element_write_through_inout),
# and the self-hosted parser's cursor relies on it. inout_result_holds_argument
# keeps the result of an inout callee that holds its argument: the copy that
# result makes is formed after the callee writes, so no Clone lands at the
# inout argument (a C build of the compiler's own mir_lower failed on that).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-native-runtime-parity"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_route_native_runtime_parity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CASES="tests/cases/function_param_flow_summary"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/copy_then_set.pgy" <<'PGY'
func Main() -> Int {
    var items: Array<Int> = [3, 1, 2];
    let before: Array<Int> = items;
    ArraySet(items, 0, 7);
    Print(ToString(before[0] * 10 + items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/reverse_after_copy.pgy" <<'PGY'
func Main() -> Int {
    var items: Array<Int> = [3, 1, 2];
    let before: Array<Int> = items;
    ArrayReverse(items);
    Print(ToString(before[0] * 10 + items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/pushed_element_dropped.pgy" <<'PGY'
func OwnedRows() -> Array<String> {
    var rows: Array<String> = [];
    ArrayPushOwnedString(rows, Concat("owned", " value"));
    return rows;
}

func Main() -> Void {
    let owned: Array<String> = OwnedRows();
    var copies: Array<String> = [];
    let text: String = owned[0];
    ArrayPush(copies, text);
    ArrayDropOwnedStrings(owned);
    Log(copies[0]);
}
PGY

cat >"$WORK_DIR/inout_set_after_copy.pgy" <<'PGY'
func Rewrite(inout items: Array<Int>) -> Void {
    ArraySet(items, 0, 9);
}

func Main() -> Int {
    var a: Array<Int> = [1, 2];
    let b: Array<Int> = a;
    Rewrite(a);
    Print(ToString(b[0] * 10 + a[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/inout_result_holds_argument.pgy" <<'PGY'
struct Query {
    ok: Bool;
    nodes: Array<Int>;
}

func QueryReady(nodes: Array<Int>) -> Query {
    return Query(true, nodes);
}

func Validate(inout nodes: Array<Int>) -> Query {
    ArrayPush(nodes, 1);
    return QueryReady(nodes);
}

func Main() -> Int {
    var nodes: Array<Int> = [];
    var i: Int = 0;
    while i < 2 {
        let query: Query = Validate(nodes);
        if !query.ok { return 1; }
        i = i + 1;
    }
    Print(ToString(ArrayLength(nodes)));
    return 0;
}
PGY

# 2026-11-24: element writes through copies, paths and formals.
cat >"$WORK_DIR/copy_then_index_set.pgy" <<'PGY'
func Main() -> Int {
    var items: Array<Int> = [1, 2];
    let before: Array<Int> = items;
    items[0] = 5;
    Print(ToString(before[0] * 10 + items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/loop_copy_then_index_set.pgy" <<'PGY'
func Main() -> Int {
    var items: Array<Int> = [1, 2];
    var i: Int = 0;
    var total: Int = 0;
    while i < 2 {
        let snap: Array<Int> = items;
        items[0] = items[0] + 10;
        total = total + snap[0];
        i = i + 1;
    }
    Print(ToString(total * 100 + items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/inout_index_set_after_copy.pgy" <<'PGY'
func Mark(inout rows: Array<Int>) -> Int {
    rows[0] = 9;
    return 0;
}

func Main() -> Int {
    var rows: Array<Int> = [1];
    let snap: Array<Int> = rows;
    let n: Int = Mark(rows);
    Print(ToString(snap[0] * 10 + rows[0]));
    return n;
}
PGY

cat >"$WORK_DIR/record_copy_side_set.pgy" <<'PGY'
struct Box {
    items: Array<Int>;
}

func Main() -> Int {
    let box: Box = Box([1, 2]);
    var held: Box = box;
    ArraySet(held.items, 0, 9);
    Print(ToString(box.items[0] * 10 + held.items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/formal_field_index_set.pgy" <<'PGY'
struct Arena {
    atoms: Array<Int>;
}

func Rename(arena: Arena) -> Int {
    arena.atoms[0] = 9;
    return ArrayLength(arena.atoms);
}

func Main() -> Int {
    let arena: Arena = Arena([1]);
    let count: Int = Rename(arena);
    Print(ToString(arena.atoms[0]));
    return count;
}
PGY

cat >"$WORK_DIR/copy_passed_then_index_set.pgy" <<'PGY'
func Sum(items: Array<Int>) -> Int {
    return items[0] + items[1];
}

func Main() -> Int {
    let items: Array<Int> = [1, 2];
    var held: Array<Int> = items;
    Print(ToString(Sum(held)));
    held[0] = 9;
    Print(ToString(items[0] * 10 + held[0]));
    return 0;
}
PGY

MUST_EQUAL=(
    "src/self_hosted/codegen/fixture/log_trailing_newline.pgy"
    "$WORK_REL/copy_then_index_set.pgy"
    "$WORK_REL/loop_copy_then_index_set.pgy"
    "$WORK_REL/inout_index_set_after_copy.pgy"
    "$CASES/array_result_owned_named_inout_shallow_set_landed_valid.pgy"
    "$CASES/by_value_array_grown_by_inout_callee_last_use_valid.pgy"
    "$CASES/by_value_cursor_set_through_recursive_parser_valid.pgy"
    "$WORK_REL/inout_result_holds_argument.pgy"
)
EQUAL_OR_REFUSED=(
    "$CASES/named_string_identity_loop_reassignment_landed_valid.pgy"
    "$CASES/by_value_argument_before_its_inout_landed_valid.pgy"
    "$CASES/by_value_formal_array_sorted_landed_valid.pgy"
    "$CASES/local_copy_source_sorted_landed_valid.pgy"
    "$CASES/by_value_field_alias_written_by_inout_callee_landed_valid.pgy"
    "$WORK_REL/copy_then_set.pgy"
    "$WORK_REL/reverse_after_copy.pgy"
    "$WORK_REL/pushed_element_dropped.pgy"
    "$WORK_REL/inout_set_after_copy.pgy"
    "$CASES/local_copy_source_element_set_landed_valid.pgy"
    "$CASES/by_value_lend_result_written_argument_copied_landed_valid.pgy"
    "$CASES/default_aggregate_move_field_element_write_landed_valid.pgy"
    "$CASES/default_aggregate_move_field_set_landed_valid.pgy"
    "$WORK_REL/record_copy_side_set.pgy"
    "$WORK_REL/formal_field_index_set.pgy"
    "$WORK_REL/copy_passed_then_index_set.pgy"
)

# run_leg SOURCE LEG FLAGS...: compiles SOURCE and runs it. Prints "refused"
# when the compiler exits nonzero without an executable, else the program's
# stdout and exit status. A compiler that succeeds without an executable fails.
run_leg() {
    local source="$1" leg="$2" stem status
    shift 2
    stem="$WORK_DIR/$(basename "$source" .pgy)-$leg"
    status=0
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" "$source" "$@" -o "$stem.exe") \
        >"$stem.compile.log" 2>&1 || status=$?
    if [[ "$status" != 0 ]]; then
        [[ ! -e "$stem.exe" ]] || fail "$leg left an executable after refusing $source"
        echo "refused"
        return
    fi
    [[ -x "$stem.exe" ]] || fail "$leg reported success without an executable for $source"
    status=0
    "$stem.exe" >"$stem.out" 2>"$stem.err" </dev/null || status=$?
    printf 'exit=%s\n' "$status" >>"$stem.out"
    tr -d '\r' <"$stem.out" | od -An -c
}

check() {
    local source="$1" must_equal="$2" native leg got refusals=0
    [[ -f "$ROOT_DIR/$source" ]] || fail "missing program $source"
    native="$(run_leg "$source" native-llvm --native-pipeline --backend=llvm)"
    [[ "$native" != refused ]] || fail "native refused $source"
    for leg in default-c default-llvm; do
        if [[ "$leg" == default-c ]]; then got="$(run_leg "$source" "$leg" --backend=c)";
        else got="$(run_leg "$source" "$leg" --backend=llvm)"; fi
        if [[ "$got" == refused ]]; then
            [[ "$must_equal" == 0 ]] ||
                { cat "$WORK_DIR/$(basename "$source" .pgy)-$leg.compile.log" >&2; fail "$leg refused $source"; }
            refusals=$((refusals + 1))
            continue
        fi
        [[ "$got" == "$native" ]] ||
            fail "$leg ran $source unlike native: native [$native] $leg [$got]"
    done
    echo "[$LABEL] ok $source (refused legs: $refusals)"
}

for source in "${MUST_EQUAL[@]}"; do check "$source" 1; done
for source in "${EQUAL_OR_REFUSED[@]}"; do check "$source" 0; done
echo "[$LABEL] PASS: ${#MUST_EQUAL[@]} equal on both default routes, ${#EQUAL_OR_REFUSED[@]} equal or refused"
