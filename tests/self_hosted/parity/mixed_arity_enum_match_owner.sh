#!/usr/bin/env bash
# Existing enum declaration arity -> scoped match locals -> one C/LLVM plan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
LABEL="self-host-mixed-arity-enum-match"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL:public" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
! grep -Fq 'DirectMirScalarCfgProgramCallableFactReadyWithFacts(' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_fact_owner.pgy" ||
    fail "obsolete payload-free callable validator remains"
grep -Fq 'DirectMirScalarCfgProgramCallableFactReadyWithReferencedEnum(' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_readiness_owner.pgy" ||
    fail "final callable consumer omits the referenced enum fact"
! grep -Fq 'MirRoutineFactIndexSourceLocalType(' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy" ||
    fail "match local types fall back to a routine-wide name lookup"
! grep -Fq 'JsonObjectFactTableFromBounds(' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_match_binding_local_owner.pgy" ||
    fail "match local admission re-parses already owned binding facts"
! grep -Fq 'let local:' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy" ||
    fail "reserved local keyword used as an identity-check variable"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted/mixed_arity_enum
WORK_REL="$(mktemp -d .tmp/self_hosted/mixed_arity_enum/run.XXXXXX)"
WORK="$ROOT_DIR/$WORK_REL"

compile_run() {
    local artifact="$1" backend="$2" expected="$3" binary="$1.exe"
    local command
    if [[ "$backend" == llvm ]]; then
        command=("$CLANG" -x ir "$artifact" -o "$binary")
    else
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
    fi
    "${command[@]}" >"$artifact.compile.out" 2>"$artifact.compile.err" || {
        cat "$artifact.compile.err" >&2; fail "$artifact did not compile";
    }
    "$binary" | tr -d '\r' >"$artifact.run"
    cmp -s "$expected" "$artifact.run" || {
        cat "$artifact.run" >&2; fail "$artifact output mismatch";
    }
}

# A typed Option probe distinguishes missing/ambiguous lookup from present row
# zero. It imports the production owner; no test-local lookup substitutes for it.
printf 'match binding origin owner: ok\n' >"$WORK/origin.expected"
if ! "$DRIVER" --emit-c-artifact-verified \
    tests/self_hosted/parity/fixture/match_binding_origin_owner_probe.pgy \
    "$WORK_REL/origin.c" >"$WORK/origin.emit.out" 2>"$WORK/origin.emit.err"; then
    cat "$WORK/origin.emit.out" "$WORK/origin.emit.err" >&2
    fail "typed match binding origin probe failed source admission"
fi
compile_run "$WORK/origin.c" c "$WORK/origin.expected"

for input in callable mixed enum-local; do
    if [[ "$input" == callable ]]; then
        source_rel="tests/cases/backend_compare/enum_match_payload_basic/main.pgy"
        printf '75\n12\n0\n' >"$WORK/$input.expected"
    elif [[ "$input" == mixed ]]; then
        source_rel="tests/self_hosted/fixtures/direct_mir_payload_enum_mixed_arity.pgy"
        printf '0\n7\n26\n246\n5\n10\n0\n' >"$WORK/$input.expected"
    else
        source_rel="tests/self_hosted/parity/fixture/enum_member_match_local.pgy"
        printf '22\n0\n3\n' >"$WORK/$input.expected"
    fi
    mir_rel="$WORK_REL/$input.mir.json"
    "$DRIVER" --emit-mir-json-verified "$source_rel" -o "$mir_rel" \
        >"$WORK/$input.producer.out" 2>"$WORK/$input.producer.err" || {
        cat "$WORK/$input.producer.out" "$WORK/$input.producer.err" >&2
        fail "$input MIR production"
    }
    [[ -s "$ROOT_DIR/$mir_rel" ]] || fail "$input empty MIR"
    mir_hash="$(sha256sum "$ROOT_DIR/$mir_rel" | awk '{print $1}')"
    "$DRIVER" --mir-json "$mir_rel" >"$WORK/$input.general.c" \
        2>"$WORK/$input.general.err" || {
        cat "$WORK/$input.general.c" "$WORK/$input.general.err" >&2
        fail "$input general MIR consumer";
    }
    compile_run "$WORK/$input.general.c" c "$WORK/$input.expected"
    for backend in c llvm; do
        for route in direct source; do
            artifact_rel="$WORK_REL/$input.$route.$backend"
            if [[ "$route" == direct ]]; then
                command=("$DRIVER" "--mir-json-backend=$backend" "$mir_rel" -o "$artifact_rel")
            else
                command=(env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER"
                    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$source_rel"
                    "--emit-$backend" -o "$artifact_rel")
            fi
            "${command[@]}" >"$ROOT_DIR/$artifact_rel.out" \
                2>"$ROOT_DIR/$artifact_rel.err" || {
                cat "$ROOT_DIR/$artifact_rel.out" "$ROOT_DIR/$artifact_rel.err" >&2
                fail "$input $route/$backend projection"
            }
            if [[ "$route" == source ]]; then
                ! grep -Fq '[pipeline timing]' "$ROOT_DIR/$artifact_rel.out" \
                    "$ROOT_DIR/$artifact_rel.err" || fail "$input/$backend retried native"
            fi
            compile_run "$ROOT_DIR/$artifact_rel" "$backend" "$WORK/$input.expected"
        done
    done
    [[ "$mir_hash" == "$(sha256sum "$ROOT_DIR/$mir_rel" | awk '{print $1}')" ]] ||
        fail "$input MIR changed during consumption"
done

