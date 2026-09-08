#!/usr/bin/env bash
# Source-produced v3 plans must cross the existing machine admission boundary.
# Invalid controls are mutated MIR data consumed only by that validator.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here typed-intent-publication "$PGY"
pgy_require_runnable_binary_here typed-intent-publication "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
RUN_DIR="$(mktemp -d .tmp/self_hosted/typed-intent-publication.XXXXXX)"
echo "[typed-intent-publication] evidence: $RUN_DIR"
sha256sum "$PGY" "$DRIVER" >"$RUN_DIR/inputs.sha256"
PROBE="$RUN_DIR/plan-admission.exe"
if ! timeout 120 "$PGY" --native-pipeline --backend=c \
    tests/self_hosted/parity/fixture/intent_execution_plan_json_admission_probe.pgy \
    -o "$PROBE" >"$RUN_DIR/probe.compile.log" 2>&1; then
    tail -n 30 "$RUN_DIR/probe.compile.log" >&2
    exit 1
fi
sha256sum "$PROBE" >>"$RUN_DIR/inputs.sha256"
failures=0
checks=0
while IFS='|' read -r name source; do
    sha256sum "$source" >>"$RUN_DIR/inputs.sha256"
    for pipeline in native public; do
        checks=$((checks + 1))
        file="$RUN_DIR/$name.$pipeline.json"
        if [[ "$pipeline" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json "$source")
        else
            command=("$DRIVER" --emit-mir-json-verified "$source")
        fi
        if ! timeout 60 "${command[@]}" >"$file" 2>"$file.err" ||
            ! timeout 30 "$PROBE" --verify-input "$file" \
                >"$file.admission" 2>"$file.admission.err" ||
            ! grep -Fq 'intent execution admission: present' "$file.admission"; then
            echo "[typed-intent-publication] FAIL: $name $pipeline cross-seal" >&2
            failures=$((failures + 1))
            continue
        fi
        echo "[typed-intent-publication] PASS: $name $pipeline cross-seal"
        if [[ "$pipeline" != public ]]; then continue; fi
        "${PYTHON_BIN:-python3}" tests/concept_semantics/typed_intent_publication_mutations.py "$file"
        for mutation in missing-plan crossed-payload missing-completion; do
            checks=$((checks + 1))
            status=0
            timeout 30 "$PROBE" --verify-input "$file.$mutation.json" \
                >"$file.$mutation.out" 2>"$file.$mutation.err" || status=$?
            claim='MIR intent execution facts are missing or invalid'
            if [[ "$mutation" == missing-plan ]]; then
                claim='MIR machine-layer facts are missing or invalid'
            fi
            if [[ "$status" != 1 ]] ||
                grep -Fq 'pgy.mir.v1 input verified' "$file.$mutation.out" ||
                ! grep -Fq "$claim" "$file.$mutation.out" "$file.$mutation.err"; then
                echo "[typed-intent-publication] FAIL: $name $mutation refusal ($status)" >&2
                failures=$((failures + 1))
            fi
        done
    done
done <<'CASES'
single|tests/concept_semantics/intent/single_step_exact.pgy
compensation|tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy
CASES
echo "[typed-intent-publication] $checks checks / $failures failures"
[[ "$failures" == 0 ]]
