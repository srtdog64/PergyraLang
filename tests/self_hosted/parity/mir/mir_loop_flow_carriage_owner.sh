#!/usr/bin/env bash
# Loop and resource flow facts in self-host MIR. The self-host producer
# carries loop result flags from the semantic flow calculus and marks effect
# masks, loop states and resource-flow symbols as not carried; native MIR
# carries all of them. On the fixture, every self-host loop row has native's
# kind and flags, native tracks a resource-flow symbol the self-host routine
# does not claim, and direct C (ASan) and LLVM print native's output. The
# second fixture's loops never fall through; the general route refuses its
# routines today, so only its MIR rows are compared. Forged self-host MIR is refused: a non-zero effect placeholder, a
# resource-flow row under the not-carried mark or with no mark at all, and a
# transaction-failure loop exit.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-mir-loop-flow-carriage"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/mir_loop_flow_carriage"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE=tests/self_hosted/fixtures/mir_loop_flow_flags.pgy
NO_FALLTHROUGH=tests/self_hosted/fixtures/mir_loop_flow_flags_no_fallthrough.pgy

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
    local name="$1" backend="$2"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$name.mir.json" \
        -o "$WORK_REL/$name.$backend") >"$WORK_DIR/$name.$backend.out" 2>"$WORK_DIR/$name.$backend.err"
}

for pair in "self:$SOURCE" "nofall:$NO_FALLTHROUGH"; do
    name="${pair%%:*}" source="${pair#*:}"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.err" >&2; fail "$name: self-host MIR production failed"; }
    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$source") \
        >"$WORK_DIR/$name.native.mir.json" 2>"$WORK_DIR/$name.native.mir.err" || {
            cat "$WORK_DIR/$name.native.mir.err" >&2; fail "$name: native MIR production failed"; }
done

"$PYTHON_BIN" - "$WORK_DIR" <<'PY'
import json, sys
def routine_list(name):
    return json.load(open("%s/%s" % (sys.argv[1], name), encoding="utf-8"))["routines"]
native = {"routines": routine_list("self.native.mir.json") + routine_list("nofall.native.mir.json")}
selfd = {"routines": routine_list("self.mir.json") + routine_list("nofall.mir.json")}
EXPECTED = {"FirstAbove": [9], "Countdown": [8], "FirstOfThree": [8],
            "NestedFind": [9, 9], "BreakOut": [1], "SumUntil": [9]}

def routines(doc):
    return {r["name"]: r for r in doc["routines"]}

def loops(routine):
    return sorted((s["kind"], s["flags"]) for s in routine["loop_flow_summaries"])

n, s = routines(native), routines(selfd)
for name, flags in EXPECTED.items():
    if name not in n or name not in s:
        sys.exit("missing routine %s" % name)
    if n[name].get("flow_state_carried") is not True:
        sys.exit("native %s does not carry flow state" % name)
    if s[name].get("flow_state_carried") is not False:
        sys.exit("self-host %s does not mark flow state as not carried" % name)
    if loops(n[name]) != loops(s[name]):
        sys.exit("%s loop rows differ: native %s self %s" % (name, loops(n[name]), loops(s[name])))
    if sorted(f for _, f in loops(s[name])) != sorted(flags):
        sys.exit("%s loop flags %s, expected %s" % (name, loops(s[name]), flags))
    if s[name]["resource_flow_symbol_count"] != 0 or s[name]["loop_flow_state_count"] != 0 or any(
            row[k] != 0 for row in s[name]["loop_flow_summaries"]
            for k in ("effect_base", "effect_delta", "entry_state_count", "exit_state_count")):
        sys.exit("self-host %s claims flow state it does not carry" % name)
if n["SumUntil"]["resource_flow_symbol_count"] < 1:
    sys.exit("native SumUntil tracks no resource-flow symbol; the not-carried case is not exercised")
PY

(cd "$ROOT_DIR" && "$PGY" "$SOURCE" --native-pipeline --backend=c -o "$WORK_REL/native.exe") \
    >"$WORK_DIR/native.out" 2>&1 || { cat "$WORK_DIR/native.out" >&2; fail "native compile failed"; }
