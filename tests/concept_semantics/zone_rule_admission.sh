#!/usr/bin/env bash
# Source admission only: invalid programs must never be executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here zone-rule-admission "$PGY"
pgy_require_runnable_binary_here zone-rule-admission "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/zone-rule-admission.XXXXXX)"
echo "[zone-rule-admission] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name expected diagnostic; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    for pipeline in native public; do
        command=("$PGY" --native-pipeline --mir-json)
        [[ "$pipeline" != public ]] || command=("$DRIVER" --emit-mir-json-verified)
        status=0
        timeout 60 "${command[@]}" "$source" >"$WORK/$name.$pipeline.out" 2>"$WORK/$name.$pipeline.err" || status=$?
        checks=$((checks + 1))
        if [[ "$expected" == accept ]]; then
            if [[ "$status" != 0 ]] || ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out"; then
                echo "[zone-rule-admission] FAIL: $name $pipeline lost a valid program (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[zone-rule-admission] FAIL: $name $pipeline must refuse its exact claim (status $status)" >&2
            failures=$((failures + 1))
        elif [[ "$pipeline" == public ]] &&
            ! grep -Fq 'Code: zone_contract_invalid' "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[zone-rule-admission] FAIL: $name public diagnostic identity drifted" >&2
            failures=$((failures + 1))
        fi
    done
done <<'CASES'
zone_authority_owner_valid|accept|
zone_authority_free_valid|accept|
authority_wrong_same_type_slot|reject|participant 'observer' is not declared in zone authority set
effect_layer_as_class|reject|references unknown effect type 'Marked'
zone_authority_missing_by|reject|must specify 'by <subjectSlot>' when authority is declared
zone_authority_other_zone|reject|participant 'observer' is not declared in zone authority set
zone_link_wrong_authority|reject|participant 'observer' is not declared in zone authority set
zone_unused_effect_class|reject|references unknown effect type 'Marked'
zone_relation_class|reject|references unknown relation type 'Bond'
CASES
echo "[zone-rule-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
