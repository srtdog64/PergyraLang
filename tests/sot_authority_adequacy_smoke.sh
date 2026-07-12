#!/usr/bin/env bash
# Binds the bounded Coq SoT authority model to the first live typed-expression
# substitution slice. This is source-consistency evidence, not extraction.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="docs/semantics/proofs/SoTAuthority.v"
OWNER="src/self_hosted/semantic/ast_local_binding_fact_owner.pgy"
STATEMENT_OWNER="src/self_hosted/semantic/ast_statement_fact_owner.pgy"
ENUM_OWNER="src/self_hosted/semantic/ast_enum_fact_owner.pgy"
NOMINAL_OWNER="src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy"
ROLE_OWNER="src/self_hosted/semantic/ast_role_fact_owner.pgy"
EXPRESSION_SURFACE_OWNER="src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy"
TYPE_SURFACE_OWNER="src/self_hosted/semantic/ast_type_surface_fact_owner.pgy"
KIND_SURFACE_OWNER="src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy"
SIGNATURE_OWNER="src/self_hosted/semantic/ast_signature_fact_owner.pgy"
ARRAY_CONSUMER="src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy"
TRY_CONSUMER="src/self_hosted/codegen/input/semantic_try_let_codegen_view_owner.pgy"
COLLECTION_CONSUMER="src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy"
ENUM_CONSUMER="src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy"
ENUM_EMITTER="src/self_hosted/codegen/emission/function_emit.pgy"
NOMINAL_CONSUMER="src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy"
ROLE_CONSUMER="src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy"
EXPRESSION_USAGE_CONSUMER="src/self_hosted/codegen/input/ast_expression_usage_owner.pgy"
TYPE_USAGE_CONSUMER="src/self_hosted/codegen/input/ast_type_usage_owner.pgy"
KIND_USAGE_CONSUMER="src/self_hosted/codegen/input/ast_kind_usage_owner.pgy"
ENTRYPOINT_VERDICT_CONSUMER="src/self_hosted/semantic/ast_artifact_verdict_owner.pgy"
ENTRYPOINT_PROJECTION_CONSUMER="src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy"
PROGRAM_EMITTER="src/self_hosted/codegen/emission/program_emit.pgy"

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

check_enum_owner_copy() {
    local path="$1"
    grep -Eq -- '^    enum_names: Array<String>;$' "$path" &&
        grep -Eq -- '^    variant_enum_names: Array<String>;$' "$path" &&
        grep -Eq -- '^    variant_names: Array<String>;$' "$path" &&
        grep -Eq -- '^    variant_param_counts: Array<Int>;$' "$path" &&
        grep -Fq -- "func SemanticAstEnumFactsMatchArtifact(" "$path" &&
        grep -Fq -- "func SemanticAstEnumVariantNameAt(" "$path" &&
        grep -Fq -- "func SemanticAstEnumVariantParamCountAt(" "$path"
}

check_nominal_owner_copy() {
    local path="$1"
    grep -Eq -- '^    names: Array<String>;$' "$path" &&
        grep -Eq -- '^    field_names: Array<String>;$' "$path" &&
        grep -Eq -- '^    param_type_names: Array<String>;$' "$path" &&
        grep -Fq -- "func SemanticAstNominalConstructorNameAt(" "$path" &&
        grep -Fq -- "func SemanticAstNominalConstructorFieldNameAt(" "$path" &&
        grep -Fq -- "func SemanticAstNominalConstructorFieldTypeAt(" "$path"
}

check_role_owner_copy() {
    local path="$1"
    grep -Eq -- '^    role_node_ids: Array<Int>;$' "$path" &&
        grep -Eq -- '^    target_type_names: Array<String>;$' "$path" &&
        grep -Eq -- '^    method_node_ids: Array<Int>;$' "$path" &&
        grep -Fq -- "func SemanticAstRoleAncestorNodeId(" "$path" &&
        grep -Fq -- "func SemanticAstRoleMethodNodeAt(" "$path" &&
        grep -Fq -- "func SemanticAstRoleFactsMatchArtifact(" "$path"
}

