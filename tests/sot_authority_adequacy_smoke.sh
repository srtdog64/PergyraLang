#!/usr/bin/env bash
# Binds the bounded Coq SoT authority model to the first live typed-expression
# substitution slice. This is source-consistency evidence, not extraction.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="docs/semantics/proofs/SoTAuthority.v"
OWNER="src/self_hosted/semantic/ast_local_binding_fact_owner.pgy"
CONSUMER="src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy"

fail() {
    echo "[sot-authority] $*" >&2
    exit 1
}

require_file() {
    [[ -f "$ROOT_DIR/$1" ]] || fail "missing $1"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

reject_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden fallback term: $term"
    fi
}

check_owner_copy() {
    local path="$1"
    grep -Fq -- "initializer_array_bodies: Array<String>;" "$path" &&
        grep -Fq -- "has_initializer_array_bodies: Array<Int>;" "$path" &&
        grep -Fq -- "func SemanticAstLocalBindingArrayLiteralBodyAt(" "$path"
}

check_consumer_copy() {
    local path="$1"
    grep -Fq -- "SemanticAstLocalBindingArrayLiteralBodyAt(" "$path" &&
        ! grep -Fq -- "StringTrim(" "$path" &&
        ! grep -Fq -- "CharAt(" "$path" &&
        ! grep -Fq -- "TypedAstArenaValueText" "$path" &&
        ! grep -Fq -- "CodegenAstArenaValueOrDie" "$path"
}

require_file "$PROOF"
require_file "docs/semantics/proofs/SoTAuthority.md"
require_file "$OWNER"
require_file "$CONSUMER"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_array_literal_owner.pgy" ]] ||
    fail "retired AST-text array-literal owner returned"

for term in \
    "Definition AuthorityComplete" \
    "Definition AuthorityUnique" \
    "Definition RequiredFactsConsumed" \
    "Definition NoSemanticFallback" \
    "Definition RungClosed" \
    "Theorem closed_required_fact_has_exactly_one_authority" \
    "Theorem closed_semantic_read_is_not_fallback" \
    "Theorem current_array_literal_rung_closed" \
    "Theorem owned_plus_fallback_bridge_is_not_closed" \
    "Theorem duplicate_semantic_producer_is_not_closed" \
    "Theorem missing_required_fact_is_not_closed"; do
    require_text "$PROOF" "$term"
done

require_text "$PROOF" "FInitializerArrayBody"
require_text "$PROOF" "OSemanticLocalBindingFacts"
require_text "$PROOF" "CArrayLiteralEmitter"
require_text "$PROOF" "OCodegenTextRecovery"

check_owner_copy "$ROOT_DIR/$OWNER" ||
    fail "live semantic owner does not provide the modeled array body fact"
check_consumer_copy "$ROOT_DIR/$CONSUMER" ||
    fail "live codegen consumer reopened text recovery"

reject_text "$CONSUMER" "StringTrim("
reject_text "$CONSUMER" "CharAt("
reject_text "$CONSUMER" "TypedAstArenaValueText"
reject_text "$CONSUMER" "CodegenAstArenaValueOrDie"

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/pgy-sot-authority.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

cp "$ROOT_DIR/$OWNER" "$tmp_dir/owner_missing.pgy"
sed 's/func SemanticAstLocalBindingArrayLiteralBodyAt(/func RemovedArrayLiteralBodyAt(/' \
    "$tmp_dir/owner_missing.pgy" >"$tmp_dir/owner_missing.next"
mv "$tmp_dir/owner_missing.next" "$tmp_dir/owner_missing.pgy"
if check_owner_copy "$tmp_dir/owner_missing.pgy"; then
    fail "missing-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$CONSUMER" "$tmp_dir/consumer_fallback.pgy"
printf '\nfunc ReintroducedFallback(x: String) -> String { return StringTrim(x); }\n' \
    >>"$tmp_dir/consumer_fallback.pgy"
if check_consumer_copy "$tmp_dir/consumer_fallback.pgy"; then
    fail "fallback mutation was not rejected"
fi

if command -v coqc >/dev/null 2>&1; then
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    if command -v timeout >/dev/null 2>&1; then
        (cd "$ROOT_DIR" && timeout "$coq_timeout" coqc "$PROOF")
    else
        (cd "$ROOT_DIR" && coqc "$PROOF")
    fi
    echo "[sot-authority] Coq model ok"
else
    echo "[sot-authority] Coq model skipped (coqc not found)"
fi

echo "[sot-authority] live owner/consumer binding and negative mutations ok"
