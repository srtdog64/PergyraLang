#!/usr/bin/env bash
# Populated Array<struct> literals on the general direct-MIR route: direct C
# (ASan) and LLVM run them as native does; a literal of storage-free records
# kept in its routine is released (no leaked allocation comes from the record
# array's push helper); an element whose record the program does not declare
# is refused.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-record-array-literal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_record_array_literal"
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

for name in direct_mir_record_array_literal direct_mir_record_array_literal_release direct_mir_record_array_literal_storage; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "tests/self_hosted/fixtures/$name.pgy" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.err" >&2; fail "$name: MIR production failed"; }
    (cd "$ROOT_DIR" && "$PGY" "tests/self_hosted/fixtures/$name.pgy" --native-pipeline \
        --backend=c -o "$WORK_REL/$name.native.exe") >"$WORK_DIR/$name.native.out" 2>&1 ||
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

# Storage-free literals kept in their routine are released: LeakSanitizer
# may report the ToString text (native leaks it too) but no allocation of the
# record array's push helper. (A literal handed to a by-value parameter is
# not released yet: neither side has the callee's borrow-or-move fact.)
ASAN_OPTIONS=detect_leaks=1 "$WORK_DIR/direct_mir_record_array_literal_release-c.exe" \
    >/dev/null 2>"$WORK_DIR/leaks.txt" || true
if grep -q "pgy_scalar_logical_record_array_push_" "$WORK_DIR/leaks.txt"; then
    cat "$WORK_DIR/leaks.txt" >&2
    fail "a storage-free Array<struct> literal was not released"
fi

# An element whose record the program does not declare is refused.
"$PYTHON_BIN" - "$WORK_DIR/direct_mir_record_array_literal.mir.json" \
    "$WORK_DIR/forged.mir.json" <<'PY'
import json, sys
document = json.load(open(sys.argv[1], encoding="utf-8"))
changed = 0
for routine in document["routines"]:
    for block in routine["blocks"]:
        for instruction in block["instructions"]:
            nodes = (instruction.get("expr0_graph") or {}).get("nodes", [])
            if not any(node.get("kind") == "array_element" for node in nodes):
                continue
            for node in nodes:
                if node.get("kind") == "leaf" and node.get("text") == "Point" and changed == 0:
                    node["text"] = "Unknown"
                    changed = 1
if changed != 1:
    raise SystemExit("fixture literal has no Point element to forge")
json.dump(document, open(sys.argv[2], "w", encoding="utf-8"), separators=(",", ":"))
PY
for backend in c llvm; do
    if project forged "$backend" "$WORK_REL/forged.mir.json" "$WORK_REL/forged.$backend"; then
        fail "$backend accepted an Array literal element of an undeclared record"
    fi
    [[ ! -e "$ROOT_DIR/$WORK_REL/forged.$backend" ]] || fail "$backend published the forged program"
done

echo "[$LABEL] PASS: 3 Array<struct> literal programs equal to native on C (ASan) and LLVM; storage-free literals released; undeclared element record refused"
