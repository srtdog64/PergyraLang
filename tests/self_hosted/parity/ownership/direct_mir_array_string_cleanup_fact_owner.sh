#!/usr/bin/env bash
# A direct-MIR Array<String> local is cleaned up only from its admitted
# collection ownership row or a runtime producer, and only while no other
# holder has its storage. A local whose storage and elements the semantic fact
# leaves unknown (a call result, an alias of a formal), a local handed to a
# callee that grows it without a hand-off fact (here through an alias of the
# formal, or again on every loop iteration), a local given to two records or
# to a record in a loop that does not declare it, and an alias of an inout
# member that grows in place, and a record lent to a ref formal beside an
# inout output that may keep its Strings are refused with no artifact; none is
# released from its operation shape. A local the semantic verdict hands to a record
# (decision 2026-11-15) is released once, by that record, and runs. The native
# semantic issues the same hand-off, so native MIR of that program runs on the
# --mir-json route too; with the hand-off removed, or forged onto a member move,
# the route refuses it. A callee that reads a member of a record it is given
# by value releases nothing, so the caller's record releases it once.
# A record or call result is the only owner of the locals it takes, of any
# collection type: a local it takes that is also returned or taken by another
# record, or taken again on each pass of a loop that defines it outside, is
# refused (the old driver released that storage twice). Admitted programs run
# under glibc heap checks, so a second release aborts them.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-array-string-cleanup"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
NO_ROW="direct MIR Array<String> cleanup requires an admitted ownership row"
SHARED="direct MIR Array<String> cleanup refuses storage another holder still has"
MEMBER_ALIAS="direct MIR Array<String> alias of a parameter member grows storage the member still names"
NO_HANDOFF="direct MIR Array<String> constructor input has no hand-off fact"
LENT_INOUT="owned local lent to a ref formal is released while an inout argument of the same call may hold its Strings"
# The MIR reader refuses the routine whose member move claims a hand-off.
FORGED_HANDOFF="direct MIR compile-time declaration erasure declaration schema is invalid"
TAKEN="owned local taken by a record or call result is already held by another owner"
TAKEN_AGAIN="owned local taken by a record or call result is taken again on each pass of a loop"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
export PGY_SELF_DRIVER_BIN="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/direct_mir_array_string_cleanup.XXXXXX")"
WORK_REL=".tmp/self_hosted/${WORK_DIR##*/}"

