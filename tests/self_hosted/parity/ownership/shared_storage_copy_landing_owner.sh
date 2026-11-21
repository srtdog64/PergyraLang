#!/usr/bin/env bash
# Shared storage a growth or a release retires is copied where it is shared,
# or the program is refused; the self-host front end never admits it with the
# storage still shared.
# - A statement push through a formal, or through a member path of a local
#   or a formal, grows that binding (it used to be skipped). A by-value
#   argument the callee grows that the caller uses again is copied where the
#   call takes it (decision 2026-10-22); the round-trip C route emits the
#   copy, and a direct MIR route, whose MIR carries no copy, refuses.
# - A member-path growth of a copy whose source is read again, a String
#   finished with a pool that leaves its routine or is read after the pool's
#   destroy, and a String element read after its Array was handed to `own`
#   are refused on both legs with no artifact, as is a String a callee
#   returns from the map it is handed, held while the map changes.
# - Neighbours that share nothing still compile and run on both legs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-shared-storage-copy-landing"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
DIRECT="direct MIR emission cannot carry"
MEMBER="unproved_exclusive_member_mutation"
LEAVES="allocator_lane_loan_leaves_routine"
DESTROYED="allocator_lane_loan_read_after_destroy"
# The `own` callee releases the elements it is handed (its element requirement).
RELEASED="indexed_string_use_after_element_release"
MAPLOAN="map_string_use_after_mutation"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
export PGY_SELF_DRIVER_BIN="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/shared_storage_copy_landing.XXXXXX")"
WORK_REL=".tmp/self_hosted/${WORK_DIR##*/}"

# Grow pushes through its by-value formal's member path past the literal's
# capacity while Main still reads outer: Main passes a copy.
cat >"$WORK_DIR/formal_member_grown.pgy" <<'EOF'
struct Inner { items: Array<Int>; }
struct Outer { inner: Inner; count: Int; }
func Grow(outer: Outer) -> Int {
    ArrayPush(outer.inner.items, 9); ArrayPush(outer.inner.items, 9);
    ArrayPush(outer.inner.items, 9); ArrayPush(outer.inner.items, 9);
    return ArrayLength(outer.inner.items);
}
func Main() -> Int {
    let outer: Outer = Outer(Inner([1, 2]), 0);
    let n: Int = Grow(outer);
    Print(ToString(n + ArrayLength(outer.inner.items) + outer.inner.items[1]));
    return 0;
}
EOF
printf '10' >"$WORK_DIR/formal_member_grown.expected"
# Pass hands its by-value Array to an inout grower; Main reads rows again.
cat >"$WORK_DIR/inout_hand_off.pgy" <<'EOF'
func Grow(inout rows: Array<Int>) -> Int { ArrayPush(rows, 9); return 0; }
func Pass(rows: Array<Int>) -> Int { let k: Int = Grow(rows); return k; }
func Main() -> Int {
    let rows: Array<Int> = [1];
    let k: Int = Pass(rows);
    Print(ToString(rows[0] + ArrayLength(rows) + k));
    return 0;
}
EOF
printf '2' >"$WORK_DIR/inout_hand_off.expected"
# A copy of outer.inner grows while outer is read again.
cat >"$WORK_DIR/member_copy_grown.pgy" <<'EOF'
struct Inner { items: Array<Int>; }
struct Outer { inner: Inner; count: Int; }
func Main() -> Int {
    let outer: Outer = Outer(Inner([1, 2]), 0);
    var other: Inner = outer.inner;
    ArrayPush(other.items, 9); ArrayPush(other.items, 9);
    ArrayPush(other.items, 9); ArrayPush(other.items, 9);
    Print(ToString(ArrayLength(other.items) + outer.inner.items[1]));
    return 0;
}
EOF
cat >"$WORK_DIR/pool_text_returned.pgy" <<'EOF'
func Build(text: String) -> String {
    let allocator = AllocatorPool(128);
    let builder = TextBuilderNew(32);
    TextBuilderAppend(builder, text);
    let output = TextBuilderFinish(builder, allocator);
    AllocatorDestroy(allocator);
    return output;
}
func Main() -> Void { Log(StringLength(Build(Concat("heap", " text")))); }
EOF
cat >"$WORK_DIR/pool_text_read_after_destroy.pgy" <<'EOF'
func Wrap(value: String, allocator: Allocator) -> String {
    let output: TextBuilder = TextBuilderNew(StringLength(value) + 2);
    TextBuilderAppend(output, "[");
    TextBuilderAppend(output, value);
    TextBuilderAppend(output, "]");
    let wrapped: String = TextBuilderFinish(output, allocator);
    return wrapped;
}
func Main() -> Int {
    let pool: Allocator = AllocatorPool(64);
    let wrapped: String = Wrap(Concat("a", "b"), pool);
    AllocatorDestroy(pool);
    Print(wrapped);
    return 0;
}
EOF
cat >"$WORK_DIR/element_after_own_release.pgy" <<'EOF'
func Clear(own names: Array<String>) -> Void { ArrayDropOwnedStrings(names); }
func Pick(i: Int) -> Int {
    var names: Array<String> = [];
    ArrayPushOwnedString(names, Concat("re", "d"));
    let text: String = names[0];
    Clear(names);
    if i < 0 { return 0; }
    return StringLength(text);
}
func Main() -> Int { Print(ToString(Pick(1))); return 0; }
EOF
# A String a callee returns from the map it is handed lends that map; the
# map changes while Main still holds it.
cat >"$WORK_DIR/map_loan_mutated.pgy" <<'EOF'
func ReadValue(ref values: HashMap<String, String>) -> String { return MapGet(values, "key"); }
func Main() -> Int {
    var values: HashMap<String, String> = MapNew();
    MapSet(values, "key", Concat("a", "b"));
    let held: String = ReadValue(values);
    MapSet(values, "key", Concat("c", "d"));
    Print(held);
    return 0;
}
EOF
# Neighbours: an inout grower, a pool text read (also by a borrow-only
# formal) before its destroy, and a String element read before the hand-off.
cat >"$WORK_DIR/neighbours.pgy" <<'EOF'
func Grow(inout rows: Array<Int>) -> Void { ArrayPush(rows, 9); ArrayPush(rows, 7); }
func Size(text: String) -> Int { return StringLength(text); }
func Wrap(value: String, allocator: Allocator) -> String {
    let output: TextBuilder = TextBuilderNew(StringLength(value) + 2);
    TextBuilderAppend(output, "[");
    TextBuilderAppend(output, value);
    TextBuilderAppend(output, "]");
    let wrapped: String = TextBuilderFinish(output, allocator);
    return wrapped;
}
func Clear(own names: Array<String>) -> Void { ArrayDropOwnedStrings(names); }
func Main() -> Int {
    var rows: Array<Int> = [1];
    Grow(rows);
    Grow(rows);
    Print(ToString(ArrayLength(rows) + rows[4]));
    let pool: Allocator = AllocatorPool(64);
    let wrapped: String = Wrap(Concat("a", "b"), pool);
    Print(wrapped);
    Print(ToString(Size(wrapped)));
    AllocatorDestroy(pool);
    var names: Array<String> = [];
    ArrayPushOwnedString(names, Concat("re", "d"));
    let size: Int = StringLength(names[0]);
    Clear(names);
    Print(ToString(size));
    return 0;
}
EOF
printf '12[ab]43' >"$WORK_DIR/neighbours.expected"
# A map loan read before the map changes. The direct MIR LLVM route has no
# plan for this map program (it refuses it on main too), so it runs on C.
cat >"$WORK_DIR/map_read_first.pgy" <<'EOF'
func ReadValue(ref values: HashMap<String, String>) -> String { return MapGet(values, "key"); }
func Main() -> Int {
    var values: HashMap<String, String> = MapNew();
    MapSet(values, "key", Concat("x", "y"));
    let held: String = ReadValue(values);
    Print(held);
    MapSet(values, "key", Concat("c", "d"));
    return 0;
}
EOF
printf 'xy' >"$WORK_DIR/map_read_first.expected"

