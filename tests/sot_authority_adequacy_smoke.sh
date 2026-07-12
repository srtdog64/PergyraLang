#!/usr/bin/env bash
# Binds the bounded Coq SoT authority model to the first live typed-expression
# substitution slice. This is source-consistency evidence, not extraction.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="docs/semantics/proofs/SoTAuthority.v"
OWNER="src/self_hosted/semantic/ast_local_binding_fact_owner.pgy"
STATEMENT_OWNER="src/self_hosted/semantic/ast_statement_fact_owner.pgy"
ARRAY_CONSUMER="src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy"
TRY_CONSUMER="src/self_hosted/codegen/input/semantic_try_let_codegen_view_owner.pgy"
COLLECTION_CONSUMER="src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy"

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
        grep -Fq -- "func SemanticAstLocalBindingArrayLiteralBodyAt(" "$path" &&
        grep -Fq -- "initializer_try_operands: Array<String>;" "$path" &&
        grep -Fq -- "has_initializer_try_operands: Array<Int>;" "$path" &&
        grep -Fq -- "func SemanticAstLocalBindingTryOperandAt(" "$path"
}

check_consumer_copy() {
    local path="$1"
    local accessor="$2"
    grep -Fq -- "$accessor" "$path" &&
        ! grep -Fq -- "StringTrim(" "$path" &&
        ! grep -Fq -- "CharAt(" "$path" &&
        ! grep -Fq -- "TypedAstArenaValueText" "$path" &&
        ! grep -Fq -- "CodegenAstArenaValueOrDie" "$path"
}

check_statement_owner_copy() {
    local path="$1"
    grep -Eq -- '^    payload_texts: Array<String>;$' "$path" &&
        grep -Eq -- '^    value_texts: Array<String>;$' "$path" &&
        grep -Eq -- '^    auxiliary_texts: Array<String>;$' "$path" &&
        grep -Fq -- "TypedAstKindArraySetStmtTag()" "$path" &&
        grep -Fq -- "TypedAstKindArrayPushStmtTag()" "$path"
}

require_file "$PROOF"
require_file "docs/semantics/proofs/SoTAuthority.md"
require_file "$OWNER"
require_file "$STATEMENT_OWNER"
require_file "$ARRAY_CONSUMER"
require_file "$TRY_CONSUMER"
require_file "$COLLECTION_CONSUMER"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_array_literal_owner.pgy" ]] ||
    fail "retired AST-text array-literal owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_try_let_owner.pgy" ]] ||
    fail "retired AST-text try-let owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_collection_stmt_owner.pgy" ]] ||
    fail "retired AST-text collection statement owner returned"

for term in \
    "Definition AuthorityComplete" \
    "Definition AuthorityUnique" \
    "Definition RequiredFactsConsumed" \
    "Definition NoSemanticFallback" \
    "Definition RungClosed" \
    "Theorem closed_required_fact_has_exactly_one_authority" \
    "Theorem closed_semantic_read_is_not_fallback" \
    "Theorem current_array_literal_rung_closed" \
    "Theorem current_try_let_rung_closed" \
    "Theorem current_collection_mutation_rung_closed" \
    "Theorem owned_plus_fallback_bridge_is_not_closed" \
    "Theorem try_owner_plus_text_fallback_is_not_closed" \
    "Theorem collection_owner_plus_text_fallback_is_not_closed" \
    "Theorem duplicate_semantic_producer_is_not_closed" \
    "Theorem missing_required_fact_is_not_closed"; do
    require_text "$PROOF" "$term"
done

require_text "$PROOF" "FInitializerArrayBody"
require_text "$PROOF" "FInitializerTryOperand"
require_text "$PROOF" "FCollectionMutationParts"
require_text "$PROOF" "OSemanticLocalBindingFacts"
require_text "$PROOF" "OSemanticStatementFacts"
require_text "$PROOF" "CArrayLiteralEmitter"
require_text "$PROOF" "CTryLetEmitter"
require_text "$PROOF" "CCollectionMutationEmitter"
require_text "$PROOF" "OCodegenTextRecovery"