# Borrowed literals pushed into storage that a call hands back: the caller's
# local has unknown elements.
cat >"$WORK_DIR/call_result.pgy" <<'EOF'
func Gather(old: Array<String>, extra: Array<String>) -> Array<String> {
    var uses: Array<String> = [];
    var i: Int = 0;
    while i < ArrayLength(extra) { ArrayPush(uses, extra[i]); i = i + 1; }
    var j: Int = 0;
    while j < ArrayLength(old) { ArrayPush(uses, old[j]); j = j + 1; }
    return uses;
}
func Main() -> Void {
    let old: Array<String> = ["x"];
    let extra: Array<String> = ["y", "z"];
    let uses: Array<String> = Gather(old, extra);
    Log(ArrayLength(uses));
}
EOF
# Locals that alias a ref formal own neither its storage nor its elements.
cat >"$WORK_DIR/formal_alias.pgy" <<'EOF'
func Matches(values: Array<String>) -> Bool { return values[0] == "borrowed"; }
func Observe(ref values: Array<String>) -> Bool {
    let first: Array<String> = values;
    let second: Array<String> = first;
    return Matches(first) && Matches(second) && ArrayLength(values) > 0;
}
func Main() -> Void { let values: Array<String> = ["borrowed"]; Log(Observe(values)); }
EOF
# The callee grows its parameter's storage in place; the caller's release
# would free it a second time.
cat >"$WORK_DIR/grown_handoff.pgy" <<'EOF'
func GrowCount(values: Array<String>) -> Int {
    var grown: Array<String> = values;
    ArrayPush(grown, "b"); ArrayPush(grown, "c"); ArrayPush(grown, "d"); ArrayPush(grown, "e");
    return ArrayLength(grown);
}
func Main() -> Void {
    let names: Array<String> = ["a"];
    Log(GrowCount(names));
}
EOF
# The record takes the local's storage and a member alias takes it back out.
# The verdict hands the local to the record, so it is released once.
cat >"$WORK_DIR/constructor_handoff.pgy" <<'EOF'
struct Pack { texts: Array<String>; }
func Main() -> Void {
    var texts: Array<String> = [];
    ArrayPushOwnedString(texts, Concat("owned", "-text"));
    let pack: Pack = Pack(texts);
    let moved: Array<String> = pack.texts;
    Log(ArrayLength(moved));
}
EOF
# The record takes the local's storage and a callee grows it past the
# literal's capacity. The local was handed to the record, so the caller does
# not release what realloc already freed.
cat >"$WORK_DIR/grown_record.pgy" <<'EOF'
struct NodeSet { valid: Bool; types: Array<String>; }
func NodeSetAttach(facts: NodeSet, name: String) -> NodeSet {
    var types: Array<String> = facts.types;
    ArrayPush(types, name); ArrayPush(types, name); ArrayPush(types, name);
    ArrayPush(types, name); ArrayPush(types, name);
    return NodeSet(facts.valid, types);
}
func Main() -> Int {
    let types: Array<String> = ["x"];
    var facts: NodeSet = NodeSet(true, types);
    facts = NodeSetAttach(facts, "y");
    return ArrayLength(facts.types);
}
EOF
# One local given to two records: no record takes it, and releasing it
# frees what both records still hold.
cat >"$WORK_DIR/constructor_twice.pgy" <<'EOF'
struct Rows { names: Array<String>; }
func Main() -> Void {
    var names: Array<String> = [];
    ArrayPushOwnedString(names, Concat("a", ""));
    let first: Rows = Rows(names);
    let second: Rows = Rows(names);
    Log(ArrayLength(first.names) + ArrayLength(second.names));
}
EOF
# A record built in a loop that does not declare the local takes the same
# storage on every iteration.
cat >"$WORK_DIR/constructor_loop.pgy" <<'EOF'
struct Rows { names: Array<String>; }
func Main() -> Void {
    let names: Array<String> = ["a"];
    var i: Int = 0;
    while i < 2 {
        let rows: Rows = Rows(names);
        Log(ArrayLength(rows.names));
        i = i + 1;
    }
}
EOF
# A growing callee called again on every iteration with the same local.
cat >"$WORK_DIR/grown_loop.pgy" <<'EOF'
func GrowCount(values: Array<String>) -> Int {
    var grown: Array<String> = values;
    ArrayPush(grown, "b"); ArrayPush(grown, "c"); ArrayPush(grown, "d"); ArrayPush(grown, "e");
    return ArrayLength(grown);
}
func Main() -> Void {
    let names: Array<String> = ["a"];
    var i: Int = 0;
    while i < 2 { Log(GrowCount(names)); i = i + 1; }
}
EOF
# A record whose owned Strings are lent to a ref formal while the same call
# writes an inout output: the callee stores borrowed Strings of the loan in
# that output, and the record's release would leave them dangling.
cat >"$WORK_DIR/lent_beside_inout.pgy" <<'EOF'
struct Index { labels: Array<String>; }
struct Out { names: Array<String>; }
func Lend(ref index: Index) -> Array<String> {
    var names: Array<String> = [];
    var i: Int = 0;
    while i < ArrayLength(index.labels) { ArrayPush(names, index.labels[i]); i = i + 1; }
    return names;
}
func Fill(ref index: Index, inout out: Out) -> Void {
    let lent: Array<String> = Lend(index);
    out.names = lent;
}
func Make(inout o: Out) -> Void {
    let index: Index = Index([Concat("a", "")]);
    Fill(index, o);
}
func Main() -> Int {
    let none: Array<String> = [];
    var o: Out = Out(none);
    Make(o);
    Log(o.names[0]);
    return 0;
}
EOF
# The alias of an inout member grows and is never stored back; the caller's
# member keeps the storage the growth released.
cat >"$WORK_DIR/member_alias.pgy" <<'EOF'
struct Rows { types: Array<String>; }
func Capture(inout rows: Rows) -> Void {
    var types: Array<String> = rows.types;
    var count: Int = 0;
    while count < 8 { ArrayPush(types, "row"); count = count + 1; }
}
func Main() -> Void {
    var rows: Rows = Rows(["seed"]);
    Capture(rows);
    Log(rows.types[0]);
}
EOF
# Neighbours whose ownership is proved still compile and run: a literal of
# borrowed elements, a local that owns pushed copies, a runtime Split result
# whose elements the runtime copied, and locals handed only to callees that
# read them (directly or through a further reading callee).
cat >"$WORK_DIR/rowed.pgy" <<'EOF'
func Count(values: Array<String>) -> Int { return ArrayLength(values); }
func First(values: Array<String>) -> String { return values[0]; }
func Forward(values: Array<String>) -> Int { return Count(values) + 1; }
func Main() -> Void {
    let names: Array<String> = ["a", "b"];
    Log(names[1]);
    var owned: Array<String> = [];
    ArrayPushOwnedString(owned, "c");
    ArrayPushOwnedString(owned, "d");
    Log(owned[0]);
    Log(ArrayLength(owned));
    let parts: Array<String> = Split("p,q", ",");
    Log(parts[1]);
    Log(Count(names));
    Log(First(names));
    Log(Forward(names));
    Log(names[0]);
}
EOF
printf 'b\nc\n2\nq\n2\na\n3\na\n' >"$WORK_DIR/rowed.expected"
# A record that takes a local's storage and only reads it leaves one release.
cat >"$WORK_DIR/record_observed.pgy" <<'EOF'
struct Carrier { values: Array<String>; count: Int; }
func Main() -> Void {
    let seed: Array<String> = ["owned", "kept"];
    let values: Array<String> = Clone(seed);
    let record: Carrier = Carrier(values, 2);
    Log(ArrayLength(values) + ArrayLength(record.values));
    Log(record.values[1]);
    Log(record.count);
}
EOF
printf '4\nkept\n2\n' >"$WORK_DIR/record_observed.expected"
# A callee reads a member of the record it is given by value. The caller's
# record releases that storage once; the callee's alias releases nothing (it
# used to release it too, which glibc aborts as a double free).
cat >"$WORK_DIR/formal_member_read.pgy" <<'EOF'
struct Bundle { value: Array<String>; }
struct Envelope { bundle: Bundle; }
func Count(envelope: Envelope) -> Int {
    let value: Array<String> = envelope.bundle.value;
    return ArrayLength(value);
}
func Main() -> Void {
    var value: Array<String> = [];
    ArrayPush(value, "borrowed");
    let bundle: Bundle = Bundle(value);
    let envelope: Envelope = Envelope(bundle);
    Log(Count(envelope));
}
EOF
printf '1\n' >"$WORK_DIR/formal_member_read.expected"
printf '1\n' >"$WORK_DIR/constructor_handoff.expected"
: >"$WORK_DIR/grown_record.expected"
# The Pair takes `first` and the function also returns it: the Pair's release
# and the caller's would free one Array<Int> storage twice.
cat >"$WORK_DIR/record_then_return.pgy" <<'EOF'
struct Pair { first: Array<Int>; second: Array<Int>; }
func Make() -> Array<Int> {
    let first: Array<Int> = [1];
    let second: Array<Int> = [2];
    let pair: Pair = Pair(first, second);
    Log(ArrayLength(pair.first));
    return first;
}
func Main() -> Void { let rows: Array<Int> = Make(); Log(ArrayLength(rows)); }
EOF
# Two records take one Array<Int> local; each would release it.
cat >"$WORK_DIR/record_twice.pgy" <<'EOF'
struct Rows { values: Array<Int>; }
func Main() -> Void {
    let values: Array<Int> = [1];
    let first: Rows = Rows(values);
    let second: Rows = Rows(values);
    Log(ArrayLength(first.values) + ArrayLength(second.values));
}
EOF
# A record in a loop takes a local defined before the loop on every pass; the
# replaced record releases the storage the next pass takes again.
cat >"$WORK_DIR/record_loop.pgy" <<'EOF'
struct Rows { values: Array<Int>; }
func Main() -> Void {
    let values: Array<Int> = [1];
    var i: Int = 0;
    while i < 2 {
        let rows: Rows = Rows(values);
        Log(ArrayLength(rows.values));
        i = i + 1;
    }
}
EOF
# Neighbours with one owner each: a record that a second record takes whole,
# a record whose local is also read, and a loop that defines the local anew
# before its record takes it.
cat >"$WORK_DIR/record_single_owner.pgy" <<'EOF'
struct Rows { values: Array<Int>; }
struct Outer { rows: Rows; }
func Main() -> Void {
    let values: Array<Int> = [1, 2];
    let rows: Rows = Rows(values);
    let outer: Outer = Outer(rows);
    Log(ArrayLength(outer.rows.values) + ArrayLength(values));
    var i: Int = 0;
    while i < 2 {
        let fresh: Array<Int> = [i, i, i];
        let again: Rows = Rows(fresh);
        Log(ArrayLength(again.values));
        i = i + 1;
    }
}
EOF
printf '4\n3\n3\n' >"$WORK_DIR/record_single_owner.expected"

