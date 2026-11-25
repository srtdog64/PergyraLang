#!/usr/bin/env bash
# A record's Array<Int>, Array<Bool> or Array<String> field gets its Array
# ABI from the canonical Array row when no routine parameter or result
# carries one; the direct C and LLVM projections then run the program as
# native does. A malformed captured Array<Int> or Array<String> row stays
# refused next to such a field.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-record-field-array-abi"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_record_field_array_abi"
WORK_DIR="$ROOT_DIR/$WORK_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir/direct_mir_multi_routine_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
[[ -f "$MUTATIONS" ]] || fail "missing owner: ${MUTATIONS#"$ROOT_DIR/"}"

mkdir -p "$WORK_DIR"
rm -rf "${WORK_DIR:?}"/*
runtime_object="$WORK_DIR/pgy-runtime-lib.o"
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_object" \
    >"$WORK_DIR/runtime.out" 2>"$WORK_DIR/runtime.err" || {
        cat "$WORK_DIR/runtime.err" >&2; fail "LLVM runtime object did not compile"; }

produce_mir() {
    local name="$1"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "tests/self_hosted/fixtures/$name.pgy" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.out" "$WORK_DIR/$name.producer.err" >&2
            fail "$name: MIR production failed"; }
}

# Native's output is the expected output.
native_run() {
    local name="$1"
    (cd "$ROOT_DIR" && "$PGY" "tests/self_hosted/fixtures/$name.pgy" --native-pipeline \
        --backend=c -o "$WORK_REL/$name.native.exe") >"$WORK_DIR/$name.native.out" 2>&1 ||
        { cat "$WORK_DIR/$name.native.out" >&2; fail "$name: native compile failed"; }
    "$WORK_DIR/$name.native.exe" | tr -d '\r' >"$WORK_DIR/$name.expected"
    [[ -s "$WORK_DIR/$name.expected" ]] || fail "$name: native printed nothing"
}

project_and_run() {
    local name="$1" backend="$2" artifact_rel="$WORK_REL/$1.$2" bin="$WORK_DIR/$1-$2.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/$name.mir.json" -o "$artifact_rel") >"$WORK_DIR/$name.$backend.out" \
        2>"$WORK_DIR/$name.$backend.err" || {
            cat "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" >&2
            fail "$name: $backend projection refused"; }
    if [[ "$backend" == c ]]; then
        local command=("$CC" -x c -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT_DIR/$artifact_rel")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$artifact_rel"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        "${command[@]}" -lm -o "$bin" 2>"$WORK_DIR/$name.c.compile.err" ||
            { cat "$WORK_DIR/$name.c.compile.err" >&2; fail "$name: C artifact did not compile"; }
    else
        "$CLANG" -x ir "$ROOT_DIR/$artifact_rel" -x none "$runtime_object" -pthread -lm \
            -o "$bin" 2>"$WORK_DIR/$name.llvm.compile.err" ||
            { cat "$WORK_DIR/$name.llvm.compile.err" >&2; fail "$name: LLVM artifact did not compile"; }
    fi
    # The C leg runs under AddressSanitizer. Leak detection is off: a record
    # local handed to a by-value parameter has its Array fields released by
    # neither the caller nor the callee (a record kept in its routine is
    # released), as for the records the route admitted before; recorded in
    # docs/truth/evidence/2026-11-25_record_field_array_string_abi.md.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$bin" 2>"$WORK_DIR/$name.$backend.run.err" |
        tr -d '\r' >"$WORK_DIR/$name.$backend.run" ||
        { cat "$WORK_DIR/$name.$backend.run.err" >&2; fail "$name: $backend run failed"; }
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" || {
        diff "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" >&2 || true
        fail "$name: $backend output differs from native"; }
}

for name in direct_mir_record_field_array_int direct_mir_record_field_array_bool \
        direct_mir_record_field_array_string direct_mir_record_field_array_int_with_parameter \
        direct_mir_record_field_array_string_with_local; do
    produce_mir "$name"
    native_run "$name"
    for backend in c llvm; do project_and_run "$name" "$backend"; done
done

# A malformed Array row (an Int parameter, a String local) is refused although
# a record field demands that Array too: the canonical row never replaces a
# refused capture.
for forged in direct_mir_record_field_array_int_with_parameter:array-int-value-abi-layout \
        direct_mir_record_field_array_string_with_local:logical-record-array-string-abi-layout; do
    name="${forged%%:*}"; mutation="${forged#*:}"
    "$PYTHON_BIN" "$MUTATIONS" "$WORK_DIR/$name.mir.json" "$mutation" \
        "$WORK_DIR/$name.forged.mir.json"
    for backend in c llvm; do
        output_rel="$WORK_REL/$name.forged.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$name.forged.mir.json" -o "$output_rel") \
            >"$WORK_DIR/$name.forged.$backend.out" 2>"$WORK_DIR/$name.forged.$backend.err"; then
            fail "$backend accepted $name with $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $name with $mutation"
    done
done

echo "[$LABEL] PASS: 5 record-field Array programs equal to native on C (ASan) and LLVM; forged Array<Int> and Array<String> rows refused on both"
