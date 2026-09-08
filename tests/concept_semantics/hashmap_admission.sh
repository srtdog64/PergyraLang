#!/usr/bin/env bash
# HashMap source equivalence: typed refusal, valid admission, then execution.
# Invalid programs are never executed. No expected-failure allowance.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here hashmap-admission "$PGY"
pgy_require_runnable_binary_here hashmap-admission "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-admission.XXXXXX)"
echo "[hashmap-admission] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name native_code self_code anchor; do
    source="tests/concept_semantics/hashmap/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        command=("$DRIVER" --emit-mir-json-verified "$source")
        diagnostic="Code: $self_code"
        if [[ "$origin" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
            diagnostic="$native_code"
        fi
        status=0
        timeout 30 "${command[@]}" >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
        if [[ "$native_code" == valid ]]; then
            if [[ "$status" == 0 ]] && grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out"; then
                echo "[hashmap-admission] PASS $name/$origin admission"
            else
                echo "[hashmap-admission] FAIL $name/$origin valid source refused (status $status)" >&2
                failures=$((failures + 1))
            fi
        elif [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" ||
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" ||
            { [[ "$origin" == self ]] && ! grep -Fq "$anchor" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; }; then
            echo "[hashmap-admission] FAIL $name/$origin owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[hashmap-admission] PASS $name/$origin owned refusal"
        fi
    done
    if [[ "$native_code" != valid ]]; then
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.public.$backend"
            status=0
            env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 30 "$PGY" "$source" "--backend=$backend" --opt=dev -o "$WORK/$stem.exe" \
                >"$WORK/$stem.reject" 2>&1 || status=$?
            if [[ "$status" != 1 || -e "$WORK/$stem.exe" ]] ||
                ! grep -Fq "Code: $self_code" "$WORK/$stem.reject" ||
                ! grep -Fq "$anchor" "$WORK/$stem.reject" ||
                grep -Fq '[pipeline timing]' "$WORK/$stem.reject"; then
                echo "[hashmap-admission] FAIL $stem refusal before publication absent (status $status)" >&2
                failures=$((failures + 1))
            else
                echo "[hashmap-admission] PASS $stem refusal before publication"
            fi
        done
    fi
done <<'CASES'
operations_valid|valid||
key_types_valid|valid||
callable_shadow_valid|valid||
evaluation_order_valid|valid||
wrong_key|PGY_SEM_TYPE_MISMATCH|call_arg_type_mismatch|expected: String
wrong_value|PGY_SEM_TYPE_MISMATCH|call_arg_type_mismatch|expected: Int
wrong_receiver|PGY_SEM_BUILTIN_ARGS_INVALID|builtin_arg_type_mismatch|expected: HashMap<K, V>
wrong_nested_key|PGY_SEM_TYPE_MISMATCH|call_arg_type_mismatch|expected: String
wrong_arity|PGY_SEM_BUILTIN_ARGS_INVALID|call_arity_mismatch|expected: 2
unsupported_key|PGY_SEM_BUILTIN_ARGS_INVALID|builtin_arg_type_mismatch|supported stable scalar key
parameter_mutation|PGY_SEM_BUILTIN_ARGS_INVALID|value_param_collection_mutation|param: values
unresolved_constructor|PGY_SEM_INFER_COLLECTION|initializer_type_unresolved|binding: values
CASES
admission_checks=$checks
admission_failures=$failures
while IFS='|' read -r name expected; do
    printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
    for origin in native public; do
        for backend in c llvm; do
            checks=$((checks + 1))
            stem="$name.$origin.$backend"
            command=("$PGY" "tests/concept_semantics/hashmap/$name.pgy"
                "--backend=$backend" --opt=dev -o "$WORK/$stem.exe")
            [[ "$origin" == native ]] && command+=(--native-pipeline)
            if ! env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
                timeout 45 "${command[@]}" >"$WORK/$stem.compile" 2>&1; then
                echo "[hashmap-admission] FAIL $stem valid source refused" >&2
                failures=$((failures + 1)); continue
            fi
            if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
                echo "[hashmap-admission] FAIL $stem native fallback" >&2
                failures=$((failures + 1)); continue
            fi
            if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
                echo "[hashmap-admission] FAIL $stem valid execution failed" >&2
                failures=$((failures + 1)); continue
            fi
            tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.actual"
            if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.actual"; then
                echo "[hashmap-admission] FAIL $stem observable result differs" >&2
                failures=$((failures + 1))
            else
                echo "[hashmap-admission] PASS $stem exact $expected execution"
            fi
        done
    done
done <<'EXECUTION_CASES'
operations_valid|5,true,2,first,second,1,false,second
key_types_valid|three,-2,3,40,-9,4,1,false,true
callable_shadow_valid|6
evaluation_order_valid|key,value,7
EXECUTION_CASES
echo "[hashmap-admission] admission: $admission_checks checks / $admission_failures failures"
echo "[hashmap-admission] execution: $((checks - admission_checks)) checks / $((failures - admission_failures)) failures"
echo "[hashmap-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
