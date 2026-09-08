#!/usr/bin/env bash
# Open semantic-admission claims. This gate must stay RED while any claim is
# false; it has no expected-failure allowance and is not in the green runner.
# Invalid inputs are checked only through source-to-MIR, never executed.
# Positive nominal controls also verify execution, so an overbroad refusal or
# accidental value/identity reinterpretation cannot make this gate green.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here concept-source-admission "$PGY"
pgy_require_runnable_binary_here concept-source-admission "$DRIVER"
SCRATCH="$ROOT_DIR/.tmp/self_hosted/concept_semantics_20260905/source_admission"
mkdir -p "$SCRATCH"
RUN_DIR="$(mktemp -d "$SCRATCH/run.XXXXXX")"
cd "$ROOT_DIR"
failures=0
checks=0

checks=$((checks + 1))
if ! "$BASH" "$ROOT_DIR/tests/mir_lexical_binding_owner_smoke.sh"; then
    failures=$((failures + 1))
fi

while IFS='|' read -r source diagnostic owned_code; do
    case_name="$(basename "$source" .pgy)"
    name="${source#tests/concept_semantics/}"
    name="${name%.pgy}"
    name="${name//\//__}"
    checks=$((checks + 1))
    native_status=0
    timeout 60 "$PGY" --native-pipeline --mir-json --error-format=json "$source" \
        >"$RUN_DIR/$name.native.out" 2>"$RUN_DIR/$name.native.err" || native_status=$?
    if [[ "$native_status" != 1 ]] || [[ -s "$RUN_DIR/$name.native.out" ]] || \
        ! grep -Fq "$diagnostic" "$RUN_DIR/$name.native.err"; then
        echo "[concept-source-admission] INVALID CONTROL: $name (native status $native_status)" >&2
        failures=$((failures + 1))
        continue
    fi
    self_status=0
    timeout 60 "$DRIVER" --emit-mir-json-verified "$source" \
        >"$RUN_DIR/$name.self.out" 2>"$RUN_DIR/$name.self.err" || self_status=$?
    if [[ "$self_status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$RUN_DIR/$name.self.out"; then
        echo "[concept-source-admission] FAIL: $name (self status $self_status; must reject without MIR)" >&2
        failures=$((failures + 1))
    elif [[ ! -s "$RUN_DIR/$name.self.out" && ! -s "$RUN_DIR/$name.self.err" ]]; then
        echo "[concept-source-admission] FAIL: $name rejected without a diagnostic" >&2
        failures=$((failures + 1))
    elif [[ -n "$owned_code" ]] &&
        ! grep -Fq "Code: $owned_code" "$RUN_DIR/$name.self.out" "$RUN_DIR/$name.self.err"; then
        echo "[concept-source-admission] FAIL: $name refused for an unrelated owned verdict" >&2
        failures=$((failures + 1))
    elif [[ "$diagnostic" == PGY_SEM_IMMUTABLE_FIELD_WRITE ]] &&
        ! grep -Fq 'Code: immutable_field_write' "$RUN_DIR/$name.self.out" "$RUN_DIR/$name.self.err"; then
        echo "[concept-source-admission] FAIL: $name refused for an unrelated reason" >&2
        failures=$((failures + 1))
    elif [[ "$case_name" == clock_wrong_capability ]] &&
        ! grep -Fq 'Code: declared_capability_missing' "$RUN_DIR/$name.self.out" "$RUN_DIR/$name.self.err"; then
        echo "[concept-source-admission] FAIL: $name refused for an unrelated reason" >&2
        failures=$((failures + 1))
    elif [[ -z "$owned_code" && "$diagnostic" != PGY_SEM_IMMUTABLE_FIELD_WRITE ]] &&
        ! grep -Fq "$diagnostic" "$RUN_DIR/$name.self.out" "$RUN_DIR/$name.self.err"; then
        echo "[concept-source-admission] FAIL: $name refused without its ownership claim" >&2
        failures=$((failures + 1))
    else
        echo "[concept-source-admission] PASS: $name rejected without MIR"
    fi
done <<'CASES'
tests/concept_semantics/authority_effect/clock_wrong_capability.pgy|missing declared capabilities: clock
tests/concept_semantics/authority_effect/clock_wrong_effect.pgy|missing declared effects: nondeterministic
tests/concept_semantics/authority_effect/authority_wrong_same_type_slot.pgy|participant 'observer' is not declared in zone authority set
tests/concept_semantics/authority_effect/effect_layer_as_class.pgy|references unknown effect type 'Marked'
tests/concept_semantics/nominal/object_write_rejected.pgy|PGY_SEM_IMMUTABLE_FIELD_WRITE
tests/concept_semantics/nominal/struct_write_rejected.pgy|PGY_SEM_IMMUTABLE_FIELD_WRITE
tests/cases/generic_falsification/f_where_ability_bad.pgy|does not satisfy constraint 'Sortable'
tests/cases/axis_composition/comp_world_intent/cross.pgy|cannot escape as a live binding
tests/concept_semantics/nominal/subject_binding_copy_rejected.pgy|Subjects cannot be copied into a new binding|subject_value_copy
tests/concept_semantics/nominal/subject_parameter_copy_rejected.pgy|Subjects cannot be copied into a new binding|subject_value_copy
tests/concept_semantics/nominal/subject_index_copy_rejected.pgy|Subjects cannot be copied into a new binding|subject_value_copy
tests/concept_semantics/nominal/subject_return_rejected.pgy|Returning subjects by value is not supported yet|subject_value_return
tests/concept_semantics/word_deletion/cases/04_match_enum/neg_orig.pgy|PGY_SEM_MATCH_PATTERN_INVALID|match_pattern_invalid
tests/concept_semantics/word_deletion/cases/28_intent_header_policies/retry.pgy|PGY_SEM_INTENT_STEP_INVALID|intent_retry_unavailable
tests/concept_semantics/intent/retry_typed_unused_rejected.pgy|PGY_SEM_INTENT_STEP_INVALID|intent_retry_unavailable
tests/concept_semantics/word_deletion/cases/06_action_func/orig_within.pgy|PGY_SEM_ACTION_CONTRACT_INVALID|action_contract_invalid
tests/concept_semantics/word_deletion/cases/21_ability_role/neg_orig.pgy|PGY_SEM_ABILITY_CONTRACT_INVALID|action_ability_unsatisfied
tests/concept_semantics/word_deletion/cases/13_spawn_await/neg_orig.pgy|PGY_SEM_TASK_LIFECYCLE|task_lifecycle_invalid
CASES

checks=$((checks + 1))
source='tests/concept_semantics/intent/single_step_exact.pgy'
validator="$ROOT_DIR/tests/concept_semantics/verify_source_intent_plan.py"
if ! timeout 60 "$PGY" --native-pipeline --mir-json "$source" \
        >"$RUN_DIR/intent.native.json" 2>"$RUN_DIR/intent.native.err" || \
    ! "${PYTHON_BIN:-python3}" "$validator" "$RUN_DIR/intent.native.json" \
        >"$RUN_DIR/intent.native.check" 2>&1; then
    echo '[concept-source-admission] INVALID CONTROL: native typed Intent plan' >&2
    failures=$((failures + 1))
elif ! timeout 60 "$DRIVER" --emit-mir-json-verified "$source" \
        >"$RUN_DIR/intent.self.json" 2>"$RUN_DIR/intent.self.err" || \
    ! "${PYTHON_BIN:-python3}" "$validator" "$RUN_DIR/intent.self.json" \
        >"$RUN_DIR/intent.self.check" 2>&1; then
    echo '[concept-source-admission] FAIL: self source must publish the typed Intent v3 plan' >&2
    failures=$((failures + 1))
else
    echo '[concept-source-admission] PASS: source typed Intent plan carriage'
fi

while IFS='|' read -r source expected; do
    name="$(basename "$source" .pgy)"
    printf '%s\n' "$expected" | tr ',' '\n' >"$RUN_DIR/$name.expected"
    for leg in native.c native.llvm public.c public.llvm; do
        checks=$((checks + 1))
        origin="${leg%%.*}"
        backend="${leg#*.}"
        stem="$name.$leg"
        command=("$PGY" "tests/concept_semantics/$source"
            "--backend=$backend" --opt=dev -o "$RUN_DIR/$stem.exe")
        [[ "$origin" != native ]] || command+=(--native-pipeline)
        status=0
        env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
            timeout 45 "${command[@]}" >"$RUN_DIR/$stem.compile" 2>&1 || status=$?
        if [[ "$status" != 0 ]] || { [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$RUN_DIR/$stem.compile"; }; then
            echo "[concept-source-admission] FAIL: $stem positive compile ($status)" >&2
            failures=$((failures + 1))
            continue
        fi
        status=0
        timeout 10 "$RUN_DIR/$stem.exe" >"$RUN_DIR/$stem.raw" 2>"$RUN_DIR/$stem.err" || status=$?
        tr -d '\r' <"$RUN_DIR/$stem.raw" >"$RUN_DIR/$stem.actual"
        if [[ "$status" != 0 || -s "$RUN_DIR/$stem.err" ]] ||
            ! cmp -s "$RUN_DIR/$name.expected" "$RUN_DIR/$stem.actual"; then
            echo "[concept-source-admission] FAIL: $stem value/identity execution" >&2
            failures=$((failures + 1))
        else
            echo "[concept-source-admission] PASS: $stem exact $expected execution"
        fi
    done
done <<'EXECUTION_CASES'
nominal/subject_fresh_borrow_valid.pgy|7,19,7
nominal/class_value_copy_valid.pgy|7,19
nominal/named_record_fields_valid.pgy|7,19,31,7,23,31
nominal/named_record_evaluation_order_valid.pgy|3,1,2,1,2,3
nominal/named_record_identifier_hygiene_valid.pgy|3,1,2,1,2,3
nominal/lexical_local_type_identity_valid.pgy|alpha,0,1
authority_effect/action_authority_valid.pgy|0
EXECUTION_CASES

# The public C source projection and shared direct-MIR C projection are
# separate last consumers. Exercise the same valid source at both boundaries.
name=named_record_evaluation_order_valid
source="tests/concept_semantics/nominal/$name.pgy"
for origin in native public; do
    checks=$((checks + 1))
    stem="$name.$origin-mir.c"
    mir="$RUN_DIR/$stem.json"
    command=("$PGY" --native-pipeline --mir-json "$source")
    [[ "$origin" != public ]] || command=("$DRIVER" --emit-mir-json-verified "$source")
    status=0
    timeout 45 "${command[@]}" >"$mir" 2>"$RUN_DIR/$stem.admission" || status=$?
    if [[ "$status" == 0 ]]; then
        timeout 45 "$DRIVER" --mir-json-backend=c "${mir#"$ROOT_DIR/"}" \
            -o "${RUN_DIR#"$ROOT_DIR/"}/$stem" \
            >"$RUN_DIR/$stem.projection" 2>&1 || status=$?
    fi
    if [[ "$status" == 0 ]]; then
        "${CC:-gcc}" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" "$RUN_DIR/$stem" \
            -o "$RUN_DIR/$stem.exe" >"$RUN_DIR/$stem.compile" 2>&1 || status=$?
    fi
    if [[ "$status" != 0 ]]; then
        echo "[concept-source-admission] FAIL: $stem ordered constructor projection ($status)" >&2
        failures=$((failures + 1))
        continue
    fi
    timeout 10 "$RUN_DIR/$stem.exe" >"$RUN_DIR/$stem.raw" 2>"$RUN_DIR/$stem.err" || status=$?
    tr -d '\r' <"$RUN_DIR/$stem.raw" >"$RUN_DIR/$stem.actual"
    if [[ "$status" != 0 || -s "$RUN_DIR/$stem.err" ]] ||
        ! cmp -s "$RUN_DIR/$name.expected" "$RUN_DIR/$stem.actual"; then
        echo "[concept-source-admission] FAIL: $stem ordered constructor execution" >&2
        failures=$((failures + 1))
    else
        echo "[concept-source-admission] PASS: $stem exact 3,1,2,1,2,3 execution"
    fi
done

echo "[concept-source-admission] $checks claims / $failures failures; evidence: $RUN_DIR"
[[ "$failures" == 0 ]]
