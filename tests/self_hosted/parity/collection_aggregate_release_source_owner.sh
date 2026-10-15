#!/usr/bin/env bash
# Exact aggregate-field release proof at the source semantic boundary.
# Supplied fixtures are parsed and analyzed only; this gate makes no MIR or
# target-runtime claim.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=collection-aggregate-release-source
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/aggregate-release-source.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
QUERY=tests/self_hosted/fixtures/collection_constructor_field_input_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
CASE_DATA=tests/self_hosted/parity/fixture/collection_field_lifetime/aggregate_release_source_cases.tsv
mapfile -t CASES <"$CASE_DATA"
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" "$QUERY" "$CASE_DATA" "${BASH_SOURCE[0]}" >"$WORK/inputs.sha256"
sha256sum tests/self_hosted/fixtures/collection_execution_context_generation_probe.pgy >>"$WORK/inputs.sha256"
for case_row in "${CASES[@]}"; do
    IFS='|' read -r fixture _ _ _ <<<"$case_row"
    sha256sum "$FIXTURES/$fixture" >>"$WORK/inputs.sha256"
done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | \
    xargs -0 sha256sum >"$WORK/imports.sha256"
: >"$WORK/binaries.sha256"
guard_exit() {
    local status=$?
    trap - EXIT
    for manifest in native inputs imports binaries; do
        sha256sum --quiet -c "$WORK/$manifest.sha256" || status=1
    done
    find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | \
        xargs -0 sha256sum >"$WORK/imports.after.sha256" || status=1
    cmp "$WORK/imports.sha256" "$WORK/imports.after.sha256" || status=1
    exit "$status"
}
trap guard_exit EXIT
echo "[$LABEL] evidence=$REL; source fixtures are analyzed, never emitted/run"
for backend in c llvm; do
    timeout 240 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" \
        --opt=dev -o "$REL/$backend-source.exe" \
        >"$WORK/$backend-source.compile" 2>&1 || \
        fail "$backend source analyzer did not build"
    [[ -s "$WORK/$backend-source.exe" ]] && sha256sum "$WORK/$backend-source.exe" >>"$WORK/binaries.sha256" || \
        fail "$backend source analyzer binary guard could not be recorded"
    timeout 120 "$PGY" --native-pipeline "$QUERY" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-query.exe" >"$WORK/$backend-query.compile" 2>&1 || \
        fail "$backend constructor query did not build"
    [[ -s "$WORK/$backend-query.exe" ]] && sha256sum "$WORK/$backend-query.exe" >>"$WORK/binaries.sha256" || \
        fail "$backend constructor query binary guard could not be recorded"
    timeout 30 "$WORK/$backend-query.exe" "$FIXTURES/empty_capture_two_fields_positive.pgy" 32 \
        >"$WORK/$backend-query.raw" 2>"$WORK/$backend-query.err" || \
        fail "$backend actual named-vacant query failed"
    [[ ! -s "$WORK/$backend-query.err" ]] || fail "$backend query wrote stderr"
    tr -d '\r' <"$WORK/$backend-query.raw" >"$WORK/$backend-query.run"
    grep -Fxq 'NAMED VACANT 13 FAULTS AND RESTORATIONS PASS' "$WORK/$backend-query.run" || \
        fail "$backend named-vacant query oracle drift"
    case_index=0
    for case_row in "${CASES[@]}"; do
        IFS='|' read -r fixture verdict expected_diagnostic expected_boundary expected_function \
            <<<"$case_row"
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$fixture" diagnostic \
            >"$WORK/$backend-$case_index.raw" \
            2>"$WORK/$backend-$case_index.err" || \
            fail "$backend fixture=$fixture analyzer refused"
        [[ ! -s "$WORK/$backend-$case_index.err" ]] || \
            fail "$backend fixture=$fixture wrote stderr"
        tr -d '\r' <"$WORK/$backend-$case_index.raw" \
            >"$WORK/$backend-$case_index.run"
        if [[ "$verdict" == pass ]]; then
            grep -Fxq 'body_ok=true' "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture did not pass"
            grep -Fxq 'body_diagnostic=' "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture reported a diagnostic"
        else
            grep -Fxq 'body_ok=false' "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture did not fail closed"
            grep -Fxq "body_diagnostic=$expected_diagnostic" \
                "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture diagnostic drift"
            grep -Fxq -- "- boundary: $expected_boundary" \
                "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture boundary drift"
            if [[ -n "$expected_function" ]]; then
                grep -Fxq "body_function=$expected_function" "$WORK/$backend-$case_index.run" || \
                    fail "$backend fixture=$fixture lost the conflicting call location"
            fi
            if [[ "$expected_boundary" == aggregate_release_incomplete ]]; then
                grep -Eq '^body_syntax=[1-9][0-9]*$' "$WORK/$backend-$case_index.run" || \
                    fail "$backend fixture=$fixture lost the failed event identity"
                grep -Eq '^body_function=.+$' "$WORK/$backend-$case_index.run" || \
                    fail "$backend fixture=$fixture lost its callable identity"
            fi
        fi
        case_index=$((case_index + 1))
    done
    timeout 30 "$WORK/$backend-source.exe" \
        "$FIXTURES/aggregate_readonly_call_chain_positive.pgy" \
        collection-execution-context-generation \
        >"$WORK/$backend-generation.raw" 2>"$WORK/$backend-generation.err" || \
        fail "$backend execution context generation guards failed"
    [[ ! -s "$WORK/$backend-generation.err" ]] || \
        fail "$backend execution context generation guards wrote stderr"
    tr -d '\r' <"$WORK/$backend-generation.raw" >"$WORK/$backend-generation.run"
    [[ "$(cat "$WORK/$backend-generation.run")" == 'EXECUTION CONTEXT GENERATION GUARDS PASS' ]] || \
        fail "$backend execution context generation oracle drift"
done
for case_index in "${!CASES[@]}" generation; do
    cmp "$WORK/c-$case_index.run" "$WORK/llvm-$case_index.run" || \
        fail "C/LLVM analyzer parity drift at case=$case_index"
done
for manifest in native inputs imports; do
    sha256sum --quiet -c "$WORK/$manifest.sha256"
done
echo "[$LABEL] ${#CASES[@]} source cases per C/LLVM PASS; source targets never emitted/run"