refuse() {
    local name="$1" flag="$2" extension="$3" message="$4" rc
    local artifact="$WORK_REL/$name.$extension"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$flag" "$WORK_REL/$name.pgy" -o "$artifact") \
        >"$WORK_DIR/$name.$extension.out" 2>"$WORK_DIR/$name.$extension.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$name $flag admitted storage another holder still reads"
    [[ ! -e "$ROOT_DIR/$artifact" ]] || fail "$name $flag published an artifact"
    grep -Fq "$message" "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err" ||
        fail "$name $flag was refused for another reason: $(head -3 "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err")"
}

run() {
    local name="$1" backend="$2"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$WORK_REL/$name.pgy" "--backend=$backend" \
        -o "$WORK_REL/$name-$backend.exe") >"$WORK_DIR/$name-$backend.log" 2>&1 ||
        fail "$name was refused on $backend: $(head -3 "$WORK_DIR/$name-$backend.log")"
    (cd "$ROOT_DIR" && timeout 30 "$WORK_DIR/$name-$backend.exe") | tr -d '\r\n' >"$WORK_DIR/$name-$backend.run" ||
        fail "$name failed on $backend"
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$backend.run" ||
        fail "$name output drifted on $backend: $(cat "$WORK_DIR/$name-$backend.run")"
}

# The copy lands on the round-trip C route; the direct MIR LLVM route, whose
# MIR carries no copy, refuses.
for name in formal_member_grown inout_hand_off; do
    run "$name" c
    refuse "$name" --emit-llvm ll "$DIRECT"
done
for flag in --emit-c --emit-llvm; do
    extension=c
    [[ "$flag" == --emit-llvm ]] && extension=ll
    refuse member_copy_grown "$flag" "$extension" "$MEMBER"
    refuse pool_text_returned "$flag" "$extension" "$LEAVES"
    refuse pool_text_read_after_destroy "$flag" "$extension" "$DESTROYED"
    refuse element_after_own_release "$flag" "$extension" "$RELEASED"
    refuse map_loan_mutated "$flag" "$extension" "$MAPLOAN"
done
run neighbours c
run neighbours llvm
run map_read_first c

echo "[$LABEL] PASS landed=2 refused=12 admitted=1x2+1 evidence=$WORK_REL"
