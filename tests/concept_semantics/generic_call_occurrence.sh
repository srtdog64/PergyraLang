#!/usr/bin/env bash
# Public MIR call-site identity only; mutations are admitted/refused, never run.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here generic-call-occurrence "$PGY"
pgy_require_runnable_binary_here generic-call-occurrence "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
RUN_DIR="$(mktemp -d .tmp/self_hosted/generic-call-occurrence.XXXXXX)"
echo "[generic-call-occurrence] evidence: $RUN_DIR"
sha256sum "$PGY" "$DRIVER" >"$RUN_DIR/inputs.sha256"
PROBE="$RUN_DIR/occurrence-admission.exe"
if ! timeout 120 "$PGY" --native-pipeline --backend=c \
    tests/self_hosted/parity/fixture/generic_call_occurrence_admission_probe.pgy \
    -o "$PROBE" >"$RUN_DIR/probe.compile" 2>&1; then
    tail -n 25 "$RUN_DIR/probe.compile" >&2
    exit 1
fi
sha256sum "$PROBE" >>"$RUN_DIR/inputs.sha256"
checks=0
failures=0
while IFS='|' read -r name instance_count; do
    file="$RUN_DIR/$name.json"
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$RUN_DIR/inputs.sha256"
    checks=$((checks + 1))
    if ! timeout 60 "$DRIVER" --emit-mir-json-verified "$source" >"$file" 2>"$file.err" ||
        ! timeout 30 "$PROBE" "$file" >"$file.admission" 2>"$file.admission.err" ||
        ! grep -Fq 'generic call occurrences verified:' "$file.admission" ||
        ! grep -Fxq "generic instance closure verified: $instance_count" "$file.admission"; then
        echo "[generic-call-occurrence] FAIL $name source/identity publication" >&2
        failures=$((failures + 1))
        continue
    fi
    echo "[generic-call-occurrence] PASS $name source/identity publication"
    "${PYTHON_BIN:-python3}" tests/concept_semantics/generic_call_occurrence_mutations.py "$file"
    for mutation in missing-table missing-row missing-anchors empty-anchors duplicate-anchor \
        crossed-routine_syntax_id crossed-block_id crossed-instruction_id crossed-call_node \
        wrong-lane wrong-target wrong-formal row-order provenance-epoch; do
        checks=$((checks + 1))
        status=0
        timeout 30 "$PROBE" "$file.$mutation.json" \
            >"$file.$mutation.out" 2>"$file.$mutation.err" || status=$?
        if [[ "$mutation" == row-order || "$mutation" == provenance-epoch ]]; then
            if [[ "$status" == 0 ]] && grep -Fq 'generic call occurrences verified:' "$file.$mutation.out"; then
                continue
            fi
        elif [[ "$status" == 1 ]] &&
            grep -Fq 'MIR generic call occurrences are missing or invalid' "$file.$mutation.err" "$file.$mutation.out" &&
            ! grep -Fq 'generic call occurrences verified:' "$file.$mutation.out"; then
            continue
        fi
        echo "[generic-call-occurrence] FAIL $name $mutation (status $status)" >&2
        failures=$((failures + 1))
    done
    # The production backend entry must consume the same validator, not leave
    # it reachable only through the probe. Refused MIR is never executed.
    for backend in c llvm; do
        checks=$((checks + 1))
        status=0
        timeout 30 "$DRIVER" "--mir-json-backend=$backend" "$file.empty-anchors.json" \
            -o "$file.$backend.invalid-output" \
            >"$file.$backend.refusal.out" 2>"$file.$backend.refusal.err" || status=$?
        if [[ "$status" != 1 ]] || [[ -s "$file.$backend.invalid-output" ]] ||
            ! grep -Fq 'MIR generic call occurrences are missing or invalid' \
                "$file.$backend.refusal.out" "$file.$backend.refusal.err"; then
            echo "[generic-call-occurrence] FAIL $name $backend production refusal ($status)" >&2
            failures=$((failures + 1))
        fi
    done
done <<'CASES'
generic_bounds_type_valid|1
generic_bounds_valid|2
generic_bounds_forward_valid|2
generic_forwarding_value_valid|6
generic_forwarding_recursion_valid|2
generic_forwarding_constant_valid|2
CASES
echo "[generic-call-occurrence] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
