#!/usr/bin/env bash
# Fresh owned Array<String> result proof at the source semantic boundary.
# Fixtures are parsed and analyzed only; this gate makes no MIR/runtime claim.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=collection-owned-result
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-owned-result.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
CASES=(
    "owned_result_forward_late_positive.pgy|pass|none"
    "owned_result_empty_branch_positive.pgy|pass|none"
    "owned_result_map_keys_positive.pgy|pass|none"
    "owned_result_borrowed_branch_negative.pgy|fail|owned_string_drop"
    "owned_result_drop_then_return_negative.pgy|fail|owned_array_result_return_unproved"
    "owned_result_shadowed_push_negative.pgy|fail|owned_string_drop"
    "owned_result_recursion_ungrounded_negative.pgy|fail|owned_string_drop"
    "owned_result_assignment_stale_negative.pgy|fail|ArrayDropOwnedStrings"
    "storage_opaque_return_own_negative.pgy|fail|owned_argument_storage_not_live"
    "storage_opaque_assign_own_negative.pgy|fail|owned_argument_storage_not_live"
    "storage_opaque_alias_own_negative.pgy|fail|owned_argument_storage_not_live"
    "storage_shadowed_clone_own_negative.pgy|fail|owned_argument_storage_not_live"
)
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" >"$WORK/inputs.sha256"
for case_row in "${CASES[@]}"; do
    IFS='|' read -r fixture _ _ <<<"$case_row"
    sha256sum "$FIXTURES/$fixture" >>"$WORK/inputs.sha256"
done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | \
    xargs -0 sha256sum >"$WORK/imports.sha256"
echo "[$LABEL] evidence=$REL; source fixtures are analyzed, never emitted/run"
for backend in c llvm; do
    timeout 240 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" \
        --opt=dev -o "$REL/$backend-source.exe" \
        >"$WORK/$backend-source.compile" 2>&1 || \
        fail "$backend source analyzer did not build"
    case_index=0
    for case_row in "${CASES[@]}"; do
        IFS='|' read -r fixture verdict expected_boundary <<<"$case_row"
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$fixture" \
            diagnostic >"$WORK/$backend-$case_index.raw" \
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
            grep -Fxq 'body_diagnostic=borrow_boundary_escape' \
                "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture diagnostic drift"
            grep -Fxq -- "- boundary: $expected_boundary" \
                "$WORK/$backend-$case_index.run" || \
                fail "$backend fixture=$fixture boundary drift"
        fi
        case_index=$((case_index + 1))
    done
done
for case_index in "${!CASES[@]}"; do
    cmp "$WORK/c-$case_index.run" "$WORK/llvm-$case_index.run" || \
        fail "C/LLVM analyzer parity drift at case=$case_index"
done
for manifest in native inputs imports; do
    sha256sum --quiet -c "$WORK/$manifest.sha256"
done
sha256sum "$WORK/c-source.exe" "$WORK/llvm-source.exe" \
    >"$WORK/binaries.sha256"
echo "[$LABEL] 3 positive + 9 falsifying cases per C/LLVM PASS"
