#!/usr/bin/env bash
# A direct-MIR Array<String> local is cleaned up only from its admitted
# collection ownership row. A local whose storage and elements the semantic
# fact leaves unknown (a call result, an alias of a formal) is refused with no
# artifact on both legs; it is never released from its operation shape.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-array-string-cleanup"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
REFUSAL="direct MIR Array<String> cleanup requires an admitted ownership row"

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
# Neighbours whose ownership is proved still compile and run: a literal of
# borrowed elements, a local that owns pushed copies, and a runtime Split
# result whose elements the runtime copied.
cat >"$WORK_DIR/rowed.pgy" <<'EOF'
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
}
EOF
printf 'b\nc\n2\nq\n' >"$WORK_DIR/rowed.expected"

refuse() {
    local name="$1" flag="$2" extension="$3" rc
    local artifact="$WORK_REL/$name.$extension"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$flag" "$WORK_REL/$name.pgy" -o "$artifact") \
        >"$WORK_DIR/$name.$extension.out" 2>"$WORK_DIR/$name.$extension.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$name $flag cleaned up a local with no ownership row"
    [[ ! -e "$ROOT_DIR/$artifact" ]] || fail "$name $flag published an artifact"
    # The driver prints its CODEGEN ERROR on stdout; pgy adds its own line on stderr.
    grep -Fq "$REFUSAL" "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err" ||
        fail "$name $flag was refused for another reason: $(head -3 "$WORK_DIR/$name.$extension.out" "$WORK_DIR/$name.$extension.err")"
}

for name in call_result formal_alias; do
    refuse "$name" --emit-c c
    refuse "$name" --emit-llvm ll
done

(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" --emit-c "$WORK_REL/rowed.pgy" -o "$WORK_REL/rowed.c") \
    >"$WORK_DIR/rowed.out" 2>"$WORK_DIR/rowed.err" || fail "rowed locals were refused: $(head -3 "$WORK_DIR/rowed.err")"
compile_command=("$CC" -std=c11 "$WORK_DIR/rowed.c")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/rowed.c"; then
    compile_command+=(-D_POSIX_C_SOURCE=200809L "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
"${compile_command[@]}" -o "$WORK_DIR/rowed.exe" >"$WORK_DIR/rowed.cc.out" 2>&1 ||
    fail "rowed C did not compile"
(cd "$ROOT_DIR" && timeout 30 "$WORK_DIR/rowed.exe") | tr -d '\r' >"$WORK_DIR/rowed.run" ||
    fail "rowed program failed"
cmp -s "$WORK_DIR/rowed.expected" "$WORK_DIR/rowed.run" || fail "rowed program output drifted"

echo "[$LABEL] PASS refused=2x2 rowed=1 evidence=$WORK_REL"