check_expression_surface_owner_copy() {
    local path="$1"
    grep -Eq -- '^    node_ids: Array<Int>;$' "$path" &&
        grep -Eq -- '^    atoms: Array<String>;$' "$path" &&
        grep -Eq -- '^    values: Array<String>;$' "$path" &&
        grep -Eq -- '^    auxiliary_values: Array<String>;$' "$path" &&
        grep -Fq -- "func SemanticAstExpressionSurfaceCallPresent(" "$path" &&
        grep -Fq -- "func SemanticAstExpressionSurfaceTokenPresent(" "$path" &&
        grep -Fq -- "func SemanticAstExpressionSurfaceFactsMatchArtifact(" "$path"
}

check_type_surface_owner_copy() {
    local path="$1"
    grep -Eq -- '^    node_ids: Array<Int>;$' "$path" &&
        grep -Eq -- '^    type_names: Array<String>;$' "$path" &&
        grep -Fq -- "func SemanticAstTypeSurfaceNameAt(" "$path" &&
        grep -Fq -- "func SemanticAstTypeSurfaceContains(" "$path" &&
        grep -Fq -- "func SemanticAstTypeSurfaceFactsMatchArtifact(" "$path"
}

check_kind_surface_owner_copy() {
    local path="$1"
    grep -Eq -- '^    node_ids: Array<Int>;$' "$path" &&
        grep -Eq -- '^    kind_tags: Array<Int>;$' "$path" &&
        grep -Fq -- "func SemanticAstKindSurfaceContains(" "$path" &&
        grep -Fq -- "func SemanticAstKindSurfaceFactsMatchArtifact(" "$path"
}

check_signature_owner_copy() {
    local path="$1"
    grep -Eq -- '^    function_node_ids: Array<Int>;$' "$path" &&
        grep -Eq -- '^    names: Array<String>;$' "$path" &&
        grep -Fq -- "func SemanticAstFunctionSignatureCount(" "$path" &&
        grep -Fq -- "func SemanticAstFunctionNodeAt(" "$path" &&
        grep -Fq -- "func SemanticAstFunctionNameAt(" "$path"
}

require_file "$PROOF"
require_file "docs/semantics/proofs/SoTAuthority.md"
require_file "$OWNER"
require_file "$STATEMENT_OWNER"
require_file "$ENUM_OWNER"
require_file "$NOMINAL_OWNER"
require_file "$ROLE_OWNER"
require_file "$EXPRESSION_SURFACE_OWNER"
require_file "$TYPE_SURFACE_OWNER"
require_file "$KIND_SURFACE_OWNER"
require_file "$SIGNATURE_OWNER"
require_file "$ARRAY_CONSUMER"
require_file "$TRY_CONSUMER"
require_file "$COLLECTION_CONSUMER"
require_file "$ENUM_CONSUMER"
require_file "$ENUM_EMITTER"
require_file "$NOMINAL_CONSUMER"
require_file "$ROLE_CONSUMER"
require_file "$EXPRESSION_USAGE_CONSUMER"
require_file "$TYPE_USAGE_CONSUMER"
require_file "$KIND_USAGE_CONSUMER"
require_file "$ENTRYPOINT_VERDICT_CONSUMER"
require_file "$ENTRYPOINT_PROJECTION_CONSUMER"
require_file "$PROGRAM_EMITTER"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_array_literal_owner.pgy" ]] ||
    fail "retired AST-text array-literal owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_try_let_owner.pgy" ]] ||
    fail "retired AST-text try-let owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_collection_stmt_owner.pgy" ]] ||
    fail "retired AST-text collection statement owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_enum_variant_owner.pgy" ]] ||
    fail "retired AST-text enum variant owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_declaration_owner.pgy" ]] ||
    fail "retired mixed AST-text declaration owner returned"
