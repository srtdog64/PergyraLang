#!/usr/bin/env bash
# Reached production epoch plus own-formal content, escape and order falsifiers.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=expression-root-consumption
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/expression-root-consumption.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
INPUT=tests/self_hosted/parity/fixture/codegen_expression_root_commit_frontier_probe.pgy
INVALID=tests/self_hosted/parity/fixture/codegen_expression_root_commit_invalid_probe.pgy
F=tests/self_hosted/parity/fixture/collection_field_lifetime
POSITIVE=("$INPUT" "$F/own_indexed_copy_retirement_positive.pgy"
    "$F/own_wrapper_owned_positive.pgy" "$F/shadow_owned_drop_callable_positive.pgy")
NEGATIVE=(own_indexed_copy_retirement_borrowed_negative
    own_indexed_copy_retirement_after_negative own_indexed_copy_retirement_repeat_negative
    own_indexed_copy_retirement_escape_negative own_indexed_copy_retirement_deferred_negative
    own_wrapper_borrowed_negative own_formal_shallow_after_drop_negative
    own_formal_read_after_forward_negative own_formal_double_forward_negative
    indexed_string_alias_after_owned_drop_negative
    indexed_string_unknown_call_after_owned_drop_negative
    indexed_string_tostring_passthrough_after_owned_drop_negative
    default_indexed_deep_retirement_negative own_storage_nested_same_binding_negative)
sha256sum "$PGY" "$PROBE" "$INVALID" "${BASH_SOURCE[0]}" "${POSITIVE[@]}" >"$B/input.sha256"
for name in "${NEGATIVE[@]}"; do sha256sum "$F/$name.pgy" >>"$B/input.sha256"; done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
for consumer in ast_collection_formal_use_owner ast_collection_argument_permission_effect_owner ast_collection_ownership_argument_transfer_owner; do
    grep -Fq 'SemanticAstCollectionBuiltinReleaseArgument(' "src/self_hosted/semantic/$consumer.pgy" || fail "$consumer bypassed release identity"
    ! grep -Fq '"CompilerRetireArrayStorage"' "src/self_hosted/semantic/$consumer.pgy" || fail "$consumer rebuilt release classification"
done
! grep -Fq 'ArrayPush(bodies, mode != 2' src/self_hosted/semantic/ast_collection_formal_effect_identity_owner.pgy || fail "own mode replaced body availability"
! grep -Fq 'mode != 2 &&' src/self_hosted/semantic/ast_collection_formal_effect_readiness_owner.pgy || fail "readiness rebuilt mode-based body policy"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" -o "$B/$backend-epoch.exe" >"$B/$backend-epoch.compile" 2>&1 || fail "$backend production epoch did not compile"
    timeout 30 "$B/$backend-epoch.exe" >"$B/$backend-epoch.raw" 2>"$B/$backend-epoch.err" || fail "$backend production epoch failed"
    test ! -s "$B/$backend-epoch.err" || fail "$backend production epoch stderr"
    tr -d '\r' <"$B/$backend-epoch.raw" >"$B/$backend-epoch.run"
    printf 'EXPRESSION ROOT COMMIT PASS\n' >"$B/epoch.expected"
    cmp "$B/epoch.expected" "$B/$backend-epoch.run" || fail "$backend production epoch value drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INVALID" -o "$B/$backend-invalid.exe" >"$B/$backend-invalid.compile" 2>&1 || fail "$backend invalid epoch probe did not compile"
    for mode in missing wrong; do
        if timeout 30 "$B/$backend-invalid.exe" "$mode" >"$B/$backend-$mode.raw" 2>&1; then fail "$backend accepted $mode epoch"; else status=$?; fi
        test "$status" -ne 124 || fail "$backend $mode epoch timed out"
        message='owned C expression root is absent from its lifetime epoch'
        if [ "$mode" = wrong ]; then message='owned C expression root is not the final lifetime fragment'; fi
        grep -Fq "$message" "$B/$backend-$mode.raw" || fail "$backend lost $mode fatal boundary"
    done
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$F/own_indexed_copy_retirement_positive.pgy" -o "$B/$backend-copy.exe" >"$B/$backend-copy.compile" 2>&1 || fail "$backend copy did not compile"
    timeout 30 "$B/$backend-copy.exe" >"$B/$backend-copy.raw" 2>"$B/$backend-copy.err" || fail "$backend copy failed"
    test ! -s "$B/$backend-copy.err" || fail "$backend copy stderr"
    tr -d '\r' <"$B/$backend-copy.raw" >"$B/$backend-copy.run"
    printf 'OWN INDEXED COPY RETIREMENT PASS\n' >"$B/copy.expected"
    cmp "$B/copy.expected" "$B/$backend-copy.run" || fail "$backend copied value drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not compile"
    for input in "${POSITIVE[@]}"; do
        name="$(basename "$input" .pgy)"
        timeout 60 "$B/$backend-observer.exe" "$input" diagnostic >"$B/$backend-$name.observe" 2>&1 || fail "$backend $name observation failed"
        tr -d '\r' <"$B/$backend-$name.observe" >"$B/$backend-$name.normalized"
        grep -Fxq 'body_ok=true' "$B/$backend-$name.normalized" || fail "$backend refused $name"
        grep -Fxq 'formal_ready=true' "$B/$backend-$name.normalized" || fail "$backend retained invalid formal facts for $name"
    done
    for name in "${NEGATIVE[@]}"; do
        timeout 60 "$B/$backend-observer.exe" "$F/$name.pgy" diagnostic >"$B/$backend-$name.observe" 2>&1 || fail "$backend $name observation failed"
        tr -d '\r' <"$B/$backend-$name.observe" >"$B/$backend-$name.normalized"
        grep -Fxq 'body_ok=false' "$B/$backend-$name.normalized" || fail "$backend admitted $name"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' "$B/$backend-$name.normalized" || fail "$backend lost lifetime diagnosis for $name"
    done
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] native C/LLVM production/copy values, two fatal epochs, four source positives and fourteen lifetime refusals per backend PASS; evidence=$B"
