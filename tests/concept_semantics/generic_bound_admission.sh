#!/usr/bin/env bash
# Source admission only: invalid programs must never be executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here generic-bound-admission "$PGY"
pgy_require_runnable_binary_here generic-bound-admission "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/generic-bound-admission.XXXXXX)"
echo "[generic-bound-admission] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name expected diagnostic; do
    source="$name"
    name="$(basename "$source" .pgy)"
    for pipeline in native public; do
        command=("$PGY" --native-pipeline --mir-json)
        [[ "$pipeline" != public ]] || command=("$DRIVER" --emit-mir-json-verified)
        status=0
        timeout 60 "${command[@]}" "$source" >"$WORK/$name.$pipeline.out" 2>"$WORK/$name.$pipeline.err" || status=$?
        checks=$((checks + 1))
        if [[ "$expected" == accept ]]; then
            if [[ "$status" != 0 ]] || ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out"; then
                echo "[generic-bound-admission] FAIL: $name $pipeline lost a valid program (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$pipeline.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[generic-bound-admission] FAIL: $name $pipeline must refuse its exact claim (status $status)" >&2
            failures=$((failures + 1))
        elif [[ "$pipeline" == public ]] &&
            ! grep -Fq 'Code: generic_constraint_unsatisfied' "$WORK/$name.$pipeline.out" "$WORK/$name.$pipeline.err"; then
            echo "[generic-bound-admission] FAIL: $name public diagnostic identity drifted" >&2
            failures=$((failures + 1))
        fi
    done
done <<'CASES'
tests/cases/generic_falsification/f_where_ability_ok.pgy|accept|
tests/concept_semantics/authority_effect/generic_bounds_valid.pgy|accept|
tests/concept_semantics/authority_effect/generic_bounds_forward_valid.pgy|accept|
tests/concept_semantics/authority_effect/generic_bounds_type_valid.pgy|accept|
tests/cases/generic_falsification/f_where_ability_bad.pgy|reject|does not satisfy constraint 'Sortable'
tests/concept_semantics/authority_effect/generic_bounds_explicit_bad.pgy|reject|does not satisfy constraint 'Sortable'
tests/concept_semantics/authority_effect/generic_bounds_second_bad.pgy|reject|does not satisfy constraint 'Cloneable'
tests/concept_semantics/authority_effect/generic_bounds_forward_bad.pgy|reject|does not satisfy constraint 'Cloneable'
tests/concept_semantics/authority_effect/generic_bounds_type_bad.pgy|reject|does not satisfy constraint 'Int'
CASES
while IFS='|' read -r name expected; do
printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
for origin in native public; do
    for backend in c llvm; do
        checks=$((checks + 1))
        stem="$name.$origin.$backend"
        command=("$PGY" "tests/concept_semantics/authority_effect/$name.pgy"
            "--backend=$backend" --opt=dev -o "$WORK/$stem.exe")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        if ! env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
            timeout 45 "${command[@]}" >"$WORK/$stem.compile" 2>&1; then
            echo "[generic-bound-admission] FAIL $stem: valid source refused" >&2
            failures=$((failures + 1))
            continue
        fi
        if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
            echo "[generic-bound-admission] FAIL $stem: native fallback" >&2
            failures=$((failures + 1))
            continue
        fi
        if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
            echo "[generic-bound-admission] FAIL $stem: valid execution failed" >&2
            failures=$((failures + 1))
            continue
        fi
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.actual"
        if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.actual"; then
            echo "[generic-bound-admission] FAIL $stem: observable result differs" >&2
            failures=$((failures + 1))
        else
            echo "[generic-bound-admission] PASS $stem exact $expected execution"
        fi
    done
done
done <<'EXECUTION_CASES'
generic_bounds_valid|1,1,2
generic_bounds_forward_valid|1
generic_bounds_type_valid|3
EXECUTION_CASES
echo "[generic-bound-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