[[ ! -e "$ROOT_DIR/src/self_hosted/codegen/input/ast_text_role_declaration_owner.pgy" ]] ||
    fail "retired AST-text role declaration owner returned"

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
    "Theorem current_enum_declaration_rung_closed" \
    "Theorem current_nominal_declaration_rung_closed" \
    "Theorem current_role_declaration_rung_closed" \
    "Theorem current_expression_runtime_usage_rung_closed" \
    "Theorem current_type_runtime_usage_rung_closed" \
    "Theorem current_kind_runtime_usage_rung_closed" \
    "Theorem current_entrypoint_selection_rung_closed" \
    "Theorem owned_plus_fallback_bridge_is_not_closed" \
    "Theorem try_owner_plus_text_fallback_is_not_closed" \
    "Theorem collection_owner_plus_text_fallback_is_not_closed" \
    "Theorem enum_owner_plus_text_fallback_is_not_closed" \
    "Theorem nominal_owner_plus_text_fallback_is_not_closed" \
    "Theorem role_owner_plus_ast_fallback_is_not_closed" \
    "Theorem expression_usage_owner_plus_ast_fallback_is_not_closed" \
    "Theorem type_usage_owner_plus_ast_fallback_is_not_closed" \
    "Theorem kind_usage_owner_plus_ast_fallback_is_not_closed" \
    "Theorem entrypoint_owner_plus_ast_fallback_is_not_closed" \
    "Theorem duplicate_semantic_producer_is_not_closed" \
    "Theorem missing_required_fact_is_not_closed"; do
    require_text "$PROOF" "$term"
done

require_text "$PROOF" "FInitializerArrayBody"
require_text "$PROOF" "FInitializerTryOperand"
require_text "$PROOF" "FCollectionMutationParts"
require_text "$PROOF" "FEnumDeclarationRows"
require_text "$PROOF" "FNominalDeclarationRows"
require_text "$PROOF" "FRoleDeclarationRows"
require_text "$PROOF" "FExpressionRuntimeUsageSurface"
require_text "$PROOF" "FTypeRuntimeUsageSurface"
require_text "$PROOF" "FKindRuntimeUsageSurface"
require_text "$PROOF" "FEntrypointSelection"
require_text "$PROOF" "OSemanticLocalBindingFacts"
require_text "$PROOF" "OSemanticStatementFacts"
require_text "$PROOF" "OSemanticEnumFacts"
require_text "$PROOF" "OSemanticNominalConstructorFacts"
require_text "$PROOF" "OSemanticRoleFacts"
require_text "$PROOF" "OSemanticExpressionSurfaceFacts"
require_text "$PROOF" "OSemanticTypeSurfaceFacts"
require_text "$PROOF" "OSemanticKindSurfaceFacts"
require_text "$PROOF" "OSemanticSignatureFacts"
require_text "$PROOF" "CArrayLiteralEmitter"
require_text "$PROOF" "CTryLetEmitter"
require_text "$PROOF" "CCollectionMutationEmitter"
require_text "$PROOF" "CEnumEmitter"
require_text "$PROOF" "CNominalEmitter"
require_text "$PROOF" "CRoleOperatorEmitter"
require_text "$PROOF" "CRuntimeUsageProjection"
require_text "$PROOF" "CProgramEntrypointProjection"
require_text "$PROOF" "OCodegenTextRecovery"

check_owner_copy "$ROOT_DIR/$OWNER" ||
    fail "live semantic owner does not provide the modeled array body fact"
check_statement_owner_copy "$ROOT_DIR/$STATEMENT_OWNER" ||
    fail "live semantic statement owner does not provide collection facts"
check_enum_owner_copy "$ROOT_DIR/$ENUM_OWNER" ||
    fail "live semantic enum owner does not provide declaration rows"
check_nominal_owner_copy "$ROOT_DIR/$NOMINAL_OWNER" ||
    fail "live semantic nominal owner does not provide declaration rows"
check_role_owner_copy "$ROOT_DIR/$ROLE_OWNER" ||
    fail "live semantic role owner does not provide declaration rows"
