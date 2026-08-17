#!/usr/bin/env bash
# Record-array and ArrayInt copyouts share one mixed Void signature boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-array-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_array_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_array_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_result_policy_owner.pgy"
TARGET="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_target_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'let mixed_copyout: Bool = record_array_count == 1' "$POLICY" ||
    fail "mixed signature policy does not require both copyout families"
grep -Fq 'layout.llvm_aggregate_type' "$TARGET" ||
    fail "LLVM projection does not consume the nominal-array layout owner"
grep -Fq 'layout.field_order != "data,len,cap"' "$TARGET" ||
    fail "target projection does not reject the public four-field Array shape"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"InventoryRow"' "$MIR" ||
    fail "producer omitted the record declaration"
grep -Fq '"type":"Array<InventoryRow>","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the record-array copyout identity"
grep -Fq '"abi_type_name":"Array<InventoryRow>","abi_layout_id":0,"abi_layout_required":false' "$MIR" ||
    fail "producer attached a public physical ABI to the compiler-owned array"
grep -Fq '"type":"Array<Int>","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the ArrayInt copyout identity"
grep -Fq '"arg0":"ArrayPush"' "$MIR" ||
    fail "producer omitted the record-array push"
grep -Fq 'InventoryRow(7, label)' "$MIR" ||
    fail "producer omitted the constructed record value"
grep -Fq 'ArraySet(rows, 0, ReplacementRow(label))' "$MIR" ||
    fail "producer omitted the direct-call record-array set"
grep -Fq '"name":"rows","type":"Array<InventoryRow>"' "$MIR" ||
    fail "producer omitted the local record-array identity"
grep -Fq '"name":"ParseDestructureLetStmt"' "$MIR" ||
    fail "producer omitted the composable String-return routine"
grep -Fq '"type":"Array<AstExpressionGraphRows>","carriage":"value-result","resource":"none","pass":"direct","abi_type_name":"Array<AstExpressionGraphRows>","abi_layout_id":0,"abi_layout_required":false' "$MIR" ||
    fail "producer omitted the compiler-owned graph-row copyout identity"
printf 'logical-record-array-copyout-ready\n1\ngraph-row-copyout-ready\n5\n0\n' >"$WORK_DIR/expected.run"

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
        grep -Fq 'typedef struct { InventoryRow *data; long long len; long long cap; } pgy_InventoryRow_array;' "$artifact" ||
            fail "C artifact omitted the three-field record-array type"
        grep -Eq 'pgy_InventoryRow_array \*pgy_param_1_mutref, pgy_ai \*pgy_param_2_mutref' "$artifact" ||
            fail "C signature omitted either copyout pointer"
        grep -Fq 'pgy_InventoryRow_array pgy_param_1 = *pgy_param_1_mutref;' "$artifact" ||
            fail "C artifact omitted record-array copy-in"
        grep -Fq '*pgy_param_1_mutref = pgy_param_1;' "$artifact" ||
            fail "C artifact omitted record-array copy-out"
        grep -Fq '*pgy_param_2_mutref = pgy_param_2;' "$artifact" ||
            fail "C artifact omitted ArrayInt copy-out"
        grep -Eq 'static void pgy_scalar_logical_record_array_push_[0-9]+\(' "$artifact" ||
            fail "C artifact omitted the typed record-array push owner"
        grep -Eq 'pgy_scalar_logical_record_array_push_[0-9]+\(&pgy_param_1, \(pgy_scalar_logical_record_value_[0-9]+\)' "$artifact" ||
            fail "C artifact omitted the constructed record push call"
        grep -Eq 'static void pgy_scalar_logical_record_array_set_[0-9]+\(' "$artifact" ||
            fail "C artifact omitted the guarded record-array set owner"
        grep -Eq 'pgy_scalar_logical_record_array_set_[0-9]+\(&pgy_param_1, 0LL, pgy_scalar_routine_[0-9]+\(' "$artifact" ||
            fail "C artifact omitted the direct-call record set"
        grep -Eq 'pgy_InventoryRow_array pgy_local_[0-9]+ = \{0\};' "$artifact" ||
            fail "C artifact omitted local record-array storage"
        grep -Eq 'pgy_local_[0-9]+ = \(\(pgy_InventoryRow_array\)\{0\}\);' "$artifact" ||
            fail "C artifact omitted the local record-array empty literal"
        grep -Fq 'pgy_AstExpressionGraphRows_array *pgy_param_4_mutref' "$artifact" ||
            fail "C signature omitted the composable graph-row copyout"
        grep -Fq 'pgy_AstExpressionGraphRows_array pgy_param_4 = *pgy_param_4_mutref;' "$artifact" ||
            fail "C artifact omitted graph-row copy-in"
        grep -Fq '*pgy_param_4_mutref = pgy_param_4;' "$artifact" ||
            fail "C artifact omitted graph-row copy-out"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq '%pgy\.scalar\.logical\.record\.array\.[0-9]+ = type \{ ptr, i64, i64 \}' "$artifact" ||
            fail "LLVM artifact omitted the three-field record-array type"
        grep -Fq 'ptr %pgy.param.1.mutref, ptr %pgy.param.2.mutref' "$artifact" ||
            fail "LLVM signature omitted either copyout pointer"
        grep -Eq '%pgy\.param\.1\.local = alloca %pgy\.scalar\.logical\.record\.array\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted record-array copy-in storage"
        grep -Fq '%pgy.param.1.array.copyout.' "$artifact" ||
            fail "LLVM artifact omitted record-array copy-out"
        grep -Eq 'store %pgy\.array\.int %pgy\.param\.2\.copyout\.[0-9]+, ptr %pgy\.param\.2\.mutref' "$artifact" ||
            fail "LLVM artifact omitted ArrayInt copy-out"
        grep -Eq 'define internal void @pgy\.scalar\.logical\.record\.array\.push\.[0-9]+\(' "$artifact" ||
            fail "LLVM artifact omitted the typed record-array push owner"
        grep -Eq 'call void @pgy\.scalar\.logical\.record\.array\.push\.[0-9]+\(ptr %pgy\.param\.1\.local, %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted the constructed record push call"
        grep -Eq 'define internal void @pgy\.scalar\.logical\.record\.array\.set\.[0-9]+\(' "$artifact" ||
            fail "LLVM artifact omitted the guarded record-array set owner"
        grep -Eq 'call void @pgy\.scalar\.logical\.record\.array\.set\.[0-9]+\(ptr %pgy\.param\.1\.local, i64 0, %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted the direct-call record set"
        grep -Eq '%pgy\.local\.[0-9]+ = alloca %pgy\.scalar\.logical\.record\.array\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted local record-array storage"
        grep -Eq 'store %pgy\.scalar\.logical\.record\.array\.[0-9]+ zeroinitializer, ptr %pgy\.local\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted the local record-array empty literal"
        grep -Fq '%pgy.param.4.local = alloca %pgy.scalar.logical.record.array.' "$artifact" ||
            fail "LLVM artifact omitted graph-row copy-in storage"
        grep -Fq '%pgy.param.4.array.copyout.' "$artifact" ||
            fail "LLVM artifact omitted graph-row copy-out"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-array-copyout-carriage \
    graph-row-copyout-carriage graph-row-copyout-missing-element \
    graph-row-copyout-physical-abi \
    record-array-missing-element record-array-physical-abi \
    record-array-push-binding record-array-push-field-type \
    record-array-push-source-kind record-array-local-missing-element \
    record-array-local-literal-shape record-array-set-binding \
    record-array-set-index-type record-array-set-value-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] mixed record-array/ArrayInt copyout C/LLVM parity + negatives: PASS"