refuse() {
    local name="$1" flag="$2" extension="$3" message="$4" rc
    local artifact="$WORK_REL/$name.$extension"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$flag" "$WORK_REL/$name.pgy" -o "$artifact") \
        >"$WORK_DIR/$name.$extension.out" 2>"$WORK_DIR/$name.$extension.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$name $flag cleaned up storage it does not own"
    [[ ! -e "$ROOT_DIR/$artifact" ]] || fail "$name $flag published an artifact"
    # The driver prints its CODEGEN ERROR on stdout; pgy adds its own line on stderr.
    grep -Fq "$message" "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err" ||
        fail "$name $flag was refused for another reason: $(head -3 "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err")"
}

for name in call_result formal_alias; do
    refuse "$name" --emit-c c "$NO_ROW"
    refuse "$name" --emit-llvm ll "$NO_ROW"
done
refuse grown_handoff --emit-c c "$SHARED"
refuse grown_handoff --emit-llvm ll "$SHARED"
refuse grown_loop --emit-c c "$SHARED"
# A program with a struct takes the round-trip C route, which copies instead;
# only the LLVM leg reaches the direct MIR cleanup.
refuse constructor_twice --emit-llvm ll "$NO_HANDOFF"
refuse constructor_loop --emit-llvm ll "$NO_HANDOFF"
refuse member_alias --emit-llvm ll "$MEMBER_ALIAS"
refuse lent_beside_inout --emit-llvm ll "$LENT_INOUT"
refuse record_then_return --emit-llvm ll "$TAKEN"
refuse record_twice --emit-llvm ll "$TAKEN"
refuse record_loop --emit-llvm ll "$TAKEN_AGAIN"

