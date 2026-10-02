#!/usr/bin/env bash
# Pending physical field/current-definition carriage, never an ownership grant.
set -Eeuo pipefail
trap 'status=$?; echo "[constructor-field-input] failed at line $LINENO (status $status); evidence: ${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here constructor-field-input "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/constructor-field-input.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/collection_constructor_field_input_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUTS=(callable_table_owned_release_positive callable_table_empty_release_positive
    callable_table_borrowed_negative constructor_field_current_definition_input
    constructor_field_candidate_input constructor_field_absent_input constructor_field_intent_input)
sha256sum "$PGY" "$PROBE" tests/self_hosted/parity/collection_constructor_field_input_owner.sh Makefile >"$WORK/native-probe.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
for input in "${INPUTS[@]}"; do sha256sum "$FIXTURES/$input.pgy"; done >"$WORK/inputs.sha256"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-probe.exe" >"$WORK/$backend.build" 2>&1
    sha256sum "$WORK/$backend-probe.exe" >>"$WORK/binaries.sha256"
    for input in "${INPUTS[@]}"; do
        timeout 30 "$WORK/$backend-probe.exe" "$FIXTURES/$input.pgy" -1 \
            >"$WORK/$backend-$input.raw" 2>"$WORK/$backend-$input.err"
        tr -d '\r' <"$WORK/$backend-$input.raw" >"$WORK/$backend-$input.run"
        [[ ! -s "$WORK/$backend-$input.err" ]]
        case "$input" in
            callable_table_owned_release_positive) printf 'inputs=3\nnamed=3\nempty=0\nunproved=0\nreplaced=0\n' ;;
            callable_table_empty_release_positive) printf 'inputs=3\nnamed=0\nempty=3\nunproved=0\nreplaced=0\n' ;;
            callable_table_borrowed_negative) printf 'inputs=3\nnamed=0\nempty=0\nunproved=3\nreplaced=0\n' ;;
            constructor_field_current_definition_input) printf 'inputs=4\nnamed=2\nempty=1\nunproved=1\nreplaced=1\n' ;;
            constructor_field_candidate_input) printf 'inputs=5\nnamed=0\nempty=0\nunproved=5\nreplaced=0\n' ;;
            constructor_field_absent_input) printf 'inputs=0\nnamed=0\nempty=0\nunproved=0\nreplaced=0\n' ;;
            constructor_field_intent_input) printf 'inputs=1\nnamed=0\nempty=1\nunproved=0\nreplaced=0\n' ;;
        esac >"$WORK/expected"
        if [[ "$input" == constructor_field_intent_input ]]; then printf 'intents=1\n'; else printf 'intents=0\n'; fi >>"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-$input.run"
    done
    for mutation in {0..31}; do
        input=callable_table_owned_release_positive
        if [[ "$mutation" == 22 || "$mutation" == 23 || "$mutation" == 25 ]]; then input=constructor_field_current_definition_input; fi
        if [[ "$mutation" -ge 28 ]]; then input=constructor_field_intent_input; fi
        timeout 30 "$WORK/$backend-probe.exe" "$FIXTURES/$input.pgy" "$mutation" \
            >"$WORK/$backend-unit-$mutation.raw" 2>"$WORK/$backend-unit-$mutation.err"
        tr -d '\r' <"$WORK/$backend-unit-$mutation.raw" >"$WORK/$backend-unit-$mutation.run"
        [[ ! -s "$WORK/$backend-unit-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-unit-$mutation.run"
    done
    timeout 30 "$WORK/$backend-probe.exe" "$FIXTURES/constructor_field_intent_input.pgy" -2 \
        >"$WORK/$backend-intent-admission.raw" 2>"$WORK/$backend-intent-admission.err"
    tr -d '\r' <"$WORK/$backend-intent-admission.raw" >"$WORK/$backend-intent-admission.run"
    [[ ! -s "$WORK/$backend-intent-admission.err" ]]
    printf 'body_ok=true\n' >"$WORK/expected"
    cmp "$WORK/expected" "$WORK/$backend-intent-admission.run"
    echo "[constructor-field-input] $backend: seven production collectors, thirty-two carrier checks and intent admission PASS (no field grant)"
done
for input in "${INPUTS[@]}"; do cmp "$WORK/c-$input.run" "$WORK/llvm-$input.run"; done
cmp "$WORK/c-intent-admission.run" "$WORK/llvm-intent-admission.run"
sha256sum --quiet -c "$WORK/native-probe.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/binaries.sha256"
echo "[constructor-field-input] evidence: $REL; actual Release/MIR discharge still unproved"
