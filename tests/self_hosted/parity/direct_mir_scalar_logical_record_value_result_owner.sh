#!/usr/bin/env bash
# Logical-record copy lifecycle and exact member rebinds reach C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_value_result_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"ValidationSession"' "$MIR" ||
    fail "producer omitted the record declaration"
grep -Fq '"type":"ValidationSession","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the record copyout identity"
grep -Fq '"name":"RememberSessionCopy"' "$MIR" ||
    fail "producer omitted the by-value record input routine"
grep -Fq '"type":"IntentCompensationProbe","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the eight-field record copyout identity"
grep -Fq '"name":"IntentStepLines"' "$MIR" ||
    fail "producer omitted the production-shaped composable copyout"
[[ "$(grep -Fo '"type":"IntentCompensationProbe","carriage":"value-result"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted the production-shaped record copyout identity"
grep -Fq '"expr0":"ValidationSession(keys, ids, layouts)"' "$MIR" ||
    fail "producer omitted the whole-record value-result rebind"
for field in keys ids layouts; do
    grep -Fq "\"expr1\":\"session.$field\"" "$MIR" ||
        fail "producer omitted session.$field member rebind"
done
grep -Fq '"arg0":"local_session","arg1":"local"' "$MIR" ||
    fail "producer omitted the local record member rebind"
grep -Fq '"arg0":"scalar","arg1":"local"' "$MIR" ||
    fail "producer omitted the preceding scalar local rebind"
grep -Fq '"expr1":"local_session.keys"' "$MIR" ||
    fail "producer omitted the local member target"
grep -Fq '"expr1":"analysis.expression_surfaces.expression_graph"' "$MIR" && grep -Fq '"name":"ResolveNestedGraph"' "$MIR" && grep -Fq '"call_target_name":"ReplaceNestedGraph"' "$MIR" ||
    fail "producer omitted nested value-result publication"
[[ "$(grep -Fo '"arg0":"ArrayPush"' "$MIR" | wc -l)" -ge 3 ]] ||
    fail "producer omitted ordered collection mutations"
printf 'record-copyout-ready\nrecord-value-input-ready\nintent-step-ready\n1\n7\n1\n1\n9\n' >"$WORK_DIR/expected.run"
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
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0_mutref' "$artifact" ||
            fail "C signature omitted the record mutref"
        grep -Eq 'static int32_t pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, int32_t pgy_param_1, int32_t pgy_param_2, int32_t pgy_param_3, pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_4_mutref\)' "$artifact" ||
            fail "C omitted the eight-field record copyout signature"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_param_0 = \*pgy_param_0_mutref;' "$artifact" ||
            fail "C omitted the record copy-in"
        [[ "$(grep -Ec '\*pgy_param_0_mutref = pgy_local_[0-9]+;' "$artifact")" -ge 2 ]] ||
            fail "C omitted an early/final latest-local record copy-out"
        grep -Fq '*pgy_param_10_mutref = pgy_local_' "$artifact" ||
            fail "C omitted production-shaped record copy-out"
        grep -Eq 'pgy_scalar_routine_[0-9]+\(&pgy_param_0,' "$artifact" ||
            fail "C omitted addressable by-value record call input"
        [[ "$(grep -Ec 'pgy_local_[0-9]+\.field_[0-9]+ = pgy_local_[0-9]+;' "$artifact")" -ge 5 ]] ||
            fail "C omitted ordered logical-record member stores"
        grep -Eq 'pgy_local_[0-9]+\.field_[0-9]+\.field_[0-9]+ = pgy_local_[0-9]+;' "$artifact" ||
            fail "C omitted the nested logical-record member store"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq '%pgy\.scalar\.logical\.record\.value\.[0-9]+ = type \{ %pgy\.array\.string, %pgy\.array\.int, %pgy\.array\.string \}' "$artifact" ||
            fail "LLVM record layout drifted"
        grep -Fq 'ptr %pgy.param.0.mutref' "$artifact" ||
            fail "LLVM signature omitted the record mutref"
        grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, i64 %pgy\.param\.1, i64 %pgy\.param\.2, i64 %pgy\.param\.3, ptr %pgy\.param\.4\.mutref\)' "$artifact" ||
            fail "LLVM omitted the eight-field record copyout signature"
        grep -Eq '%pgy\.param\.0\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM omitted record copy-in storage"
        [[ "$(grep -Fc '%pgy.param.0.record.copyout.' "$artifact")" -ge 4 ]] ||
            fail "LLVM omitted an early/final record copy-out"
        grep -Fq '%pgy.param.10.record.copyout.' "$artifact" ||
            fail "LLVM omitted production-shaped record copy-out"
        grep -Eq '%pgy\.param\.0\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM omitted addressable by-value record call input"
        [[ "$(grep -Fc '%pgy.record.rebind.value.' "$artifact")" -ge 5 ]] ||
            fail "LLVM omitted ordered logical-record member insertvalues"
        grep -Eq ' = insertvalue .*, [0-9]+, [0-9]+' "$artifact" ||
            fail "LLVM omitted the nested logical-record member path"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-copyout-carriage record-copyout-pass \
    value-input-carriage \
    member-field member-rhs-type member-binding member-source-kind \
    member-local-field member-local-ref-missing member-local-ref-foreign \
    member-local-use-missing member-local-use-reordered \
    member-local-result-stale member-local-binding \
    production-copyout-pass production-copyout-abi \
    production-collection-carriage production-readonly-pass \
    production-scalar-carriage nested-member nested-binding nested-rhs-type \
    nested-missing-use; do
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

echo "[$LABEL] logical-record copy-in/out C/LLVM parity + negatives: PASS"
