#!/usr/bin/env bash
# Canonical effect-family claims, separate from capability authority.
# Keep admission and execution outcomes distinct; no expected-failure allowance.
# Invalid sources are checked for admission only. Only valid local controls run.
#
# SoT semantic.builtin_effect_policy fallback IDs, each kept closed by a check
# here or in the companion tests/builtin_effect_registry_smoke.sh:
# - capability_bits_as_effects: every case below is decided by declared
#   effects alone, and the companion smoke keeps capability out of the
#   generated self projection; capabilities are recorded on their own axis.
# - missing_fixed_effect_as_pure: a fixed builtin without a registry row is a
#   refusal, never a pure call. Native reports "Fixed builtin effect policy is
#   missing", the self projection answers -1, and the self call graph turns -1
#   into unknown effects; the companion smoke asserts all three.
# - renderer_only_effect_admission: the self projection is rendered from the
#   registry with no private rows and checked byte for byte, and every refusal
#   below must happen before publication on both public backends.
# - scalar_callee_mask_as_complete_effects: a fixed mask is a lower bound. The
#   self call graph marks unknown effects instead of reading the mask as the
#   whole set, and the refusals below come from the derived callable body.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here effect-admission "$PGY"
pgy_require_runnable_binary_here effect-admission "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/effect-admission.XXXXXX)"
echo "[effect-admission] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name expected owner; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        command=("$DRIVER" --emit-mir-json-verified "$source")
        diagnostic='Code: declared_effect_missing'
        if [[ "$origin" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
            diagnostic='PGY_SEM_EFFECT_CONFLICT'
        fi
        status=0
        timeout 30 "${command[@]}" >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
        if [[ "$expected" == valid ]]; then
            if [[ "$status" == 0 ]] && grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out"; then
                echo "[effect-admission] PASS $name/$origin admission"
            else
                echo "[effect-admission] FAIL $name/$origin valid source refused (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            ! grep -Fq "Function '$owner' is missing declared effects: $expected" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; then
            echo "[effect-admission] FAIL $name/$origin owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[effect-admission] PASS $name/$origin owned refusal"
        fi
    done
    if [[ "$expected" != valid ]]; then
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.public.$backend"
            status=0
            env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 30 "$PGY" "$source" "--backend=$backend" --opt=dev -o "$WORK/$stem.exe" \
                >"$WORK/$stem.reject" 2>&1 || status=$?
            if [[ "$status" != 1 || -e "$WORK/$stem.exe" ]] ||
                ! grep -Fq 'Code: declared_effect_missing' "$WORK/$stem.reject" ||
                ! grep -Fq "Function '$owner' is missing declared effects: $expected" "$WORK/$stem.reject" ||
                grep -Fq '[pipeline timing]' "$WORK/$stem.reject"; then
                echo "[effect-admission] FAIL $stem: owned refusal before publication absent (status $status)" >&2
                failures=$((failures + 1))
            else
                echo "[effect-admission] PASS $stem refusal before publication"
            fi
        done
    fi
done <<'CASES'
effect_direct_valid|valid|
effect_pure_valid|valid|
effect_callable_isolated_valid|valid|
effect_io_valid|valid|
effect_unused_callable_valid|valid|
effect_unclassified_call|valid|
effect_scalar_composition_valid|valid|
effect_checked_arithmetic_valid|valid|
effect_collection_local_valid|valid|
effect_array_transform_valid|valid|
effect_map_local_valid|valid|
clock_wrong_effect|nondeterministic|Stamp
effect_transitive_rejected|nondeterministic|Main
effect_forward_rejected|nondeterministic|Main
effect_callable_rejected|nondeterministic|Main
effect_io_rejected|io|Exists
effect_declared_rejected|remote|Main
effect_builtin_argument_rejected|nondeterministic|Main
effect_mutator_argument_rejected|nondeterministic|Main
effect_builtin_shadow_rejected|nondeterministic|Main
effect_is_cancelled_rejected|remote|CheckCancellation
CASES
# A fixed row the self checker cannot reach yet: the quantum builtins have no
# self-host signature, and an effect row does not assert one. Native refuses
# with the owned effect conflict. The self driver and both public backends
# must still refuse without publishing, so the gap stays fail-closed rather
# than admitting the call as pure. Move a row into CASES above once its self
# refusal is the owned effect diagnostic, as IsCancelled was.
while IFS='|' read -r name expected owner; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    checks=$((checks + 1))
    status=0
    timeout 30 "$PGY" --native-pipeline --mir-json --error-format=json \
        "$source" >"$WORK/$name.native.out" 2>"$WORK/$name.native.err" || status=$?
    if [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.native.out" ||
        ! grep -Fq 'PGY_SEM_EFFECT_CONFLICT' "$WORK/$name.native.out" "$WORK/$name.native.err" ||
        ! grep -Fq "Function '$owner' is missing declared effects: $expected" \
            "$WORK/$name.native.out" "$WORK/$name.native.err"; then
        echo "[effect-admission] FAIL $name/native fixed-effect refusal absent (status $status)" >&2
        failures=$((failures + 1))
    else
        echo "[effect-admission] PASS $name/native fixed-effect refusal"
    fi
    checks=$((checks + 1))
    status=0
    timeout 30 "$DRIVER" --emit-mir-json-verified "$source" \
        >"$WORK/$name.self.out" 2>"$WORK/$name.self.err" || status=$?
    if [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.self.out"; then
        echo "[effect-admission] FAIL $name/self fixed-effect source admitted (status $status)" >&2
        failures=$((failures + 1))
    else
        echo "[effect-admission] PASS $name/self fail-closed refusal"
    fi
    for backend in c llvm; do
        checks=$((checks + 1))
        stem="$name.public.$backend"
        status=0
        env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            timeout 30 "$PGY" "$source" "--backend=$backend" --opt=dev -o "$WORK/$stem.exe" \
            >"$WORK/$stem.reject" 2>&1 || status=$?
        if [[ "$status" != 1 || -e "$WORK/$stem.exe" ]]; then
            echo "[effect-admission] FAIL $stem: fixed-effect source published (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[effect-admission] PASS $stem fail-closed refusal before publication"
        fi
    done
done <<'NATIVE_FIXED_CASES'
effect_measure_rejected|nondeterministic, collapse|Observe
NATIVE_FIXED_CASES
admission_checks=$checks
admission_failures=$failures
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
            echo "[effect-admission] FAIL $stem: valid source refused" >&2
            failures=$((failures + 1))
            continue
        fi
        if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
            echo "[effect-admission] FAIL $stem: native fallback" >&2
            failures=$((failures + 1))
            continue
        fi
        if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
            echo "[effect-admission] FAIL $stem: valid execution failed" >&2
            failures=$((failures + 1))
            continue
        fi
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.actual"
        if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.actual"; then
            echo "[effect-admission] FAIL $stem: observable result differs" >&2
            failures=$((failures + 1))
        else
            echo "[effect-admission] PASS $stem exact $expected execution"
        fi
    done
done
done <<'EXECUTION_CASES'
effect_pure_valid|5
effect_unclassified_call|2
effect_scalar_composition_valid|4,7
effect_checked_arithmetic_valid|2147483647,2147395600,-40
effect_collection_local_valid|2,3
effect_array_transform_valid|1,3,3,1,1,0,9
effect_map_local_valid|1,4
EXECUTION_CASES
echo "[effect-admission] admission: $admission_checks checks / $admission_failures failures"
echo "[effect-admission] execution: $((checks - admission_checks)) checks / $((failures - admission_failures)) failures"
echo "[effect-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
