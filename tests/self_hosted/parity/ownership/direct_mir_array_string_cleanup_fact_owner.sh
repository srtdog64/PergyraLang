#!/usr/bin/env bash
# A direct-MIR Array<String> local is cleaned up only from its admitted
# collection ownership row or a runtime producer, and only while no other
# holder has its storage. A local whose storage and elements the semantic fact
# leaves unknown (a call result, an alias of a formal), a local handed to a
# callee that grows it or to a constructor, and an alias of an inout member
# that grows in place are refused with no artifact; none is released from its
# operation shape.
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
# The record takes the local's storage and a member alias takes it back out:
# releasing both frees one descriptor twice.
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
# literal's capacity: the caller's release frees what realloc already freed.
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
# A program with a struct takes the round-trip C route, which copies instead;
# only the LLVM leg reaches the direct MIR cleanup.
refuse constructor_handoff --emit-llvm ll "$SHARED"
refuse grown_record --emit-llvm ll "$SHARED"
refuse member_alias --emit-llvm ll "$MEMBER_ALIAS"

for name in rowed record_observed; do
    for backend in c llvm; do
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$WORK_REL/$name.pgy" "--backend=$backend" \
            -o "$WORK_REL/$name-$backend.exe") >"$WORK_DIR/$name-$backend.log" 2>&1 ||
            fail "$name was refused on $backend: $(head -3 "$WORK_DIR/$name-$backend.log")"
        (cd "$ROOT_DIR" && timeout 30 "$WORK_DIR/$name-$backend.exe") | tr -d '\r' >"$WORK_DIR/$name-$backend.run" ||
            fail "$name failed on $backend"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$backend.run" ||
            fail "$name output drifted on $backend"
    done
done

echo "[$LABEL] PASS refused=9 admitted=2x2 evidence=$WORK_REL"