# Each admitted program runs on both legs with its stdout and exit status. The
# default profile builds the LLVM artifact at -O0, which keeps a second free
# in the program; glibc's heap checks then abort it.
heap_check=(env MALLOC_CHECK_=3)
for library in /lib/x86_64-linux-gnu/libc_malloc_debug.so.0 /lib64/libc_malloc_debug.so.0; do
    if [[ -e "$library" ]]; then heap_check+=("LD_PRELOAD=$library"); break; fi
done
run_admitted() {
    local name="$1" backend="$2" status="$3" rc
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$WORK_REL/$name.pgy" "--backend=$backend" \
        -o "$WORK_REL/$name-$backend.exe") >"$WORK_DIR/$name-$backend.log" 2>&1 ||
        fail "$name was refused on $backend: $(head -3 "$WORK_DIR/$name-$backend.log")"
    set +e
    (cd "$ROOT_DIR" && timeout 30 "${heap_check[@]}" "$WORK_DIR/$name-$backend.exe") >"$WORK_DIR/$name-$backend.raw"
    rc=$?
    set -e
    [[ "$rc" -eq "$status" ]] || fail "$name exited $rc on $backend, expected $status"
    tr -d '\r' <"$WORK_DIR/$name-$backend.raw" >"$WORK_DIR/$name-$backend.run"
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$backend.run" ||
        fail "$name output drifted on $backend"
}
for backend in c llvm; do
    run_admitted rowed "$backend" 0
    run_admitted record_observed "$backend" 0
    run_admitted constructor_handoff "$backend" 0
    run_admitted grown_record "$backend" 6
    run_admitted formal_member_read "$backend" 0
    run_admitted record_single_owner "$backend" 0
done

# Native MIR into the installed backend (--mir-json): the native semantic
# hands the local to its record as well, so the record releases it once and
# the program runs. The same MIR with the hand-off taken away, or with one
# forged onto the member move that takes the storage back, is refused.
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --mir-json "$WORK_REL/constructor_handoff.pgy") \
    >"$WORK_DIR/native_handoff.json" 2>"$WORK_DIR/native_handoff.err" ||
    fail "native MIR producer rejected constructor_handoff: $(tail -3 "$WORK_DIR/native_handoff.err")"
