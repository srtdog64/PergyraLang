#!/usr/bin/env bash
# ABI-free declaration-keyed logical records retain identity through Option<T>.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-logical-record-option-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_logical_record_option_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_option_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_logical_record_option_return_mutations.py"
FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_option_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_option_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT_OWNER" "$C_OWNER" "$LLVM_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'OptionPayloadTypeOpt(referenced_type)' "$FACT_OWNER" ||
    fail "logical-record inventory does not unwrap Option payload identity"
grep -Fq 'DirectMirScalarProgramLogicalRecordOptionPayloadType(' "$FACT_OWNER" ||
    fail "logical-record Option identity has no fact owner"
for owner in "$FACT_OWNER" "$C_OWNER" "$LLVM_OWNER"; do
    ! grep -Fq 'ProbeFact' "$owner" || fail "owner is fixture-name keyed"
    ! grep -Fq 'WrapProbe' "$owner" || fail "owner is routine-name keyed"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"ProbeFact"' "$MIR" || fail "record declaration missing"
grep -Fq '"abi_type_name":"Option<ProbeFact>","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted ABI-free logical-record Option carrier"

printf 'record-option\nwrapped\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'pgy_scalar_option_logical_record_[0-9]+' "$artifact" ||
            fail "C artifact omitted logical-record Option representation"
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
        grep -Eq '^%pgy\.scalar\.option\.logical\.record\.[0-9]+ = type' "$artifact" ||
            fail "LLVM artifact omitted logical-record Option representation"
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

for mutation in return-carrier local-carrier record-physical-abi; do
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

echo "[$LABEL] ABI-free logical-record Option C/LLVM parity + negatives: PASS"
