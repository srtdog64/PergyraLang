#!/usr/bin/env bash
# Multiple declaration-owned logical records retain distinct target identities.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-logical-record"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_multi_logical_record.pgy"
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
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
for name in ProbeFact UnusedFact LocalTableFact LocalDocumentFact ObjectTableFact ArrayObjectTableFact; do
    grep -Fq "\"name\":\"$name\"" "$MIR" ||
        fail "producer omitted $name declaration"
done
grep -Fq '"name":"EmptyObjectTable"' "$MIR" || fail "producer omitted zero-parameter record callable"
grep -Fq '"abi_type_name":"ProbeFact","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted the logical-record ABI-absence receipt"
for name in ObjectTableFact ArrayObjectTableFact; do
    grep -Fq "\"abi_type_name\":\"$name\",\"abi_layout_id\":0,\"abi_layout_required\":false,\"abi_layout\":null" "$MIR" ||
        fail "producer omitted $name ABI-absence receipt"
done
grep -Fq '"type":"ObjectTableFact","carriage":"readonly-ref","resource":"none","pass":"indirect"' "$MIR" ||
    fail "producer omitted readonly logical-record carriage"
grep -Fq 'LocalDocumentFact(LocalTableFact(true, json, 2, 8), 0)' "$MIR" ||
    fail "producer omitted the nested logical-record constructor graph"
printf 'ok\n3\nobject\n7\n8\n9\nlocal\n9\n8\nmember-ref\n' >"$WORK_DIR/expected.run"
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
        [[ "$(grep -Fc '} pgy_scalar_logical_record_value_' "$artifact")" == 5 ]] ||
            fail "C artifact did not emit five distinct logical record types"
        for row in 0 1 2 3 4; do
            grep -Fq "pgy_scalar_logical_record_value_$row" "$artifact" ||
                fail "C artifact omitted logical record type $row"
        done
        grep -Fq ').field_3' "$artifact" ||
            fail "C artifact omitted the String member projection"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(void\)' "$artifact" ||
            fail "C artifact omitted the zero-parameter logical-record signature"
        grep -Eq 'const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0' "$artifact" ||
            fail "C artifact omitted the readonly logical-record parameter"
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
        [[ "$(grep -Ec '^%pgy\.scalar\.logical\.record\.value\.[0-4] = type' "$artifact")" == 5 ]] ||
            fail "LLVM artifact did not emit five distinct logical record types"
        grep -Fq 'extractvalue %pgy.scalar.logical.record.value.' "$artifact" ||
            fail "LLVM artifact omitted member projection"
        grep -Eq 'define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(\)' "$artifact" ||
            fail "LLVM artifact omitted the zero-parameter logical-record signature"
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM artifact omitted the readonly logical-record parameter"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done
for mutation in logical-record-field-order logical-record-instruction-layout \
    logical-record-cross-identity logical-record-local-declaration-identity \
        logical-record-readonly-carriage; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] multi-record identity + ABI absence + C/LLVM parity/negatives: PASS"