# Prove that the mutator's JSON transport alone remains executable.
roundtrip_rel="$WORK_REL/enum-local-roundtrip.mir.json"
python -B tests/self_hosted/parity/enum_member_match_local_mutations.py \
    "$WORK/enum-local.mir.json" enum-local-roundtrip "$ROOT_DIR/$roundtrip_rel"
for backend in c llvm; do
    artifact_rel="$WORK_REL/enum-local-roundtrip.$backend"
    "$DRIVER" "--mir-json-backend=$backend" "$roundtrip_rel" -o "$artifact_rel" \
        >"$ROOT_DIR/$artifact_rel.out" 2>"$ROOT_DIR/$artifact_rel.err" || {
        cat "$ROOT_DIR/$artifact_rel.out" "$ROOT_DIR/$artifact_rel.err" >&2
        fail "enum-local unchanged JSON transport failed $backend admission"
    }
    compile_run "$ROOT_DIR/$artifact_rel" "$backend" "$WORK/enum-local.expected"
done

for mutation in enum-local-unknown-type enum-local-erased-type enum-local-other-enum enum-local-missing-declaration; do
    case "$mutation" in
        enum-local-unknown-type)
            admission_diagnostic='routine admission stage is invalid: stage=local_inventory ordinal=1' ;;
        enum-local-erased-type|enum-local-other-enum)
            admission_diagnostic='program expression admission is invalid: stage=admitted-type'
            ;;
        enum-local-missing-declaration)
            # Declaration removal declines the scalar route before local
            # admission. This proves only pre-output refusal, not the local
            # identity owner; the terminal currently hides that route receipt.
            admission_diagnostic='direct MIR three-routine structural shape is unsupported' ;;
    esac
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python -B tests/self_hosted/parity/enum_member_match_local_mutations.py \
        "$WORK/enum-local.mir.json" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/rejected-$mutation.$backend"
        if "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel" \
            >"$ROOT_DIR/$output_rel.out" 2>"$ROOT_DIR/$output_rel.err"; then
            fail "$backend accepted $mutation"
        else
            rejection_status=$?
        fi
        [[ "$rejection_status" == 1 ]] || fail "$backend/$mutation abnormal exit $rejection_status"
        grep -Fq 'CODEGEN ERROR:' "$ROOT_DIR/$output_rel.out" \
            "$ROOT_DIR/$output_rel.err" || fail "$backend/$mutation lost its admission diagnostic"
        grep -Fq "$admission_diagnostic" "$ROOT_DIR/$output_rel.out" \
            "$ROOT_DIR/$output_rel.err" || fail "$backend/$mutation did not reach its expected admission boundary"
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation artifact"
    done
done

for mutation in missing-binding-type missing-type-field wrong-binding-type swapped-binding-types \
    duplicate-binding wrong-variant-arity zero-variant-binding zero-variant-invalid-arrays \
    wrong-scrutinee-identity payload-variant-without-arguments cross-arm-binding-use; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python -B tests/self_hosted/parity/mixed_arity_enum_match_mutations.py \
        "$WORK/mixed.mir.json" "$mutation" "$ROOT_DIR/$mutated_rel"
    case "$mutation" in
        duplicate-binding)
            general_diagnostic='[match_binding_duplicate]'
            direct_diagnostic='direct MIR two-routine classification is invalid' ;;
        missing-binding-type|missing-type-field|zero-variant-invalid-arrays)
            general_diagnostic='[match_binding_type_count]'
            direct_diagnostic='direct MIR two-routine classification is invalid' ;;
        wrong-scrutinee-identity)
            general_diagnostic='semantic carried expression identity mismatch'
            direct_diagnostic='stage=leaf-alias' ;;
        payload-variant-without-arguments)
            general_diagnostic='Diagnostic: pgy.selfhost.semantic.v1'
            direct_diagnostic='stage=payload-enum-zero-constructor' ;;
        cross-arm-binding-use)
            general_diagnostic='Diagnostic: pgy.selfhost.semantic.v1'
            direct_diagnostic='stage=leaf-operand' ;;
        *)
            general_diagnostic='Diagnostic: pgy.selfhost.semantic.v1'
            direct_diagnostic='stage=enum-match' ;;
    esac
    if "$DRIVER" --mir-json "$mutated_rel" \
        >"$WORK/rejected-$mutation.general.out" \
        2>"$WORK/rejected-$mutation.general.err"; then
        fail "general MIR consumer accepted $mutation"
    else
        rejection_status=$?
    fi
    [[ "$rejection_status" == 1 ]] || fail "general/$mutation abnormal exit $rejection_status"
    grep -Fq "$general_diagnostic" "$WORK/rejected-$mutation.general.out" \
        "$WORK/rejected-$mutation.general.err" || fail "general/$mutation lost its owned diagnostic"
    ! grep -Fq 'int main' "$WORK/rejected-$mutation.general.out" ||
        fail "general MIR consumer published $mutation C artifact"
    for backend in c llvm; do
        output_rel="$WORK_REL/rejected-$mutation.$backend"
        if "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel" \
            >"$ROOT_DIR/$output_rel.out" 2>"$ROOT_DIR/$output_rel.err"; then
            fail "$backend accepted $mutation"
        else
            rejection_status=$?
        fi
        [[ "$rejection_status" == 1 ]] || fail "$backend/$mutation abnormal exit $rejection_status"
        grep -Fq "$direct_diagnostic" "$ROOT_DIR/$output_rel.out" \
            "$ROOT_DIR/$output_rel.err" || fail "$backend/$mutation lost its owned diagnostic"
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation artifact"
    done
done
echo "[$LABEL] public source + one MIR general/C/LLVM, roundtrip controls, six enum-local owner refusals, two declaration-route refusals, mixed scoped negatives: PASS ($WORK_REL)"
