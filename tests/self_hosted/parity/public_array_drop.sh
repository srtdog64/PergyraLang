#!/usr/bin/env bash
# Public storage release: executed positives and diagnostic-specific refusals.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
STAGE="${PGY_ARRAY_DROP_STAGE:-all}"
LABEL="public-array-drop"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ "$STAGE" == all || "$STAGE" == native || "$STAGE" == mir ]] || fail "unknown stage $STAGE"
pgy_require_runnable_binary_here "$LABEL" "$PGY"
if [[ "$STAGE" != native ]]; then
    pgy_require_runnable_binary_here "$LABEL" "$DRIVER"
    export PGY_SELF_DRIVER_BIN="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
fi
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/public-array-drop.XXXXXX")"
WORK_REL=".tmp/self_hosted/${WORK_DIR##*/}"
FIXTURES="tests/concept_semantics/hashmap"
echo "[$LABEL] stage=$STAGE evidence=$WORK_REL launcher=$PGY driver=$DRIVER"
lanes=(native)
[[ "$STAGE" == all ]] && lanes+=(public)
if [[ "$STAGE" != mir ]]; then
for name in scalar record_empty early_return own own_bool loop read_borrow own_pair ref_result_plain inout_mutation inout_nested inout_write; do
    expected=$'7\ntrue'
    [[ "$name" == record_empty ]] && expected=9
    [[ "$name" == early_return ]] && expected=$'4\n8'
    [[ "$name" == own ]] && expected=5
    [[ "$name" == own_bool ]] && expected=true
    [[ "$name" == loop ]] && expected=$'2\n2\n2'
    [[ "$name" == read_borrow ]] && expected=3
    [[ "$name" == own_pair ]] && expected=$'3\n4'
    [[ "$name" == ref_result_plain ]] && expected='ARRAY RESULT READ RELEASE PASS'
    [[ "$name" == inout_mutation ]] && expected='ARRAY INOUT RELEASE PASS'
    [[ "$name" == inout_nested ]] && expected='ARRAY NESTED INOUT RELEASE PASS'
    [[ "$name" == inout_write ]] && expected=7
    for lane in "${lanes[@]}"; do
        for backend in c llvm; do
            command=("$PGY")
            [[ "$lane" == native ]] && command+=(--native-pipeline)
            output="$WORK_REL/$name-$lane-$backend.exe"
            (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "${command[@]}" "$FIXTURES/public_array_drop_$name.pgy" \
                "--backend=$backend" -o "$output") >"$WORK_DIR/$name-$lane-$backend.build" 2>&1 ||
                fail "$name $lane $backend build refused; see evidence"
            "$ROOT_DIR/$output" >"$WORK_DIR/$name-$lane-$backend.run"
            actual="$(tr -d '\r' <"$WORK_DIR/$name-$lane-$backend.run")"
            [[ "$actual" == "$expected" ]] || fail "$name $lane $backend output mismatch"
        done
    done
done
# Populated logical-record arrays are executable in native C/LLVM and public
# source-C. Public LLVM currently admits only the empty logical-record-array
# frontier, so do not confuse its pre-release ArrayPush limitation with the
# storage-release claim above.
record_lanes=(native-c native-llvm)
[[ "$STAGE" != all ]] || record_lanes+=(public-c)
for name in record read_value inout_record; do
for lane_backend in "${record_lanes[@]}"; do
    lane="${lane_backend%-*}"
    backend="${lane_backend#*-}"
    command=("$PGY")
    [[ "$lane" == native ]] && command+=(--native-pipeline)
    output="$WORK_REL/$name-$lane-$backend.exe"
    expected=9
    [[ "$name" != read_value ]] || expected=$'6\n10'
    [[ "$name" != inout_record ]] || expected='ARRAY RECORD INOUT RELEASE PASS'
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "${command[@]}" "$FIXTURES/public_array_drop_$name.pgy" \
        "--backend=$backend" -o "$output") >"$WORK_DIR/$name-$lane-$backend.build" 2>&1 ||
        fail "$name $lane $backend build refused; see evidence"
    "$ROOT_DIR/$output" >"$WORK_DIR/$name-$lane-$backend.run"
    [[ "$(tr -d '\r' <"$WORK_DIR/$name-$lane-$backend.run")" == "$expected" ]] ||
        fail "$name $lane $backend output mismatch"
