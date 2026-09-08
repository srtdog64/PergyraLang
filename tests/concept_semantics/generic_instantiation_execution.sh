#!/usr/bin/env bash
# Both native/public C/LLVM must preserve call-site instance identity.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here generic-instantiation "$PGY"
pgy_require_runnable_binary_here generic-instantiation "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/generic-instantiation.XXXXXX)"
echo "[generic-instantiation] evidence: $WORK"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
failures=0
for probe in closure signature graph; do
    source="tests/self_hosted/parity/fixture/generic_instance_${probe}_probe.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    if ! timeout 120 "$PGY" --native-pipeline --backend=c --opt=dev "$source" \
        -o "$WORK/$probe-probe.exe" >"$WORK/$probe-probe.compile" 2>&1; then
        tail -n 20 "$WORK/$probe-probe.compile" >&2
        exit 1
    fi
    sha256sum "$WORK/$probe-probe.exe" >>"$WORK/inputs.sha256"
done
checks=$((checks + 1))
if ! timeout 30 "$WORK/closure-probe.exe" >"$WORK/closure-probe.out" 2>"$WORK/closure-probe.err"; then
    echo '[generic-instantiation] FAIL shared closure controls' >&2
    failures=$((failures + 1))
fi
while IFS='|' read -r name expected; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
    for leg in native.c native.llvm public.c public.llvm; do
        checks=$((checks + 1))
        origin="${leg%%.*}"
        backend="${leg#*.}"
        stem="$name.$leg"
        command=("$PGY" "$source" "--backend=$backend" --opt=dev -o "$WORK/$stem.exe")
        [[ "$origin" != native ]] || command+=(--native-pipeline)
        status=0
        env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$DRIVER" \
            timeout 45 "${command[@]}" >"$WORK/$stem.compile" 2>&1 || status=$?
        if [[ "$status" != 0 ]] || { [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; }; then
            echo "[generic-instantiation] FAIL $stem compile ($status)" >&2
            failures=$((failures + 1))
            continue
        fi
        status=0
        timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err" || status=$?
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.actual"
        if [[ "$status" != 0 || -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.actual"; then
            echo "[generic-instantiation] FAIL $stem execution ($status)" >&2
            failures=$((failures + 1))
        else
            echo "[generic-instantiation] PASS $stem exact $expected"
        fi
    done
    # Admission-only controls bind the execution inventory to unchanged source
    # IDs, exact caller instances, and producer-owned physical receipts.
    checks=$((checks + 1))
    if ! timeout 45 "$DRIVER" --emit-mir-json-verified "$source" \
        >"$WORK/$name.json" 2>"$WORK/$name.mir.err" ||
        ! timeout 30 "$WORK/signature-probe.exe" "$WORK/$name.json" \
        >"$WORK/$name.signature.out" 2>"$WORK/$name.signature.err" ||
        ! timeout 30 "$WORK/graph-probe.exe" "$WORK/$name.json" \
        >"$WORK/$name.graph.out" 2>"$WORK/$name.graph.err"; then
        echo "[generic-instantiation] FAIL $name instance input admission" >&2
        failures=$((failures + 1))
        continue
    fi
    "${PYTHON_BIN:-python3}" tests/concept_semantics/generic_instance_plan_mutations.py "$WORK/$name.json"
    mutations=(routine-order missing-return return-layout return-required
        missing-constraints empty-constraints crossed-constraint missing-bound
        null-bound duplicate-constraint scalar-constraint)
    if [[ "$name" == generic_identity_read_valid ]]; then
        mutations+=(cell-missing-id cell-field-missing-id cell-field-crossed-id
            cell-physical-layout cell-unknown-field-type cell-return-escape)
    fi
    for mutation in "${mutations[@]}"; do
        checks=$((checks + 1))
        status=0
        timeout 30 "$WORK/graph-probe.exe" "$WORK/$name.json.$mutation.json" \
            >"$WORK/$name.$mutation.out" 2>"$WORK/$name.$mutation.err" || status=$?
        if [[ "$mutation" == routine-order ]]; then wanted=0; else wanted=1; fi
        if [[ "$status" != "$wanted" ]]; then
            echo "[generic-instantiation] FAIL $name $mutation admission ($status)" >&2
            failures=$((failures + 1))
        fi
    done
done <<'CASES'
generic_forwarding_value_valid|13,ok,true
generic_forwarding_recursion_valid|3,2
generic_forwarding_constant_valid|2
generic_forwarding_local_valid|7,local,true
generic_forwarding_phi_valid|9,right,false
generic_explicit_zero_arg_valid|9,9
generic_identity_read_valid|7,19,31,43,7
CASES
# No execution of the constructor-expanding control, even if compilation
# accidentally succeeds. Its backend must explain the unsupported expansion.
checks=$((checks + 1))
source=tests/concept_semantics/authority_effect/generic_forwarding_expanding.pgy
sha256sum "$source" >>"$WORK/inputs.sha256"
status=0
env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" timeout 45 \
    "$PGY" "$source" --backend=c --opt=dev -o "$WORK/expanding.exe" \
    >"$WORK/expanding.compile" 2>&1 || status=$?
if [[ "$status" != 1 ]] || ! grep -Fq 'constructor-expanding recursive specialization' "$WORK/expanding.compile"; then
    echo "[generic-instantiation] FAIL expanding-type refusal ($status)" >&2
    failures=$((failures + 1))
fi
echo "[generic-instantiation] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
