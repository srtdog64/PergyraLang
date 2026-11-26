#!/usr/bin/env bash
# `None()` and `option ?? fallback` on the general direct-MIR route: a
# zero-argument call to None is the absence of the expected Option, and
# `??` over an Option<Int> or Option<Bool> takes a pure fallback. Direct C
# (ASan) and LLVM print native's output. A fallback that calls a routine
# (native runs it only when the option is absent) and a forged Bool
# fallback of an Option<Int> are refused.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-option-coalesce"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_option_coalesce"
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

for source in tests/self_hosted/fixtures/direct_mir_option_coalesce.pgy; do
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

# A fallback with an effect: evaluating it eagerly would run it even when
# the option is present, so the general route refuses it.
source=tests/self_hosted/fixtures/direct_mir_option_coalesce_call_refused.pgy
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/call.mir.json") \
    >"$WORK_DIR/call.producer.out" 2>"$WORK_DIR/call.producer.err" || {
        cat "$WORK_DIR/call.producer.err" >&2; fail "call: MIR production failed"; }
for backend in c llvm; do
    if project call "$backend" "$WORK_REL/call.mir.json" "$WORK_REL/call.$backend"; then
        fail "$backend admitted a coalesce with a calling fallback"
    fi
    [[ ! -e "$ROOT_DIR/$WORK_REL/call.$backend" ]] || fail "$backend published the calling-fallback program"
    grep -aq 'direct MIR scalar' "$WORK_DIR/call.$backend.out" "$WORK_DIR/call.$backend.err" || {
        cat "$WORK_DIR/call.$backend.out" "$WORK_DIR/call.$backend.err" >&2
        fail "$backend refused the calling fallback before the general route"; }
done

# Forged MIR: the Int fallback of `MaybeIncr(9, 3) ?? 11` becomes a Bool
# literal (digest resealed), so `Option<Int> ?? Bool` reaches the general
# route, which has no such coalesce and refuses it.
"$PYTHON_BIN" - "$WORK_DIR/direct_mir_option_coalesce.mir.json" "$WORK_DIR/bool-fallback.mir.json" <<'PY'
import json, sys
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
    value = hash_int(hash_int(71, graph["root"]), len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = hash_text(value, node[key])
        value = hash_int(value, node["runtime_call_abi_id"])
        value = hash_int(value, -1 if node["left"] is None else node["left"])
        value = hash_int(value, -1 if node["right"] is None else node["right"])
    return 1073741824 + value

found = []
for routine in document["routines"]:
    for block in routine["blocks"]:
        for instruction in block["instructions"]:
            graph = instruction.get("expr0_graph")
            if not graph:
                continue
            for node in graph["nodes"]:
                if node["kind"] == "coalesce" and node["right"] is not None:
                    fallback = graph["nodes"][node["right"]]
                    if fallback["kind"] == "integer_literal" and fallback["text"] == "11":
                        found.append((graph, fallback))
if len(found) != 2 or any(graph_digest(g) != g["digest"] for g, _ in found):
    raise SystemExit("fixture has no two resealable `?? 11` fallbacks")
graph, fallback = found[0]
fallback["kind"] = "bool_literal"
fallback["text"] = "true"
graph["digest"] = graph_digest(graph)
json.dump(document, open(sys.argv[2], "w", encoding="utf-8"), separators=(",", ":"))
PY
for backend in c llvm; do
    if project bool-fallback "$backend" "$WORK_REL/bool-fallback.mir.json" "$WORK_REL/bool-fallback.$backend"; then
        fail "$backend admitted Option<Int> ?? Bool"
    fi
    [[ ! -e "$ROOT_DIR/$WORK_REL/bool-fallback.$backend" ]] || fail "$backend published Option<Int> ?? Bool"
    grep -aq 'direct MIR scalar' "$WORK_DIR/bool-fallback.$backend.out" "$WORK_DIR/bool-fallback.$backend.err" || {
        cat "$WORK_DIR/bool-fallback.$backend.out" "$WORK_DIR/bool-fallback.$backend.err" >&2
        fail "$backend refused Option<Int> ?? Bool before the general route"; }
done

echo "[$LABEL] PASS: None() and ?? program equal to native on C (ASan) and LLVM; calling fallback and forged Bool fallback refused"