python3 - "$WORK_DIR/native_handoff.json" "$WORK_DIR" <<'PY' ||
import json
import sys

document = json.load(open(sys.argv[1], encoding="utf-8"))
main = next(row for row in document["routines"] if row.get("name") == "Main")
names = {row["binding_syntax_id"]: row.get("name") for row in main["source_locals"]}
rows = {names.get(row["binding_syntax_id"]): row for row in main["collection_ownership_facts"]}
texts, moved = rows.get("texts"), rows.get("moved")
if texts is None or texts["disposition"] != "handed-off" or texts["origin"] != "empty-literal":
    raise SystemExit(f"native MIR did not hand texts to its record: {texts}")
if moved is None or moved["origin"] != "member-move" or moved["disposition"] != "live":
    raise SystemExit(f"native MIR member move changed: {moved}")
texts["disposition"] = "live"
json.dump(document, open(f"{sys.argv[2]}/native_missing.json", "w", encoding="utf-8"))
texts["disposition"] = "handed-off"
moved["disposition"] = "handed-off"
json.dump(document, open(f"{sys.argv[2]}/native_forged.json", "w", encoding="utf-8"))
PY
    fail "native MIR lost the constructor hand-off of constructor_handoff"
# Native MIR of `name` runs on `backend` with the program's expected output.
native_mir_run() {
    local name="$1" backend="$2" artifact="$WORK_REL/native_$1.$2"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/native_$name.json" \
        -o "$artifact") >"$WORK_DIR/native_$name.$backend.log" 2>&1 ||
        fail "native MIR of $name was refused on $backend: $(head -3 "$WORK_DIR/native_$name.$backend.log")"
    if [[ "$backend" == c ]]; then
        "${PGY_SELFHOST_CC:-gcc}" -x c -std=c11 "$ROOT_DIR/$artifact" "-I$ROOT_DIR/src" \
            "-I$ROOT_DIR/src/runtime" -pthread -lm -o "$WORK_DIR/native_$name-c.exe" \
            >"$WORK_DIR/native_$name.c.cc" 2>&1 || fail "native MIR C artifact of $name did not compile"
    else
        "${PGY_SELFHOST_CLANG:-clang}" -x ir "$ROOT_DIR/$artifact" -x none "$WORK_DIR/runtime.o" \
            -pthread -lm -o "$WORK_DIR/native_$name-llvm.exe" \
            >"$WORK_DIR/native_$name.llvm.cc" 2>&1 || fail "native MIR LLVM artifact of $name did not compile"
    fi
    (cd "$ROOT_DIR" && timeout 30 "${heap_check[@]}" "$WORK_DIR/native_$name-$backend.exe") | tr -d '\r' \
        >"$WORK_DIR/native_$name-$backend.run" || fail "native MIR of $name failed on $backend"
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/native_$name-$backend.run" ||
        fail "native MIR output of $name drifted on $backend"
}
cp "$WORK_DIR/native_handoff.json" "$WORK_DIR/native_constructor_handoff.json"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --mir-json "$WORK_REL/formal_member_read.pgy") \
    >"$WORK_DIR/native_formal_member_read.json" 2>"$WORK_DIR/native_formal_member_read.err" ||
    fail "native MIR producer rejected formal_member_read"
"${PGY_SELFHOST_CLANG:-clang}" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.cc" 2>&1 || fail "runtime ABI compilation failed"
for backend in c llvm; do
    native_mir_run constructor_handoff "$backend"
    native_mir_run formal_member_read "$backend"
    for forged in native_missing native_forged; do
        message="$NO_HANDOFF"
        [[ "$forged" == native_forged ]] && message="$FORGED_HANDOFF"
        set +e
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$forged.json" \
            -o "$WORK_REL/$forged.$backend") >"$WORK_DIR/$forged.$backend.log" 2>&1
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] || fail "$forged MIR was admitted on $backend"
        [[ ! -e "$WORK_DIR/$forged.$backend" ]] || fail "$forged MIR published an artifact on $backend"
        grep -Fq "$message" "$WORK_DIR/$forged.$backend.log" ||
            fail "$forged MIR was refused for another reason on $backend: $(head -3 "$WORK_DIR/$forged.$backend.log")"
    done
done

echo "[$LABEL] PASS refused=14+4 admitted=6x2+native-mir-2x2 evidence=$WORK_REL"
