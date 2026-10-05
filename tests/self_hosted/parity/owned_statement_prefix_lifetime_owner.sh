#!/usr/bin/env bash
# A real codegen prefix owns one borrow-before-transfer phase, then retires it.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=owned-statement-prefix
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned-statement-prefix.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
INPUT=tests/self_hosted/parity/fixture/owned_statement_prefix_lifetime_probe.pgy
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
NEGATIVE=(owned_string_literal_transfer_concat_after_negative
    owned_string_literal_transfer_retaining_before_negative
    owned_string_literal_transfer_unproved_before_negative
    owned_string_literal_transfer_after_use_negative
    owned_string_literal_transfer_aliased_result_negative
    owned_string_literal_transfer_aliased_concat_result_negative
    owned_string_literal_transfer_borrowed_negative
    owned_string_literal_transfer_deferred_borrow_negative
    owned_string_literal_transfer_multi_element_negative
    owned_string_local_literal_transfer_reuse_negative
    owned_string_local_literal_transfer_conditional_negative
    owned_string_local_literal_transfer_allocator_formal_negative)
sha256sum "$PGY" "$INPUT" "$PROBE" "${BASH_SOURCE[0]}" \
    "$FIXTURES/owned_string_literal_transfer_positive.pgy" >"$B/input.sha256"
for name in "${NEGATIVE[@]}"; do
    sha256sum "$FIXTURES/$name.pgy" >>"$B/input.sha256"
done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
grep -Fq 'SemanticAstOwnedStringLiteralTransferBorrowUseExclusive(' src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy || fail "formal transfer bypasses shared borrow owner"
! grep -Fq 'SemanticAstOwnedStringLiteralTransferBorrowUseReady(' src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy || fail "duplicate borrow policy reopened"
! grep -Fq '"COMPILER_ARTIFACT_WRITE"' src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy || fail "formal transfer rebuilt a builtin exception"
grep -Fq 'SemanticAstOwnedStringFreshConcatCallReady(' src/self_hosted/semantic/ast_owned_string_actual_exclusivity_owner.pgy || fail "actual exclusivity bypasses the fresh primitive owner"
! grep -Fq '"CONCAT"' src/self_hosted/semantic/ast_owned_string_actual_exclusivity_owner.pgy || fail "actual exclusivity rebuilt primitive identity policy"
printf 'OWNED STATEMENT PREFIX PASS\n' >"$B/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" \
        -o "$B/$backend-values.exe" >"$B/$backend-values.compile" 2>&1 || fail "$backend values did not build"
    timeout 30 "$B/$backend-values.exe" >"$B/$backend-values.raw" 2>"$B/$backend-values.err" || fail "$backend values failed"
    test ! -s "$B/$backend-values.err" || fail "$backend values wrote stderr"
    tr -d '\r' <"$B/$backend-values.raw" >"$B/$backend-values.run"
    cmp "$B/expected" "$B/$backend-values.run" || fail "$backend text identity drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" \
        -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not build"
    for positive in "$INPUT" "$FIXTURES/owned_string_literal_transfer_positive.pgy"; do
        name="$(basename "$positive" .pgy)"
        timeout 60 "$B/$backend-observer.exe" "$positive" diagnostic >"$B/$backend-$name.observe" 2>&1 || fail "$backend $name observation failed"
        tr -d '\r' <"$B/$backend-$name.observe" >"$B/$backend-$name.normalized"
        grep -Fxq 'body_ok=true' "$B/$backend-$name.normalized" || fail "$backend refused $name"
    done
    for name in "${NEGATIVE[@]}"; do
        timeout 60 "$B/$backend-observer.exe" "$FIXTURES/$name.pgy" diagnostic >"$B/$backend-$name.observe" 2>&1 || fail "$backend $name observation failed"
        tr -d '\r' <"$B/$backend-$name.observe" >"$B/$backend-$name.normalized"
        grep -Fxq 'body_ok=false' "$B/$backend-$name.normalized" || fail "$backend admitted $name"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$B/$backend-$name.normalized" || fail "$backend lost ownership diagnosis for $name"
    done
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] native C/LLVM prefix values, two source positives and twelve preserved refusals per backend PASS; evidence=$B"
