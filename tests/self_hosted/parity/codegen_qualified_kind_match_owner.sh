#!/usr/bin/env bash
# Qualified-key query lifetime and unchanged scalar/member selection.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=qualified-kind-match
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/qualified-kind-match.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
INPUT=tests/self_hosted/fixtures/codegen_qualified_kind_match_probe.pgy
LIFETIME=tests/self_hosted/parity/fixture/qualified_kind_match_lifetime_positive.pgy
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
NEGATIVE=(owned_string_local_literal_transfer_reuse_negative
    owned_string_local_literal_transfer_borrowed_negative
    owned_string_local_literal_transfer_conditional_negative
    owned_string_local_literal_transfer_shadow_negative
    owned_string_local_literal_transfer_alias_negative
    owned_string_local_literal_transfer_allocator_formal_negative
    owned_string_literal_transfer_aliased_result_negative
    owned_string_literal_transfer_deferred_borrow_negative)
sha256sum "$PGY" "$INPUT" "$LIFETIME" "$PROBE" "${BASH_SOURCE[0]}" >"$B/input.sha256"
for name in "${NEGATIVE[@]}"; do
    sha256sum "$FIXTURES/$name.pgy" >>"$B/input.sha256"
done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
grep -Fq 'func LookupQualifiedKindTypeMatches(' src/self_hosted/codegen/type_facts/type_env.pgy || fail "query owner missing"
grep -Fq 'LookupQualifiedKindTypeMatches(' src/self_hosted/codegen/emission/expr_semantic_type_owner.pgy || fail "member consumer bypasses query owner"
! grep -Fq 'retired_variant_key' src/self_hosted/codegen/emission/expr_semantic_type_owner.pgy || fail "branch-local retirement path reopened"
printf 'QUALIFIED KIND MATCH PASS\n' >"$B/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" \
        -o "$B/$backend-values.exe" >"$B/$backend-values.compile" 2>&1 || fail "$backend values did not build"
    timeout 30 "$B/$backend-values.exe" >"$B/$backend-values.raw" 2>"$B/$backend-values.err" || fail "$backend values failed"
    test ! -s "$B/$backend-values.err" || fail "$backend values wrote stderr"
    tr -d '\r' <"$B/$backend-values.raw" >"$B/$backend-values.run"
    cmp "$B/expected" "$B/$backend-values.run" || fail "$backend member/precedence oracle drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" \
        -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not build"
    timeout 60 "$B/$backend-observer.exe" "$LIFETIME" diagnostic >"$B/$backend-lifetime.observe" 2>&1 || fail "$backend lifetime observation failed"
    tr -d '\r' <"$B/$backend-lifetime.observe" >"$B/$backend-lifetime.normalized"
    grep -Fxq 'body_ok=true' "$B/$backend-lifetime.normalized" || fail "$backend refused actual query lifetime"
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
echo "[$LABEL] native C/LLVM member/precedence values, source query lifetime and eight preserved refusals per backend PASS; evidence=$B"
