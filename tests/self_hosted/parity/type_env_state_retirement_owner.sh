#!/usr/bin/env bash
# Production scope epochs plus primitive identity and lifetime falsifiers.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=type-env-state-retirement
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/type-env-state-retirement.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
F=tests/self_hosted/parity/fixture
C="$F/collection_field_lifetime"
POSITIVES=("$F/type_env_state_retirement_frontier_probe.pgy|TYPE ENV STATE RETIREMENT PASS"
    "$C/owned_string_join_actual_positive.pgy|OWNED STRING JOIN PASS")
NEGATIVES=("$F/type_env_state_repeated_retirement_negative.pgy"
    "$F/type_env_state_alias_retirement_negative.pgy"
    "$F/type_env_state_borrowed_input_negative.pgy"
    "$C/owned_string_join_alias_negative.pgy"
    "$C/owned_string_join_retained_result_negative.pgy"
    "$C/owned_string_literal_transfer_aliased_result_negative.pgy"
    "$C/owned_string_literal_transfer_aliased_concat_result_negative.pgy"
    "$C/owned_string_named_actual_after_negative.pgy"
    "$C/owned_string_named_actual_duplicate_negative.pgy"
    "$C/owned_string_named_actual_deferred_negative.pgy"
    "$C/owned_string_named_actual_reassigned_negative.pgy"
    "$C/aggregate_release_field_writeback_missing_negative.pgy"
    "$C/aggregate_owned_push_borrowed_input_negative.pgy")
sha256sum "$PGY" "$PROBE" tests/self_hosted/fixtures/owned_string_join_identity_unit.pgy "${BASH_SOURCE[0]}" >"$B/input.sha256"
for row in "${POSITIVES[@]}"; do sha256sum "${row%%|*}" >>"$B/input.sha256"; done
sha256sum "${NEGATIVES[@]}" >>"$B/input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not build"
    index=0
    for row in "${POSITIVES[@]}"; do
        IFS='|' read -r input oracle <<<"$row"
        timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$input" -o "$B/$backend-$index-value.exe" >"$B/$backend-$index-value.compile" 2>&1 || fail "$backend value $index did not build"
        timeout 30 "$B/$backend-$index-value.exe" >"$B/$backend-$index-value.raw" 2>"$B/$backend-$index-value.err" || fail "$backend value $index failed"
        test ! -s "$B/$backend-$index-value.err" || fail "$backend value $index stderr"
        tr -d '\r' <"$B/$backend-$index-value.raw" >"$B/$backend-$index-value.run"
        printf '%s\n' "$oracle" >"$B/$backend-$index.expected"
        cmp "$B/$backend-$index.expected" "$B/$backend-$index-value.run" || fail "$backend value $index drift"
        timeout 60 "$B/$backend-observer.exe" "$input" diagnostic >"$B/$backend-$index-source.raw" 2>"$B/$backend-$index-source.err" || fail "$backend source $index observation failed"
        test ! -s "$B/$backend-$index-source.err" || fail "$backend source $index stderr"
        tr -d '\r' <"$B/$backend-$index-source.raw" >"$B/$backend-$index-source.run"
        grep -Fxq 'body_ok=true' "$B/$backend-$index-source.run" || fail "$backend source $index refused"
        grep -Fxq 'formal_ready=true' "$B/$backend-$index-source.run" || fail "$backend formal carrier invalid"
        index=$((index + 1))
    done
    timeout 60 "$B/$backend-observer.exe" "$C/owned_string_join_actual_positive.pgy" owned-string-join-identity >"$B/$backend-identity.raw" 2>"$B/$backend-identity.err" || fail "$backend identity guards failed"
    test ! -s "$B/$backend-identity.err" || fail "$backend identity stderr"
    tr -d '\r' <"$B/$backend-identity.raw" >"$B/$backend-identity.run"
    printf 'OWNED STRING JOIN IDENTITY PASS\n' >"$B/identity.expected"
    cmp "$B/identity.expected" "$B/$backend-identity.run" || fail "$backend identity guard drift"
    index=0
    for input in "${NEGATIVES[@]}"; do
        timeout 60 "$B/$backend-observer.exe" "$input" diagnostic >"$B/$backend-$index-negative.raw" 2>"$B/$backend-$index-negative.err" || fail "$backend negative $index observation failed"
        test ! -s "$B/$backend-$index-negative.err" || fail "$backend negative $index stderr"
        tr -d '\r' <"$B/$backend-$index-negative.raw" >"$B/$backend-$index-negative.run"
        grep -Fxq 'body_ok=false' "$B/$backend-$index-negative.run" || fail "$backend negative $index admitted"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' "$B/$backend-$index-negative.run" || fail "$backend negative $index diagnosis drift"
        index=$((index + 1))
    done
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] C/LLVM scope/join values, identity mutations and thirteen lifetime refusals/backend PASS; evidence=$B"
