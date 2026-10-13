#!/usr/bin/env bash
# Option<Int> callables over direct scalars and logical-record inputs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-option-int"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_option_int"; WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_option_int_callables.pgy"; MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"; PARAM_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_direct_scalar_mutations.py"; RECORD_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_option_int_logical_record_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$MUTATIONS" "$PARAM_MUTATIONS" "$RECORD_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"abi_type_name":"Option<Int>"' "$MIR" ||
    fail "producer emitted no Option<Int> ABI receipt"
grep -Fq '"name":"FindFrom"' "$MIR" ||
    fail "producer omitted mixed direct scalar Option<Int> callable"
grep -Fq '"name":"FindField"' "$MIR" ||
    fail "producer omitted readonly-record Option<Int> callable"
grep -Fq '"name":"FindValue"' "$MIR" ||
    fail "producer omitted value-record Option<Int> callable"
grep -Fq '"name":"RelayOption"' "$MIR" || fail "producer omitted by-value Option<Int> callable"
grep -Fq '"type":"Option<Int>","carriage":"value"' "$MIR" || fail "producer omitted by-value Option<Int> parameter ABI"
grep -Fq '"type":"JsonIndexView","carriage":"value"' "$MIR" ||
    fail "producer omitted value-record parameter role"
grep -Fq '"name":"relayed","type":"Option<Int>"' "$MIR" ||
    fail "producer omitted the Option<Int> source-local boundary"

printf '11\n5\n6\n7\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'typedef struct {' "$artifact" ||
            fail "C artifact omitted the Option<Int> representation"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, const char\* pgy_param_1, int32_t pgy_param_2\)' "$artifact" ||
            fail "C Option<Int> direct scalar signature drifted"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0, const char\* pgy_param_1\)' "$artifact" ||
            fail "C Option<Int> readonly-record signature drifted"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(pgy_scalar_option_int pgy_param_0\)' "$artifact" || fail "C Option<Int> value signature drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Fq '%pgy.scalar.option.int = type { i32, i32 }' "$artifact" ||
            fail "LLVM artifact omitted the admitted Option<Int> representation"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, i64 %pgy\.param\.2\)' "$artifact" ||
            fail "LLVM Option<Int> direct scalar signature drifted"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM Option<Int> readonly-record signature drifted"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.option\.int %pgy\.param\.0\)' "$artifact" || fail "LLVM Option<Int> value signature drifted"
        # UnwrapOption's failing path reports through the runtime panic export.
        grep -Fq 'declare void @pgy_runtime_panic_internal_invariant_export(ptr)' "$artifact" ||
            fail "LLVM Option<Int> unwrap lost the runtime panic ABI"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
            -o "$WORK_DIR/runtime.o" || fail "runtime object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in carriage pass-shape resource abi-required; do
    mutated_rel="$WORK_REL/option-record-$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$RECORD_MUTATIONS" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-record-$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/option-record-$mutation.$backend.out" \
            2>"$WORK_DIR/option-record-$mutation.$backend.err"; then
            fail "$backend accepted Option<Int> record $mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published Option<Int> record $mutation"
    done
done

mutated_rel="$WORK_REL/option-abi-layout.mir.json"
mutated="$ROOT_DIR/$mutated_rel"
python "$MUTATIONS" "$MIR" option-abi-layout "$mutated"
[[ -s "$mutated" ]] || fail "could not create Option<Int> ABI mutation"
for backend in c llvm; do
    output_rel="$WORK_REL/option-abi-layout.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") \
        >"$WORK_DIR/mutated.$backend.out" \
        2>"$WORK_DIR/mutated.$backend.err"; then
        fail "$backend accepted a mutated Option<Int> ABI layout"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published an artifact for a mutated Option<Int> ABI"
done

for mutation in carriage pass resource abi-required; do
    mutated_rel="$WORK_REL/option-direct-$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$PARAM_MUTATIONS" "$MIR" "$mutation" "$mutated" FindFrom
    [[ -s "$mutated" ]] || fail "could not create $mutation mutation"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-direct-$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/option-direct-$mutation.$backend.out" \
            2>"$WORK_DIR/option-direct-$mutation.$backend.err"; then
            fail "$backend accepted Option<Int> $mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published Option<Int> $mutation"
    done
done

for mutation in abi-layout carriage pass-shape; do
    mutated_rel="$WORK_REL/option-value-$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "option-int-value-$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-value-$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/option-value-$mutation.$backend.out" 2>"$WORK_DIR/option-value-$mutation.$backend.err"; then fail "$backend accepted Option<Int> value $mutation"; fi
    done
done
echo "[$LABEL] direct-scalar/value-or-readonly-record Option<Int> C/LLVM parity + negatives: PASS"