check_expression_surface_owner_copy "$ROOT_DIR/$EXPRESSION_SURFACE_OWNER" ||
    fail "live semantic expression-surface owner does not provide usage rows"
check_type_surface_owner_copy "$ROOT_DIR/$TYPE_SURFACE_OWNER" ||
    fail "live semantic type-surface owner does not provide usage rows"
check_kind_surface_owner_copy "$ROOT_DIR/$KIND_SURFACE_OWNER" ||
    fail "live semantic kind-surface owner does not provide usage rows"
check_signature_owner_copy "$ROOT_DIR/$SIGNATURE_OWNER" ||
    fail "live semantic signature owner does not provide entrypoint rows"
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
check_consumer_copy "$ROOT_DIR/$ENUM_CONSUMER" \
    "CodegenSemanticEnumNameAtOrDie(" ||
    fail "live enum codegen consumer reopened text recovery"
require_text "$ENUM_CONSUMER" "CodegenSemanticEnumVariantNameAtOrDie("
require_text "$ENUM_EMITTER" "while i < SemanticAstEnumCount(facts)"
require_text "$ENUM_EMITTER" "CodegenSemanticEnumVariantNameAtOrDie("
check_consumer_copy "$ROOT_DIR/$NOMINAL_CONSUMER" \
    "CodegenSemanticNominalNameOrDie(" ||
    fail "live nominal codegen consumer reopened arena recovery"
require_text "$NOMINAL_CONSUMER" "CodegenSemanticNominalFieldNameOrDie("
require_text "$NOMINAL_CONSUMER" "CodegenSemanticNominalFieldTypeOrDie("
require_text "$ENUM_EMITTER" "SemanticAstNominalConstructorCount(facts)"
check_consumer_copy "$ROOT_DIR/$ROLE_CONSUMER" \
    "CodegenSemanticRoleNameOrDie(" ||
    fail "live role codegen consumer reopened arena recovery"
require_text "$ROLE_CONSUMER" "CodegenSemanticRoleMethodNodeOrDie("
require_text "$ENUM_EMITTER" "SemanticAstRoleCount(roles)"
check_consumer_copy "$ROOT_DIR/$EXPRESSION_USAGE_CONSUMER" \
    "CodegenExpressionUsageFactsFromSemantic(" ||
    fail "runtime usage projection reopened AST expression recovery"
check_consumer_copy "$ROOT_DIR/$TYPE_USAGE_CONSUMER" \
    "CodegenTypeUsageFactsFromSemantic(" ||
    fail "runtime usage projection reopened AST type recovery"
check_consumer_copy "$ROOT_DIR/$KIND_USAGE_CONSUMER" \
    "CodegenKindUsageFactsFromSemantic(" ||
    fail "runtime usage projection reopened AST kind recovery"
require_text "$ENTRYPOINT_VERDICT_CONSUMER" \
    "SemanticAstFunctionNameAt(signatures, i)"
reject_text "$ENTRYPOINT_VERDICT_CONSUMER" \
    "func SemanticAstArtifactIsMainFunction"
require_text "$ENTRYPOINT_PROJECTION_CONSUMER" \
    "func CodegenSemanticSelectedFunctionNode("
require_text "$PROGRAM_EMITTER" "CodegenSemanticSelectedFunctionNode("
reject_text "$PROGRAM_EMITTER" "CodegenAstArenaIsMainFunction"

for consumer in "$ARRAY_CONSUMER" "$TRY_CONSUMER" "$COLLECTION_CONSUMER" "$ENUM_CONSUMER"; do
    reject_text "$consumer" "StringTrim("
    reject_text "$consumer" "CharAt("
    reject_text "$consumer" "TypedAstArenaAtomText"
    reject_text "$consumer" "TypedAstArenaValueText"
    reject_text "$consumer" "TypedAstArenaAuxValueText"
    reject_text "$consumer" "CodegenAstArenaValueOrDie"
    reject_text "$consumer" "ContainsOutsideStrings("
    reject_text "$consumer" "FindMatchingParen("