check_owner_copy "$ROOT_DIR/$OWNER" ||
    fail "live semantic owner does not provide the modeled array body fact"
check_statement_owner_copy "$ROOT_DIR/$STATEMENT_OWNER" ||
    fail "live semantic statement owner does not provide collection facts"
check_consumer_copy "$ROOT_DIR/$ARRAY_CONSUMER" \
    "SemanticAstLocalBindingArrayLiteralBodyAt(" ||
    fail "live array codegen consumer reopened text recovery"
check_consumer_copy "$ROOT_DIR/$TRY_CONSUMER" \
    "SemanticAstLocalBindingTryOperandAt(" ||
    fail "live try codegen consumer reopened text recovery"
require_text "$TRY_CONSUMER" "SemanticAstLocalBindingTryOperandAt("
check_consumer_copy "$ROOT_DIR/$COLLECTION_CONSUMER" \
    "CodegenSemanticArraySetTargetOrDie(" ||
    fail "live collection codegen consumer reopened text recovery"
require_text "$COLLECTION_CONSUMER" "CodegenSemanticArrayPushValueOrDie("

for consumer in "$ARRAY_CONSUMER" "$TRY_CONSUMER" "$COLLECTION_CONSUMER"; do
    reject_text "$consumer" "StringTrim("
    reject_text "$consumer" "CharAt("
    reject_text "$consumer" "TypedAstArenaAtomText"
    reject_text "$consumer" "TypedAstArenaValueText"
    reject_text "$consumer" "TypedAstArenaAuxValueText"
    reject_text "$consumer" "CodegenAstArenaValueOrDie"
    reject_text "$consumer" "ContainsOutsideStrings("
    reject_text "$consumer" "FindMatchingParen("
done

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/pgy-sot-authority.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

cp "$ROOT_DIR/$OWNER" "$tmp_dir/owner_missing.pgy"
sed 's/func SemanticAstLocalBindingArrayLiteralBodyAt(/func RemovedArrayLiteralBodyAt(/' \
    "$tmp_dir/owner_missing.pgy" >"$tmp_dir/owner_missing.next"
mv "$tmp_dir/owner_missing.next" "$tmp_dir/owner_missing.pgy"
if check_owner_copy "$tmp_dir/owner_missing.pgy"; then
    fail "missing-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$STATEMENT_OWNER" "$tmp_dir/statement_owner_missing.pgy"
sed 's/value_texts: Array<String>;/removed_value_texts: Array<String>;/' \
    "$tmp_dir/statement_owner_missing.pgy" \
    >"$tmp_dir/statement_owner_missing.next"
mv "$tmp_dir/statement_owner_missing.next" \
    "$tmp_dir/statement_owner_missing.pgy"
if check_statement_owner_copy "$tmp_dir/statement_owner_missing.pgy"; then
    fail "missing statement-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$TRY_CONSUMER" "$tmp_dir/consumer_fallback.pgy"
printf '\nfunc ReintroducedFallback(x: String) -> String { return StringTrim(x); }\n' \
    >>"$tmp_dir/consumer_fallback.pgy"
if check_consumer_copy "$tmp_dir/consumer_fallback.pgy" \
    "SemanticAstLocalBindingTryOperandAt("; then
    fail "fallback mutation was not rejected"
fi


cp "$ROOT_DIR/$COLLECTION_CONSUMER" \
    "$tmp_dir/collection_consumer_fallback.pgy"
printf '\nfunc ReintroducedCollectionFallback(x: String) -> String { return StringTrim(x); }\n' \
    >>"$tmp_dir/collection_consumer_fallback.pgy"
if check_consumer_copy "$tmp_dir/collection_consumer_fallback.pgy" \
    "CodegenSemanticArraySetTargetOrDie("; then
    fail "collection fallback mutation was not rejected"
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
