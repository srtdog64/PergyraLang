#!/usr/bin/env bash
# Invalid programs are admitted/refused only, never executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here world-zone-admission "$PGY"
pgy_require_runnable_binary_here world-zone-admission "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/world-zone-admission.XXXXXX)"
echo "[world-zone-admission] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r source expected; do
    name="$(basename "$source" .pgy)"
    for pipeline in native public; do
        command=("$PGY" --native-pipeline --mir-json)
        [[ "$pipeline" != public ]] || command=("$DRIVER" --emit-mir-json-verified)
        status=0
        timeout 60 "${command[@]}" "$source" >"$WORK/$name.$pipeline.out" 2>"$WORK/$name.$pipeline.err" || status=$?
        checks=$((checks + 1))
        if [[ "$expected" == accept ]]; then
            if [[ "$status" != 0 ]] || ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out"; then
                echo "[world-zone-admission] FAIL: $name $pipeline lost a valid program (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out"; then
            echo "[world-zone-admission] FAIL: $name $pipeline must refuse without MIR (status $status)" >&2
            failures=$((failures + 1))
        elif [[ "$pipeline" == public && "$name" == world_zone_local_bad ]]; then
            if ! grep -Fq 'Code: zone_value_copy_requires_transfer' "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
                echo "[world-zone-admission] FAIL: $name public lost the existing local-carriage diagnostic" >&2
                failures=$((failures + 1))
            fi
        elif ! grep -Fq 'cannot escape as a live binding' "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[world-zone-admission] FAIL: $name $pipeline refused an unrelated claim" >&2
            failures=$((failures + 1))
        elif [[ "$pipeline" == public ]] && ! grep -Fq 'Code: world_zone_escape' "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[world-zone-admission] FAIL: $name public lost the owned boundary diagnostic" >&2
            failures=$((failures + 1))
        fi
    done
done <<'CASES'
tests/concept_semantics/authority_effect/world_zone_clone_valid.pgy|accept
tests/concept_semantics/authority_effect/world_zone_read_valid.pgy|accept
tests/cases/axis_composition/comp_world_intent/cross.pgy|reject
tests/concept_semantics/authority_effect/world_zone_argument_bad.pgy|reject
tests/concept_semantics/authority_effect/world_zone_local_bad.pgy|reject
tests/concept_semantics/authority_effect/world_zone_return_bad.pgy|reject
tests/concept_semantics/authority_effect/world_zone_shadow_clone_bad.pgy|reject
tests/concept_semantics/authority_effect/world_zone_formal_clone_bad.pgy|reject
CASES
echo "[world-zone-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
