#!/usr/bin/env bash
# Coverage the general direct-MIR emitter gained for the compiler's own
# sources (track DM1 of docs/truth/evidence/2026-11-16_direct_mir_drv2_refusal_census.md):
# - a HashMap read (MapHas/MapGet/MapSize/MapKeys) whose receiver is a record
#   member reads through the member's storage;
# - a non-Void routine may end by calling a routine that never returns;
# - a program with HashMap locals may have an Int Main and Exit sites: each
#   map is released on every return, and Exit leaves the live maps to the
#   operating system as native Exit does.
# Each positive runs on native C/LLVM, the default C/LLVM routes and the
# general emitter's C backend with the same stdout and exit status; the
# default LLVM route and the C backend refused all of them before. Each
# negative neighbour stays refused by the default LLVM route while native
# admits it:
# - a HashMap write through a member of a by-value formal (the read path's
#   header copy would lose the write);
# - a map alias in an Int Main with Exit (the alias proof still holds);
# and one MIR mutation: a Never callee that returns normally.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="direct-mir-general-emitter-coverage"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON="${PYTHON_BIN:-python3}"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_coverage_mutations.py"
WORK_REL=".tmp/self_hosted/direct_mir_general_emitter_coverage"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CC="${PGY_SELFHOST_CC:-gcc}"
LEGS=(native-c native-llvm default-c default-llvm direct-c)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/map_member_read.pgy" <<'PGY'
struct Table {
    counts: HashMap<String, Int>;
    ids: HashMap<Int, Int>;
}

struct Holder {
    table: Table;
    width: Int;
}

func MakeTable() -> Table {
    var counts: HashMap<String, Int> = MapNew();
    MapSet(counts, "a", 4);
    MapSet(counts, "b", 9);
    var ids: HashMap<Int, Int> = MapNew();
    MapSet(ids, 7, 1);
    MapSet(ids, 3, 2);
    return Table(counts, ids);
}

func Count(table: Table, key: String) -> Int {
    if MapHas(table.counts, key) {
        return MapGet(table.counts, key);
    }
    return MapSize(table.counts);
}

func NestedHas(ref holder: Holder, key: String) -> Bool {
    return MapHas(holder.table.counts, key);
}

func IdKeys(ref table: Table) -> Int {
    let keys: Array<Int> = MapKeys(table.ids);
    return ArrayLength(keys) * 100 + keys[0];
}

func Main() -> Int {
    let holder: Holder = Holder(MakeTable(), 2);
    Log(Count(holder.table, "a"));
    Log(Count(holder.table, "z"));
    Log(NestedHas(holder, "b"));
    Log(NestedHas(holder, "q"));
    Log(MapGet(holder.table.counts, "b") + holder.width);
    Log(IdKeys(holder.table));
    Log(MapHas(holder.table.ids, 7));
    return 0;
}
PGY

cat >"$WORK_DIR/map_member_write.pgy" <<'PGY'
struct Table {
    counts: HashMap<String, Int>;
}

func Bump(table: Table, key: String) -> Table {
    MapSet(table.counts, key, 7);
    return table;
}

func Main() -> Int {
    var counts: HashMap<String, Int> = MapNew();
    MapSet(counts, "a", 4);
    let table: Table = Bump(Table(counts), "c");
    Log(MapGet(table.counts, "c"));
    return 0;
}
PGY

cat >"$WORK_DIR/never_tail.pgy" <<'PGY'
func Fail(message: String) -> Never {
    Log(message);
    Exit(3);
}

func Note(message: String) -> Void {
    Log(message);
    return;
}

func Name(code: Int) -> String {
    if code == 1 { return "one"; }
    if code == 2 { return "two"; }
    Fail("unknown code");
}

func Twice(code: Int) -> Int {
    if code < 10 { return code * 2; }
    Fail("too large");
}

func Main() -> Int {
    Note("start");
    Log(Name(1));
    Log(Name(2));
    Log(Twice(4));
    Log(Name(7));
    return 0;
}
PGY

cat >"$WORK_DIR/never_wrapper.pgy" <<'PGY'
func Fail(message: String) -> Never {
    Log(message);
    Exit(4);
}

func FailWith(prefix: String, code: Int) -> Never {
    Fail(Concat(prefix, ToString(code)));
}

func Check(code: Int) -> Int {
    if code > 0 { return code; }
    FailWith("bad:", code);
}

func Main() -> Int {
    Log(Check(5));
    Log(Check(0));
    return 0;
}
PGY

cat >"$WORK_DIR/map_exit.pgy" <<'PGY'
func Lookup(ref counts: HashMap<String, Int>, key: String) -> Int {
    if MapHas(counts, key) { return MapGet(counts, key); }
    Log("missing");
    Exit(2);
}

func Main() -> Int {
    var counts: HashMap<String, Int> = MapNew();
    MapSet(counts, "a", 4);
    Log(Lookup(counts, "a"));
    var other: HashMap<String, Int> = MapNew();
    MapSet(other, "b", 1);
    if MapSize(other) > 5 { Exit(9); }
    Log(MapSize(counts) + MapSize(other));
    Log(Lookup(counts, "z"));
    return 0;
}
PGY

