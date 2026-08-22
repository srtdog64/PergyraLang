#!/usr/bin/env bash
# Two readonly records plus one logical-record copyout C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-readonly-record-single-copyout"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_readonly_record_single_copyout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_readonly_logical_record_single_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_single_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'let readonly: Bool = carriage == "readonly-ref"' "$POLICY" ||
    fail "signature owner omitted readonly record admission"
grep -Fq 'return copyout_count >= 1;' "$POLICY" ||
    fail "signature owner does not pin positive record copyouts"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
[[ "$(grep -Eo '"type":"(Left|Right)BranchView","carriage":"readonly-ref","resource":"none","pass":"indirect"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted two readonly record inputs"
grep -Fq '"type":"EmissionOrder","carriage":"value-result","resource":"none","pass":"direct"' "$MIR" ||
    fail "producer omitted the single record copyout"
printf 'readonly-copyout-ready\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        signature="$(grep -E '^static const char\* pgy_scalar_routine_[0-9]+\(' "$artifact" | grep -F 'pgy_param_4_mutref' | head -n 1)"
        [[ "$(grep -Eo 'const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_[01]' <<<"$signature" | wc -l)" -eq 2 ]] || fail "C omitted readonly records"
        [[ "$signature" == *'int32_t pgy_param_2, int32_t pgy_param_3'* ]] || fail "C omitted direct scalar inputs"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_param_4 = \*pgy_param_4_mutref;' "$artifact" || fail "C omitted record copy-in"
        grep -Fq '*pgy_param_4_mutref = pgy_param_4;' "$artifact" || fail "C omitted record copyout"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal ptr @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, i64 %pgy\.param\.2, i64 %pgy\.param\.3, ptr %pgy\.param\.4\.mutref\)' "$artifact" || fail "LLVM omitted exact signature"
        grep -Eq '%pgy\.param\.4\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" || fail "LLVM omitted record copy-in"
        grep -Fq '%pgy.param.4.record.copyout.' "$artifact" || fail "LLVM omitted record copyout"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for variant in readonly-value merged-copyout-value; do
    mutated_rel="$WORK_REL/$variant.mir.json"
    python "$MUTATIONS" "$MIR" "$variant" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$variant.$backend"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$variant.$backend.out" 2>"$WORK_DIR/$variant.$backend.err" ||
            fail "$backend rejected composable $variant"
        [[ -s "$ROOT_DIR/$output_rel" ]] || fail "$backend omitted composable $variant"
    done
done

for mutation in readonly-carriage readonly-pass merged-copyout-carriage \
    merged-copyout-pass; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] two readonly records + one record copyout C/LLVM parity + negatives: PASS"
