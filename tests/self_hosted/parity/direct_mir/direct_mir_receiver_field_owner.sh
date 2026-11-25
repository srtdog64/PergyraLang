#!/usr/bin/env bash
# Methods of a mutable-identity receiver that name its fields without
# `self.` run on the general direct-MIR route: an owner-field definition is
# the identity-cell store, a bare field read is the receiver's member, and
# phis of the field join nothing. In a class method (a value receiver) a
# bare field read is the receiver record's member. Direct C (ASan) and LLVM print native's
# output. Forged MIR that names a missing field, breaks the target, or gives
# a local a field variable's name is refused.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-receiver-field"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_receiver_field"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

mkdir -p "$WORK_DIR"
rm -rf "${WORK_DIR:?}"/*
runtime_object="$WORK_DIR/pgy-runtime-lib.o"
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_object" \
    >"$WORK_DIR/runtime.out" 2>"$WORK_DIR/runtime.err" || {
        cat "$WORK_DIR/runtime.err" >&2; fail "LLVM runtime object did not compile"; }

project() {
    local name="$1" backend="$2" mir_rel="$3" output_rel="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mir_rel" -o "$output_rel") \
        >"$WORK_DIR/$name.$backend.out" 2>"$WORK_DIR/$name.$backend.err"
}

for source in tests/self_hosted/fixtures/direct_mir_receiver_field.pgy \
        tests/self_hosted/fixtures/direct_mir_value_receiver_field.pgy; do
    name="$(basename "$source" .pgy)"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.err" >&2; fail "$name: MIR production failed"; }
    (cd "$ROOT_DIR" && "$PGY" "$source" --native-pipeline --backend=c -o "$WORK_REL/$name.native.exe") \
        >"$WORK_DIR/$name.native.out" 2>&1 ||
        { cat "$WORK_DIR/$name.native.out" >&2; fail "$name: native compile failed"; }
    "$WORK_DIR/$name.native.exe" | tr -d '\r' >"$WORK_DIR/$name.expected"
    [[ -s "$WORK_DIR/$name.expected" ]] || fail "$name: native printed nothing"
    for backend in c llvm; do
        project "$name" "$backend" "$WORK_REL/$name.mir.json" "$WORK_REL/$name.$backend" || {
            cat "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" >&2
            fail "$name: $backend projection refused"; }
        bin="$WORK_DIR/$name-$backend.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT_DIR/$WORK_REL/$name.c")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$WORK_REL/$name.c"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            "${command[@]}" -lm -o "$bin" 2>"$WORK_DIR/$name.c.compile.err" ||
                { cat "$WORK_DIR/$name.c.compile.err" >&2; fail "$name: C artifact did not compile"; }
        else
            "$CLANG" -x ir "$ROOT_DIR/$WORK_REL/$name.llvm" -x none "$runtime_object" -pthread -lm \
                -o "$bin" 2>"$WORK_DIR/$name.llvm.compile.err" ||
                { cat "$WORK_DIR/$name.llvm.compile.err" >&2; fail "$name: LLVM artifact did not compile"; }
        fi
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$bin" 2>"$WORK_DIR/$name.$backend.run.err" |
            tr -d '\r' >"$WORK_DIR/$name.$backend.run" ||
            { cat "$WORK_DIR/$name.$backend.run.err" >&2; fail "$name: $backend run failed"; }
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" || {
            diff "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" >&2 || true
            fail "$name: $backend output differs from native"; }
    done
done

# Forged MIR: each mutation of the Counter fixture must be refused on both
# backends without publishing an artifact.
"$PYTHON_BIN" - "$WORK_DIR/direct_mir_receiver_field.mir.json" "$WORK_DIR" <<'PY'
import copy, json, sys
document = json.load(open(sys.argv[1], encoding="utf-8"))
MODULUS = 268435456

def hash_int(value, number):
    return (value * 131 + number + 2) % MODULUS

def hash_text(value, text):
    data = text.encode("utf-8")
    out = (value * 131 + len(data)) % MODULUS
    for byte in data:
        out = (out * 131 + byte) % MODULUS
    return out

def graph_digest(graph):
    # Mirror of SelfMirExpressionGraphDigest, so the reader admits an edited
    # graph and the refusal comes from the route under test.
    value = hash_int(hash_int(71, graph["root"]), len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = hash_text(value, node[key])
        value = hash_int(value, node["runtime_call_abi_id"])
        value = hash_int(value, -1 if node["left"] is None else node["left"])
        value = hash_int(value, -1 if node["right"] is None else node["right"])
    return 1073741824 + value

def reseal(graph):
    graph["digest"] = graph_digest(graph)

for routine in document["routines"]:
    for block in routine["blocks"]:
        for instruction in block["instructions"]:
            for lane in ("expr0_graph", "expr1_graph"):
                if instruction.get(lane) and graph_digest(instruction[lane]) != instruction[lane]["digest"]:
                    raise SystemExit("digest mirror disagrees with the MIR writer")

def owner_field_defs(doc, routine_name):
    routine = [r for r in doc["routines"] if r["name"] == routine_name]
    if len(routine) != 1:
        raise SystemExit(f"fixture has no single {routine_name} routine")
    found = [i for b in routine[0]["blocks"] for i in b["instructions"]
             if i["kind"] == "def" and i.get("arg1") == "owner_field"]
    if not found:
        raise SystemExit(f"{routine_name} has no owner-field definition")
    return found

def write(name, doc):
    json.dump(doc, open(f"{sys.argv[2]}/{name}.mir.json", "w", encoding="utf-8"), separators=(",", ":"))

# A field the receiver cell does not have.
doc = copy.deepcopy(document)
definition = owner_field_defs(doc, "Increment")[0]
definition["arg0"] = "missing"; definition["result"] = "missing.1"
definition["expr1_graph"]["nodes"][0]["text"] = "missing"
reseal(definition["expr1_graph"])
write("missing-field", doc)
# The target names another field than the definition.
doc = copy.deepcopy(document)
definition = owner_field_defs(doc, "Increment")[0]
definition["expr1_graph"]["nodes"][0]["text"] = "steps"
reseal(definition["expr1_graph"])
write("crossed-target", doc)
# A local definition sharing the name of a field variable.
doc = copy.deepcopy(document)
owner_field_defs(doc, "Bump")[-1]["arg1"] = "Int"
write("field-named-local", doc)
# A bare read of a name that is no local, parameter or receiver field.
doc = copy.deepcopy(document)
total = [r for r in doc["routines"] if r["name"] == "Total"][0]
returns = [i for b in total["blocks"] for i in b["instructions"] if i["kind"] == "return"]
leaves = [n for i in returns for n in i["expr0_graph"]["nodes"] if n["kind"] == "leaf" and n["text"] == "value"]
if len(returns) != 1 or len(leaves) != 1:
    raise SystemExit("Total has no single bare value read")
leaves[0]["text"] = "missing"
reseal(returns[0]["expr0_graph"])
write("missing-read", doc)
PY
for mutation in missing-field crossed-target field-named-local missing-read; do
    for backend in c llvm; do
        if project "$mutation" "$backend" "$WORK_REL/$mutation.mir.json" "$WORK_REL/$mutation.$backend"; then
            fail "$backend accepted forged receiver-field MIR: $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$WORK_REL/$mutation.$backend" ]] || fail "$backend published forged program: $mutation"
        # The refusal comes from the general route's admission, not the reader.
        grep -aq 'direct MIR scalar CFG' "$WORK_DIR/$mutation.$backend.out" "$WORK_DIR/$mutation.$backend.err" || {
            cat "$WORK_DIR/$mutation.$backend.out" "$WORK_DIR/$mutation.$backend.err" >&2
            fail "$backend refused $mutation before the general route"; }
    done
done

echo "[$LABEL] PASS: 2 receiver-field programs equal to native on C (ASan) and LLVM; 4 forged MIR shapes refused"