done
done
for name in negative double_negative default_negative ref_negative inout_negative \
    alias_negative alias_receiver_negative escape_negative slice_negative temporary_negative \
    owned_string_negative nested_resource_negative conditional_negative private_negative shadow_negative \
    own_alias_negative builtin_alias_negative call_result_negative option_escape_negative \
    enum_escape_negative own_conditional_negative return_alias_escape_negative inout_rebind_negative literal_escape_negative \
    own_double_negative own_duplicate_argument_negative own_borrow_argument_negative ref_result_resource_negative \
    inout_alias_negative inout_nested_rebind_negative; do
    source="$FIXTURES/public_array_drop_$name.pgy"
    [[ "$name" == negative ]] && source="$FIXTURES/public_array_drop_negative.pgy"
    for lane in "${lanes[@]}"; do
        command=("$PGY" --error-format=json)
        [[ "$lane" == native ]] && command+=(--native-pipeline)
        output="$WORK_REL/$name-$lane.c"
        printf 'preserved:%s:%s\n' "$name" "$lane" >"$ROOT_DIR/$output"
        if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "${command[@]}" "$source" --backend=c --emit-c -o "$output") \
            >"$WORK_DIR/$name-$lane.out" 2>"$WORK_DIR/$name-$lane.err"; then
            fail "$lane accepted $name"
        fi
        [[ "$(tr -d '\r\n' <"$ROOT_DIR/$output")" == "preserved:$name:$lane" ]] ||
            fail "$lane replaced rejected artifact $name"
        pattern='(borrow_boundary_escape|BORROW_ESCAPE|ArrayDrop (requires|cannot|storage|release|conditional))'
        case "$name" in
            negative|double_negative|own_double_negative|own_duplicate_argument_negative|own_borrow_argument_negative|conditional_negative|own_conditional_negative)
                pattern='(move_from_released|MOVE_FROM_RELEASED|ArrayDrop conditional)' ;;
            slice_negative) pattern='(slice_storage_invalidation|SLICE_STORAGE_INVALIDATION|ArrayDrop cannot change Array storage.*Slice)' ;;
            temporary_negative) pattern='(array_drop_storage_requires_binding|ArrayDrop requires one named)' ;;
            owned_string_negative|nested_resource_negative) pattern='ArrayDrop requires plain value elements' ;;
            private_negative) pattern='(compiler_internal_builtin|CompilerRetireArrayStorage is restricted)' ;;
            shadow_negative) pattern='(reserved_builtin_name|builtin_name_reserved|PGY_SEM_REDECLARATION)' ;;
        esac
        grep -Eqi "$pattern" "$WORK_DIR/$name-$lane.out" "$WORK_DIR/$name-$lane.err" ||
            fail "$lane refusal lacked release diagnostic for $name"
    done
