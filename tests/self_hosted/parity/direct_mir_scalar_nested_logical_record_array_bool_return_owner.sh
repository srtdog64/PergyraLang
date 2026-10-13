#!/usr/bin/env bash
# Nested logical-record returns join declaration identity with Array<Bool> ABI.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-nested-logical-record-array-bool-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_nested_logical_record_array_bool_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_nested_logical_record_array_bool_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$MUTATIONS"; do
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
for name in ReachabilityRows ProgramIndex WrongProgramIndex; do
    grep -Fq "\"name\":\"$name\"" "$MIR" ||
        fail "producer omitted $name declaration"
done
grep -Fq '"type":"Array<Bool>"' "$MIR" ||
    fail "producer omitted Array<Bool> field identity"
grep -Fq '"abi_type_name":"Array<Bool>"' "$MIR" ||
    fail "producer omitted Array<Bool> instruction ABI"
grep -Fq 'ReachabilityRows(true, [true])' "$MIR" ||
    fail "producer omitted the nested populated Array<Bool> constructor"
grep -Fq '"type":"ProgramIndex","carriage":"value","resource":"none","pass":"direct"' "$MIR" ||
    fail "producer omitted by-value ProgramIndex parameter ABI"
python -c 'import json,sys; d=json.load(open(sys.argv[1],encoding="utf-8")); rows=[i for r in d["routines"] for b in r["blocks"] for i in b["instructions"] if i.get("expr0")=="(!index.rows.valid)"]; assert len(rows)==1 and rows[0]["uses"]==["index.1"]' "$MIR" ||
    fail "member-selector spelling leaked into local SSA uses"
printf '0\n8\n' >"$WORK_DIR/expected.run"

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
        grep -Fq 'typedef struct { bool *data; size_t length; size_t capacity; void *allocator; } pgy_ab;' "$artifact" ||
            fail "C artifact omitted the sealed Array<Bool> carrier"
        [[ "$(grep -Fc '} pgy_scalar_logical_record_value_' "$artifact")" == 2 ]] ||
            fail "C artifact did not exclude the unreferenced distractor"
        grep -Fq 'bool field_0; pgy_ab field_1;' "$artifact" ||
            fail "C artifact omitted nested Array<Bool> fields"
        grep -Fq 'bool field_0; pgy_scalar_logical_record_value_0 field_1; int64_t field_2;' "$artifact" ||
            fail "C artifact omitted dependency-ordered ProgramIndex fields"
        grep -Eq 'pgy_scalar_logical_record_value_1 pgy_param_[0-9]+' "$artifact" ||
            fail "C signature omitted the by-value ProgramIndex parameter"
        ! grep -Eq 'pgy_scalar_logical_record_value_1 \*pgy_param_' "$artifact" ||
            fail "C by-value ProgramIndex parameter became indirect"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "C artifact did not compile"
            }
    else
        grep -Fq '%pgy.array.bool = type { ptr, i64, i64, ptr }' "$artifact" ||
            fail "LLVM artifact omitted the sealed Array<Bool> carrier"
        grep -Fq '%pgy.scalar.logical.record.value.0 = type { i1, %pgy.array.bool }' "$artifact" ||
            fail "LLVM artifact omitted nested Array<Bool> fields"
        grep -Fq '%pgy.scalar.logical.record.value.1 = type { i1, %pgy.scalar.logical.record.value.0, i64 }' "$artifact" ||
            fail "LLVM artifact omitted dependency-ordered ProgramIndex fields"
        [[ "$(grep -Fc '%pgy.scalar.logical.record.value.' "$artifact")" -ge 2 ]] ||
            fail "LLVM artifact omitted logical-record declarations"
        grep -Eq '@pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.value\.1 %pgy\.param\.[0-9]+' "$artifact" ||
            fail "LLVM signature omitted the by-value ProgramIndex parameter"
        ! grep -Fq '@pgy_ab_get' "$artifact" ||
            fail "LLVM emitted an unused ArrayBool get helper"
        ! grep -Fq 'pgy_runtime_panic_out_of_bounds_export' "$artifact" ||
            fail "LLVM emitted an unused bounds-panic dependency"
        "$CLANG" -x ir "$artifact" -lm -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in logical-record-nested-array-bool-field-order \
        logical-record-nested-array-bool-cross-identity \
        logical-record-nested-array-bool-abi-layout \
        logical-record-nested-value-pass-shape; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] nested identity + Array<Bool> ABI + value return C/LLVM parity/negatives: PASS"
