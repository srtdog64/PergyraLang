#!/usr/bin/env bash
# Declaration-keyed payload-free enum value-parameter C/LLVM parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-payload-free-enum-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_payload_free_enum_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_payload_free_enum_value_parameter.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_fact_owner.pgy"
EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_expression_owner.pgy"
EXPRESSION_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_expression_readiness_owner.pgy"
KIND_IDS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
ROLE_PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
CALLABLE_SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
LLVM_DIRECT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_direct_call_expression_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_payload_free_enum_parameter_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'MirProgramEnumVariantPayloadFreeAt(' "$OWNER" ||
    fail "enum owner does not consume payload-free variant identity"
grep -Fq 'fact.representation != "scalar-ordinal"' "$OWNER" ||
    fail "enum owner does not pin scalar-ordinal representation"
grep -Fq 'payload_free_enum_value_count' "$ROLE_PLAN" ||
    fail "parameter-role plan omits payload-free enum cardinality"
grep -Fq 'DirectMirScalarProgramPayloadFreeEnumParameterReady(' "$ROLE_PLAN" ||
    fail "parameter-role plan omits the declaration-keyed enum role"
grep -Fq 'DirectMirScalarProgramPayloadFreeEnumTypeReady(' "$CALLABLE_SIGNATURE" ||
    fail "callable signature omits the declaration-keyed enum return role"
grep -Fq 'DirectMirScalarProgramPayloadFreeEnumVariantFromGraph(' "$EXPRESSION_OWNER" ||
    fail "enum expression owner omits declaration-owned variant projection"
grep -Fq 'DirectMirScalarProgramPayloadFreeEnumComparisonKindFact(' "$EXPRESSION_OWNER" ||
    fail "enum expression owner omits exact equality classification"
grep -Fq 'DirectMirScalarProgramPayloadFreeEnumReadyForExpressions(' "$EXPRESSION_READY" ||
    fail "enum expression readiness omits declaration identity join"
grep -Fq 'DirectMirScalarProgramExprPayloadFreeEnumVariant() -> Int { return 90; }' "$KIND_IDS" ||
    fail "enum variant expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprEqualPayloadFreeEnum() -> Int { return 91; }' "$KIND_IDS" ||
    fail "enum equality expression identity drifted"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance for enum expressions"
llvm_direct_body="$(awk '/^func DirectMirScalarProgramLlvmDirectCallExpressionAt\(/,/^}/' "$LLVM_DIRECT")"
[[ "$llvm_direct_body" == *'DirectMirScalarProgramPayloadFreeEnumTypeReady('* ]] ||
    fail "LLVM direct-call ABI does not consume payload-free enum identity"
[[ "$llvm_direct_body" != *'DirectMirScalarProgramExprPayloadFreeEnumVariant('* ]] ||
    fail "LLVM direct-call ABI reintroduced node-shape enum inference"
grep -Fq 'ToneOrdinal(EchoTone(Tone.Warm))' "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits a returned enum value consumed by another call"
grep -Fq 'ToneName(EchoTone(Tone.Cool))' "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits a returned enum value passed to a String-return call"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"kind":"enum","nominal_kind":"enum","name":"Tone"' "$MIR" ||
    fail "producer omitted the first enum declaration identity"
grep -Fq '"kind":"enum","nominal_kind":"enum","name":"Direction"' "$MIR" ||
    fail "producer omitted the second enum declaration identity"
grep -Fq '"type":"Tone","carriage":"value"' "$MIR" ||
    fail "producer omitted the Tone value parameter"
grep -Fq '"type":"Direction","carriage":"value"' "$MIR" ||
    fail "producer omitted the Direction value parameter"
printf '1\n0\n1\n0\nwarm\ncool\npayload-free-enum-parameter-ready\n' \
    >"$WORK_DIR/expected.run"

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
        [[ "$(grep -Ec 'static const char\* pgy_scalar_routine_[0-9]+\(int32_t pgy_param_0\) \{$' "$artifact")" == 3 ]] ||
            fail "C artifact omitted the three String-return enum signatures"
        grep -Eq 'static int32_t pgy_scalar_routine_[0-9]+\(int32_t pgy_param_0\)' "$artifact" ||
            fail "C artifact omitted the Int-return enum signature"
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(int32_t pgy_param_0\)' "$artifact" ||
            fail "C artifact omitted the Bool-return enum signature"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        [[ "$(grep -Ec 'define internal ptr @pgy\.scalar\.routine\.[0-9]+\(i64 %pgy\.param\.0\)' "$artifact")" == 3 ]] ||
            fail "LLVM artifact omitted the three String-return enum signatures"
        grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(i64 %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM artifact omitted the Int-return enum signature"
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(i64 %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM artifact omitted the Bool-return enum signature"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in enum-parameter-carriage enum-parameter-physical-abi \
    enum-variant-payload enum-missing-declaration enum-declaration-kind \
    enum-return-collection enum-expression-wrong-owner \
    enum-expression-wrong-variant enum-expression-missing-binding \
    enum-expression-wrong-type enum-match-duplicate-variant; do
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

echo "[$LABEL] payload-free enum parameter/variant/equality C/LLVM parity + negatives: PASS"