done
fi
if [[ "$STAGE" != native ]]; then
    pgy_selfhost_select_linked_runtime_compile_profile || fail "runtime compiler profile is invalid"
    "${PGY_SELFHOST_CLANG:-clang}" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
        "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
        -o "$WORK_DIR/runtime.o" >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
        { cat "$WORK_DIR/runtime.compile.err" >&2; fail "runtime object compilation failed; evidence=$WORK_REL"; }
    for name in scalar own own_bool own_pair inout_write; do
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$FIXTURES/public_array_drop_$name.pgy" -o "$WORK_REL/$name.mir.json") \
            >"$WORK_DIR/$name.mir.out" 2>"$WORK_DIR/$name.mir.err" ||
            fail "$name public MIR producer refused"
        expected=5
        [[ "$name" == scalar ]] && expected=$'7\ntrue'
        [[ "$name" == own_bool ]] && expected=true
        [[ "$name" == own_pair ]] && expected=$'3\n4'
        [[ "$name" == inout_write ]] && expected=7
        mir_hash="$(sha256sum "$WORK_DIR/$name.mir.json" | cut -d' ' -f1)"
        for backend in c llvm; do
            suffix=c
            [[ "$backend" == llvm ]] && suffix=ll
            artifact="$WORK_REL/$name-direct.$suffix"
            executable="$WORK_DIR/$name-direct-$backend.exe"
            (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$WORK_REL/$name.mir.json" -o "$artifact") \
                >"$WORK_DIR/$name-direct-$backend.out" 2>"$WORK_DIR/$name-direct-$backend.err" ||
                fail "$name direct $backend projection refused"
            compile=("${PGY_SELFHOST_CC:-gcc}" -std=c11 -O0 -fwrapv -fno-strict-aliasing \
                "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread "$ROOT_DIR/$artifact")
            [[ "$backend" == llvm ]] && compile=("${PGY_SELFHOST_CLANG:-clang}" -x ir "$ROOT_DIR/$artifact" \
                -x none "$WORK_DIR/runtime.o" -pthread -lm)
            "${compile[@]}" -o "$executable" >"$WORK_DIR/$name-direct-$backend.compile" 2>&1 ||
                fail "$name direct $backend compilation failed"
            "$executable" >"$WORK_DIR/$name-direct-$backend.run"
            [[ "$(tr -d '\r' <"$WORK_DIR/$name-direct-$backend.run")" == "$expected" ]] ||
                fail "$name direct $backend output mismatch"
            [[ "$(sha256sum "$WORK_DIR/$name.mir.json" | cut -d' ' -f1)" == "$mir_hash" ]] ||
                fail "$name direct $backend mutated producer MIR"
        done
    done
    "${PYTHON_BIN:-python3}" "$ROOT_DIR/tests/self_hosted/parity/public_array_drop_mir_mutations.py" \
        "$WORK_DIR/scalar.mir.json" "$WORK_DIR/own.mir.json" "$WORK_DIR/own_pair.mir.json" "$WORK_DIR"
    for mutation in mir-double-drop mir-use-after-drop mir-borrowed-forward mir-use-after-transfer mir-duplicate-own-argument; do
        for backend in c llvm; do
            output="$WORK_REL/$mutation.$backend"
            printf 'preserved:%s:%s\n' "$mutation" "$backend" >"$ROOT_DIR/$output"
            if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$WORK_REL/$mutation.mir.json" -o "$output") \
                >"$WORK_DIR/$mutation-$backend.out" 2>"$WORK_DIR/$mutation-$backend.err"; then
                fail "direct $backend accepted $mutation"
            fi
            [[ "$(tr -d '\r\n' <"$ROOT_DIR/$output")" == "preserved:$mutation:$backend" ]] ||
                fail "direct $backend replaced rejected artifact $mutation"
            grep -Fq 'program_readiness=28' \
                "$WORK_DIR/$mutation-$backend.out" "$WORK_DIR/$mutation-$backend.err" ||
                fail "direct $backend refusal did not reach storage lifetime admission for $mutation"
        done
    done
fi
if [[ "$STAGE" == mir ]]; then
    echo "[$LABEL] PASS stage=mir (5 issued MIR inputs executed and 5 preserved-artifact refusals per C/LLVM backend; no source gate claimed)"
else
    echo "[$LABEL] PASS stage=$STAGE (native C/LLVM: 15 positives each; all-stage source-C: 15 and public LLVM: 12; 30 preserved-artifact refusals per lane; all-stage also executes 5 issued MIR inputs and checks 5 MIR refusals per backend)"
fi