"$WORK_DIR/native.exe" | tr -d '\r' >"$WORK_DIR/expected"
[[ -s "$WORK_DIR/expected" ]] || fail "native printed nothing"
for backend in c llvm; do
    project self "$backend" || {
        cat "$WORK_DIR/self.$backend.out" "$WORK_DIR/self.$backend.err" >&2
        fail "$backend projection refused"; }
    bin="$WORK_DIR/self-$backend.exe"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT_DIR/$WORK_REL/self.c")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$WORK_REL/self.c"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        "${command[@]}" -lm -o "$bin" 2>"$WORK_DIR/self.c.compile.err" ||
            { cat "$WORK_DIR/self.c.compile.err" >&2; fail "C artifact did not compile"; }
    else
        "$CLANG" -x ir "$ROOT_DIR/$WORK_REL/self.llvm" -x none "$runtime_object" -pthread -lm \
            -o "$bin" 2>"$WORK_DIR/self.llvm.compile.err" ||
            { cat "$WORK_DIR/self.llvm.compile.err" >&2; fail "LLVM artifact did not compile"; }
    fi
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$bin" 2>"$WORK_DIR/self.$backend.run.err" |
        tr -d '\r' >"$WORK_DIR/self.$backend.run" ||
        { cat "$WORK_DIR/self.$backend.run.err" >&2; fail "$backend run failed"; }
    cmp -s "$WORK_DIR/expected" "$WORK_DIR/self.$backend.run" || {
        diff "$WORK_DIR/expected" "$WORK_DIR/self.$backend.run" >&2 || true
        fail "$backend output differs from native"; }
done

# Forged self-host MIR. FirstAbove's routine is rewritten in place.
"$PYTHON_BIN" - "$WORK_DIR/self.mir.json" "$WORK_DIR" <<'PY'
import json, sys
source, out = sys.argv[1], sys.argv[2]

def forge(name, change):
    document = json.load(open(source, encoding="utf-8"))
    routine = next(r for r in document["routines"] if r["name"] == "FirstAbove")
    change(routine)
    json.dump(document, open("%s/%s.mir.json" % (out, name), "w", encoding="utf-8"),
              separators=(",", ":"))

SYMBOL = {"stable_index": 0, "declaration_syntax_id": 5, "line": 7, "column": 9,
          "symbol_kind": 0, "is_parameter": False, "parameter_index": 0, "name": "i"}

def effect(routine):
    routine["loop_flow_summaries"][0]["effect_delta"] = 4

def unmarked_symbol(routine):
    del routine["flow_state_carried"]
    routine["resource_flow_symbol_count"] = 1
    routine["resource_flow_symbols"] = [SYMBOL]

def marked_symbol(routine):
    routine["resource_flow_symbol_count"] = 1
    routine["resource_flow_symbols"] = [SYMBOL]

def txn_exit(routine):
    routine["loop_flow_summaries"][0]["flags"] = 41

def carried_symbol(routine):
    marked_symbol(routine)
    routine["flow_state_carried"] = True

forge("rewritten", lambda routine: None)
forge("carried-symbol", carried_symbol)
forge("effect", effect)
forge("unmarked-symbol", unmarked_symbol)
forge("marked-symbol", marked_symbol)
forge("txn-exit", txn_exit)
PY
# Controls: re-serialised but unchanged MIR is admitted, and so is the same
# resource-flow row under a carried mark, so each refusal below comes from the
# forged field and the two symbol refusals from the missing carriage.
for name in rewritten carried-symbol; do
    for backend in c llvm; do
        project "$name" "$backend" || {
            cat "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" >&2
            fail "$backend refused control MIR: $name"; }
    done
done
for name in effect unmarked-symbol marked-symbol txn-exit; do
    for backend in c llvm; do
        if project "$name" "$backend"; then
            fail "$backend admitted forged MIR: $name"
        fi
        [[ ! -e "$ROOT_DIR/$WORK_REL/$name.$backend" ]] || fail "$backend published forged MIR: $name"
    done
done

echo "[$LABEL] PASS: self-host loop flags equal native's, flow state marked not carried, C (ASan) and LLVM equal native; 4 forged MIR shapes refused on both backends"