done
for fallback in \
    "TypedAstArenaTypeName" \
    "CodegenAstArenaTypeFactPresent" \
    "CodegenTypeUsageFactsFromArena"; do
    reject_text "$TYPE_USAGE_CONSUMER" "$fallback"
done
for fallback in \
    "TypedAstArenaAtomText" \
    "TypedAstArenaValueText" \
    "TypedAstArenaAuxValueText" \
    "ContainsCallOutsideStrings" \
    "CodegenExpressionUsageFactsFromArena" \
    "CodegenAstArenaExpressionPartsAt"; do
    reject_text "$EXPRESSION_USAGE_CONSUMER" "$fallback"
done
for fallback in \
    "TypedAstArenaNodeKindIs" \
    "CodegenAstArenaKindPresent" \
    "CodegenKindUsageFactsFromArena" \
    "CodegenAstKindArrayLiteral"; do
    reject_text "$KIND_USAGE_CONSUMER" "$fallback"
done
for fallback in \
    "SemanticAstArtifactIsMainFunction" \
    "CodegenAstArenaIsMainFunction"; do
    reject_text "$ENTRYPOINT_VERDICT_CONSUMER" "$fallback"
    reject_text "$PROGRAM_EMITTER" "$fallback"
done
for fallback in \
    "CodegenAstArenaRoleNameOrDie" \
    "CodegenAstArenaRoleTargetTypeNameOrDie" \
    "CodegenAstArenaIsRoleDecl(arena, i)" \
    "CodegenAstArenaIsDescendantOf(arena, j, i)"; do
    reject_text "$ENUM_EMITTER" "$fallback"
done
for fallback in \
    "CodegenAstArenaNominalNameOrDie" \
    "CodegenAstArenaFieldNameOrDie" \
    "CodegenAstArenaFieldTypeNameOrDie" \
    "CodegenAstArenaIsNominalDecl(arena, i)" \
    "CodegenAstArenaIsFieldsHeader(arena, j)"; do
    reject_text "$ENUM_EMITTER" "$fallback"
done
reject_text "$ENUM_CONSUMER" "ExprSequenceItemCount"
reject_text "$ENUM_CONSUMER" "ExprSequenceItemAt"
reject_text "$ENUM_CONSUMER" "facts.variant_names"
reject_text "$ENUM_CONSUMER" "facts.variant_param_counts"
for fallback in \
    "TypedAstArenaAuxValueText" \
    "ExprSequenceItemCount" \
    "ExprSequenceItemAt" \
    "CodegenAstArenaEnumNameOrDie" \
    "CodegenAstArenaEnumVariantCount" \
    "CodegenAstArenaEnumVariantNameAt"; do
    reject_text "$ENUM_EMITTER" "$fallback"
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

cp "$ROOT_DIR/$ENUM_OWNER" "$tmp_dir/enum_owner_missing.pgy"
sed 's/^    variant_names: Array<String>;/    removed_variant_names: Array<String>;/' \
    "$tmp_dir/enum_owner_missing.pgy" >"$tmp_dir/enum_owner_missing.next"
mv "$tmp_dir/enum_owner_missing.next" "$tmp_dir/enum_owner_missing.pgy"
if check_enum_owner_copy "$tmp_dir/enum_owner_missing.pgy"; then
    fail "missing enum-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$NOMINAL_OWNER" "$tmp_dir/nominal_owner_missing.pgy"
sed 's/^    field_names: Array<String>;/    removed_field_names: Array<String>;/' \
    "$tmp_dir/nominal_owner_missing.pgy" >"$tmp_dir/nominal_owner_missing.next"
mv "$tmp_dir/nominal_owner_missing.next" "$tmp_dir/nominal_owner_missing.pgy"
if check_nominal_owner_copy "$tmp_dir/nominal_owner_missing.pgy"; then
    fail "missing nominal-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$ROLE_OWNER" "$tmp_dir/role_owner_missing.pgy"
