#!/usr/bin/env bash
# Canonical callable-capability outcomes, not agreement with a native oracle.
# Invalid sources are never executed, even if a comparator incorrectly admits them.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here callable-capability "$PGY"
pgy_require_runnable_binary_here callable-capability "$DRIVER"
case "$DRIVER" in
    *.exe) COMPANION="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) COMPANION="$DRIVER.machine-layer-manifest.json" ;;
esac
[[ -s "$COMPANION" ]] || { echo '[callable-capability] missing driver machine-manifest companion (test setup)' >&2; exit 2; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/callable-capability.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
echo "[callable-capability] evidence: $REL"
sha256sum "$PGY" "$DRIVER" "$COMPANION" \
    tests/concept_semantics/authority_effect/callable_capability_fixture.pgy >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r source expectation owner; do
    name="$(basename "${source%.pgy}")"
    [[ "$name" != main ]] || name=compose_two_functions
    sha256sum "$source" >>"$WORK/inputs.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        status=0
        command=("$DRIVER" --emit-mir-json-verified "$source")
        diagnostic='Code: declared_capability_missing'
        if [[ "$origin" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
            diagnostic='PGY_SEM_EFFECT_CONFLICT'
        fi
        timeout 30 "${command[@]}" >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
        if [[ "$expectation" == valid ]]; then
            if [[ "$status" == 0 ]] && grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out"; then
                echo "[callable-capability] PASS $name/$origin admission"
            else
                echo "[callable-capability] FAIL $name/$origin: valid source refused (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            ! grep -Fq 'missing declared capabilities: clock' "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            ! grep -Fq "Function '$owner'" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; then
            echo "[callable-capability] FAIL $name/$origin: owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[callable-capability] PASS $name/$origin owned refusal"
        fi
    done
    if [[ "$expectation" != valid ]]; then
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.public.$backend"
            status=0
            env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 30 "$PGY" "$source" "--backend=$backend" --opt=dev -o "$REL/$stem.exe" \
                >"$WORK/$stem.reject" 2>&1 || status=$?
            if [[ "$status" != 1 || -e "$WORK/$stem.exe" ]] ||
                ! grep -Fq 'Code: declared_capability_missing' "$WORK/$stem.reject" ||
                ! grep -Fq 'missing declared capabilities: clock' "$WORK/$stem.reject" ||
                ! grep -Fq "Function '$owner'" "$WORK/$stem.reject" ||
                grep -Fq '[pipeline timing]' "$WORK/$stem.reject"; then
                echo "[callable-capability] FAIL $stem: public owned refusal absent (status $status)" >&2
                failures=$((failures + 1))
            else
                echo "[callable-capability] PASS $stem rejection before artifact publication"
            fi
        done
    fi
done <<'CASES'
tests/concept_semantics/authority_effect/callable_capability_valid.pgy|valid|
tests/concept_semantics/authority_effect/callable_capability_isolated_valid.pgy|valid|
tests/concept_semantics/authority_effect/callable_capability_unused_valid.pgy|valid|
tests/concept_semantics/authority_effect/callable_capability_recursive_valid.pgy|valid|
tests/concept_semantics/authority_effect/callable_capability_higher_valid.pgy|valid|
tests/concept_semantics/authority_effect/callable_capability_rejected.pgy|clock|Main
tests/concept_semantics/authority_effect/callable_capability_forward_rejected.pgy|clock|Main
tests/concept_semantics/authority_effect/callable_capability_recursive_rejected.pgy|clock|Main
tests/concept_semantics/authority_effect/callable_capability_higher_rejected.pgy|clock|Main
tests/concept_semantics/authority_effect/callable_capability_bound_rejected.pgy|clock|Restricted
tests/cases/backend_compare/compose_two_functions/main.pgy|valid|
tests/self_hosted/fixtures/callable_parameter_builtin_shadow.pgy|valid|
CASES
echo "[callable-capability] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