cat >"$WORK_DIR/map_int_main.pgy" <<'PGY'
func Main() -> Int {
    var counts: HashMap<String, Int> = MapNew();
    MapSet(counts, "a", 4);
    MapSet(counts, "b", 5);
    Log(MapGet(counts, "b"));
    return MapSize(counts);
}
PGY

cat >"$WORK_DIR/map_alias_exit.pgy" <<'PGY'
func Main() -> Int {
    let values: HashMap<String, Int> = MapNew();
    let alias: HashMap<String, Int> = values;
    Log(MapSize(alias));
    if MapSize(alias) > 3 { Exit(4); }
    return 0;
}
PGY

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# compile NAME LEG: builds WORK_DIR/NAME-LEG.exe, returns the compiler status.
# The direct-c leg is the general emitter's C backend: the self-host MIR of
# NAME projected with --mir-json-backend=c and built with the C compiler.
compile() {
    local name="$1" leg="$2" status=0
    if [[ "$leg" == direct-c ]]; then
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy" \
                -o "$WORK_REL/$name.direct.mir.json" &&
            "$DRIVER" --mir-json-backend=c "$WORK_REL/$name.direct.mir.json" \
                -o "$WORK_REL/$name-direct.c" &&
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

# expect_run NAME STDOUT STATUS: every leg prints STDOUT and exits STATUS.
expect_run() {
    local name="$1" expected_out="$2" expected_status="$3" leg status
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        compile "$name" "$leg" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2; fail "$leg did not compile $name"; }
        status=0
        "$WORK_DIR/$name-$leg.exe" >"$WORK_DIR/$name-$leg.out" \
            2>"$WORK_DIR/$name-$leg.err" </dev/null || status=$?
        [[ "$status" == "$expected_status" ]] ||
            fail "$leg $name exited $status instead of $expected_status"
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name printed other output"; }
    done
}

# expect_direct_refusal NAME NATIVE_STDOUT: native LLVM runs NAME, and the
# default LLVM route refuses it with a CODEGEN ERROR and writes no program.
expect_direct_refusal() {
    local name="$1" native_out="$2"
    compile "$name" native-llvm ||
        { cat "$WORK_DIR/$name-native-llvm.compile.log" >&2; fail "native LLVM refused $name"; }
    "$WORK_DIR/$name-native-llvm.exe" </dev/null | tr -d '\r' >"$WORK_DIR/$name-native-llvm.out" ||
        fail "native LLVM $name failed"
    printf '%s\n' "$native_out" | cmp -s - "$WORK_DIR/$name-native-llvm.out" ||
        fail "native LLVM $name printed other output"
    if compile "$name" default-llvm; then
        fail "default LLVM admitted $name"
    fi
    [[ ! -e "$WORK_DIR/$name-default-llvm.exe" ]] ||
        fail "default LLVM wrote a program for refused $name"
    grep -Fq "CODEGEN ERROR" "$WORK_DIR/$name-default-llvm.compile.log" ||
        { cat "$WORK_DIR/$name-default-llvm.compile.log" >&2
          fail "default LLVM refused $name without a direct MIR refusal"; }
}

# expect_mutation_refused NAME MUTATION REASON: the direct MIR LLVM route
# admits NAME's MIR as produced and refuses the mutated MIR naming REASON.
expect_mutation_refused() {
    local name="$1" mutation="$2" reason="$3"
    local mir_rel="$WORK_REL/$name.mir.json" mutated_rel="$WORK_REL/$name.$mutation.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$name.pgy") \
        >"$ROOT_DIR/$mir_rel" 2>"$WORK_DIR/$name.mir.err" || fail "$name MIR production failed"
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm "$mir_rel" \
        -o "$WORK_REL/$name.control.ll") >"$WORK_DIR/$name.control.out" 2>&1 ||
        { cat "$WORK_DIR/$name.control.out" >&2; fail "direct MIR LLVM refused unmutated $name"; }
    "$PYTHON" "$MUTATIONS" "$ROOT_DIR/$mir_rel" "$mutation" "$ROOT_DIR/$mutated_rel" ||
        fail "$mutation mutation failed"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm "$mutated_rel" \
            -o "$WORK_REL/$name.$mutation.ll") >"$WORK_DIR/$name.$mutation.out" 2>&1; then
        fail "direct MIR LLVM admitted $mutation"
    fi
    [[ ! -s "$WORK_DIR/$name.$mutation.ll" ]] || fail "$mutation left an artifact"
    grep -Fq "$reason" "$WORK_DIR/$name.$mutation.out" ||
        { cat "$WORK_DIR/$name.$mutation.out" >&2; fail "$mutation refused without $reason"; }
}

expect_run map_member_read $'4\n2\ntrue\nfalse\n11\n203\ntrue' 0
expect_run never_tail $'start\none\ntwo\n8\nunknown code' 3
expect_run never_wrapper $'5\nbad:0' 4
expect_run map_exit $'4\n2\nmissing' 2
expect_run map_int_main '5' 2

expect_direct_refusal map_member_write '7'
expect_direct_refusal map_alias_exit '0'

expect_mutation_refused never_tail never-callee-returns "terminal return is invalid"

echo "[$LABEL] 5 programs equal on five legs; 2 neighbours and 1 MIR mutation refused: PASS"