sed 's/^    method_node_ids: Array<Int>;/    removed_method_node_ids: Array<Int>;/' \
    "$tmp_dir/role_owner_missing.pgy" >"$tmp_dir/role_owner_missing.next"
mv "$tmp_dir/role_owner_missing.next" "$tmp_dir/role_owner_missing.pgy"
if check_role_owner_copy "$tmp_dir/role_owner_missing.pgy"; then
    fail "missing role-owner mutation was not rejected"
fi

cp "$ROOT_DIR/$EXPRESSION_SURFACE_OWNER" "$tmp_dir/expression_surface_owner_missing.pgy"
sed 's/^    auxiliary_values: Array<String>;/    removed_auxiliary_values: Array<String>;/' \
    "$tmp_dir/expression_surface_owner_missing.pgy" >"$tmp_dir/expression_surface_owner_missing.next"
mv "$tmp_dir/expression_surface_owner_missing.next" "$tmp_dir/expression_surface_owner_missing.pgy"
if check_expression_surface_owner_copy "$tmp_dir/expression_surface_owner_missing.pgy"; then
    fail "missing expression-surface owner mutation was not rejected"
fi

cp "$ROOT_DIR/$TYPE_SURFACE_OWNER" "$tmp_dir/type_surface_owner_missing.pgy"
sed 's/^    type_names: Array<String>;/    removed_type_names: Array<String>;/' \
    "$tmp_dir/type_surface_owner_missing.pgy" >"$tmp_dir/type_surface_owner_missing.next"
mv "$tmp_dir/type_surface_owner_missing.next" "$tmp_dir/type_surface_owner_missing.pgy"
if check_type_surface_owner_copy "$tmp_dir/type_surface_owner_missing.pgy"; then
    fail "missing type-surface owner mutation was not rejected"
fi

cp "$ROOT_DIR/$KIND_SURFACE_OWNER" "$tmp_dir/kind_surface_owner_missing.pgy"
sed 's/^    kind_tags: Array<Int>;/    removed_kind_tags: Array<Int>;/' \
    "$tmp_dir/kind_surface_owner_missing.pgy" >"$tmp_dir/kind_surface_owner_missing.next"
mv "$tmp_dir/kind_surface_owner_missing.next" "$tmp_dir/kind_surface_owner_missing.pgy"
if check_kind_surface_owner_copy "$tmp_dir/kind_surface_owner_missing.pgy"; then
    fail "missing kind-surface owner mutation was not rejected"
fi

cp "$ROOT_DIR/$SIGNATURE_OWNER" "$tmp_dir/signature_owner_missing.pgy"
sed 's/^    names: Array<String>;/    removed_names: Array<String>;/' \
    "$tmp_dir/signature_owner_missing.pgy" >"$tmp_dir/signature_owner_missing.next"
mv "$tmp_dir/signature_owner_missing.next" "$tmp_dir/signature_owner_missing.pgy"
if check_signature_owner_copy "$tmp_dir/signature_owner_missing.pgy"; then
    fail "missing signature-owner mutation was not rejected"
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


cp "$ROOT_DIR/$ENUM_CONSUMER" "$tmp_dir/enum_consumer_fallback.pgy"
printf '\nfunc ReintroducedEnumFallback(x: String) -> String { return StringTrim(x); }\n' \
    >>"$tmp_dir/enum_consumer_fallback.pgy"
if check_consumer_copy "$tmp_dir/enum_consumer_fallback.pgy" \
    "CodegenSemanticEnumNameAtOrDie("; then
    fail "enum fallback mutation was not rejected"
fi

cp "$ROOT_DIR/$NOMINAL_CONSUMER" "$tmp_dir/nominal_consumer_fallback.pgy"
printf '\nfunc ReintroducedNominalFallback(x: String) -> String { return StringTrim(x); }\n' \
    >>"$tmp_dir/nominal_consumer_fallback.pgy"
if check_consumer_copy "$tmp_dir/nominal_consumer_fallback.pgy" \
    "CodegenSemanticNominalNameOrDie("; then
    fail "nominal fallback mutation was not rejected"
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
