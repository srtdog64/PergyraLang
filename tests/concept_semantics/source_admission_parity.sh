#!/usr/bin/env bash
# Open semantic-admission claims. This gate must stay RED while any claim is
# false; it has no expected-failure allowance and is not in the green runner.
# Invalid inputs are checked only through source-to-MIR, never executed.
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

while IFS='|' read -r source diagnostic; do
    name="$(basename "$source" .pgy)"
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

echo "[concept-source-admission] $checks claims / $failures failures; evidence: $RUN_DIR"
[[ "$failures" == 0 ]]
