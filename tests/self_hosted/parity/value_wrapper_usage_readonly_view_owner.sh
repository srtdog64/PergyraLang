#!/usr/bin/env bash
# Current-descriptor unit admission and production owner native value checks.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=value-wrapper-readonly-view
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/value-wrapper-view.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
INPUT=tests/self_hosted/parity/fixture/value_wrapper_usage_readonly_view_probe.pgy
UNSAFE=tests/concept_semantics/hashmap/public_array_drop_slice_negative.pgy
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
POSITIVE=(inout_recursive_current_descriptor_positive inout_index_formal_shallow_positive
    inout_index_after_shallow_positive)
NEGATIVE=(inout_index_formal_unknown_negative inout_index_formal_deferred_unknown_negative
    own_formal_shallow_after_drop_negative own_formal_double_forward_negative
    inout_borrowed_push_sibling_indexed_negative inout_borrowed_push_deferred_indexed_negative
    inout_borrowed_loop_consume_negative inout_terminal_formal_retention_negative
    inout_repeated_descriptor_retention_negative inout_use_unique_scalar_raw_negative
    inout_shallow_accumulator_owned_mutation_negative)
sha256sum "$PGY" "$INPUT" "$UNSAFE" "$PROBE" >"$B/input.sha256"
for name in "${POSITIVE[@]}" "${NEGATIVE[@]}"; do
    sha256sum "$FIXTURES/$name.pgy" >>"$B/input.sha256"
done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
printf 'VALUE WRAPPER READONLY VIEW PASS\n' >"$B/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" \
        -o "$B/$backend-values.exe" >"$B/$backend-values.compile" 2>&1 || fail "$backend values did not build"
    timeout 30 "$B/$backend-values.exe" >"$B/$backend-values.raw" 2>"$B/$backend-values.err" || fail "$backend values failed"
    test ! -s "$B/$backend-values.err" || fail "$backend values wrote stderr"
    tr -d '\r' <"$B/$backend-values.raw" >"$B/$backend-values.run"
    cmp "$B/expected" "$B/$backend-values.run" || fail "$backend canonical/dedup/view oracle drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" \
        -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not build"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$FIXTURES/inout_recursive_current_descriptor_positive.pgy" \
        -o "$B/$backend-recursive.exe" >"$B/$backend-recursive.compile" 2>&1 || fail "$backend recursive values did not build"
    timeout 30 "$B/$backend-recursive.exe" >"$B/$backend-recursive.raw" 2>"$B/$backend-recursive.err" || fail "$backend recursive execution failed"
    test ! -s "$B/$backend-recursive.err" || fail "$backend recursive execution wrote stderr"
    tr -d '\r' <"$B/$backend-recursive.raw" >"$B/$backend-recursive.run"
    printf 'RECURSIVE CURRENT DESCRIPTOR PASS\n' >"$B/recursive-expected"
    cmp "$B/recursive-expected" "$B/$backend-recursive.run" || fail "$backend recursive read oracle drift"
    for name in "${POSITIVE[@]}" "${NEGATIVE[@]}"; do
        timeout 60 "$B/$backend-observer.exe" "$FIXTURES/$name.pgy" diagnostic \
            >"$B/$backend-$name.observe" 2>&1 || fail "$backend $name observation failed"
        tr -d '\r' <"$B/$backend-$name.observe" >"$B/$backend-$name.normalized"
        if [[ "$name" == *_positive ]]; then
            grep -Fxq 'body_ok=true' "$B/$backend-$name.normalized" || fail "$backend refused $name"
        else
            grep -Fxq 'body_ok=false' "$B/$backend-$name.normalized" || fail "$backend admitted $name"
            grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' \
                "$B/$backend-$name.normalized" || fail "$backend lost ownership diagnosis for $name"
        fi
    done
    timeout 60 "$B/$backend-observer.exe" "$UNSAFE" diagnostic >"$B/$backend-unsafe.observe" 2>&1 || fail "$backend Slice invalidation observation failed"
    tr -d '\r' <"$B/$backend-unsafe.observe" >"$B/$backend-unsafe.normalized"
    grep -Fxq 'body_ok=false' "$B/$backend-unsafe.normalized" || fail "$backend granted Slice-invalidating release"
    grep -Fxq 'body_diagnostic=slice_storage_invalidation' "$B/$backend-unsafe.normalized" || fail "$backend lost Slice invalidation diagnostic"
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] native production wrapper/recursive read values, three source positives and twelve refusals per backend PASS; recursive release, full production-source admission and fixed point remain separate; evidence=$B"
