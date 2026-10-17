/* Definition-wide String source independence in the existing indexed owner.
 * A candidate is not permission; only a sealed selected recipe may yield zero. */
#ifndef PGY_INDEXED_STRING_DEFINITION_RECIPE_H
#define PGY_INDEXED_STRING_DEFINITION_RECIPE_H

#include "indexed_string_result_inputs.h"

typedef enum {
    INDEXED_STRING_DEFINITION_NOT_APPLICABLE,
    INDEXED_STRING_DEFINITION_RECORDED,
    INDEXED_STRING_DEFINITION_FAILED
} IndexedStringDefinitionRecord;

typedef struct {
    const ASTNode *value, *named_call;
    uint32_t value_id, named_call_id, binding_id;
    ASTNodeType shape;
    const ASTNode *alias_declaration, *alias_initializer;
    uint32_t alias_declaration_id, alias_initializer_id;
    IndexedStringFieldCall independent_call;
    bool independent_call_present;
    IndexedStringResultInputs named_inputs;
} IndexedStringDefinitionSource;

typedef struct {
    const ASTNode *assignment, *target;
    uint32_t assignment_id, target_id, binding_id;
    IndexedStringDefinitionSource rhs;
} IndexedStringDefinitionAssignment;

typedef struct {
    const ASTNode *value, *sink;
    uint32_t value_id, sink_id, binding_id, issued_source;
    ASTNodeType value_shape;
    size_t actual_ordinal;
    bool protected_candidate, protected_use;
} IndexedStringDefinitionUse;

typedef struct IndexedStringDefinitionRecipe {
    const ASTNode *declaration, *caller, *body;
    uint32_t declaration_id, caller_id, body_id, binding_id;
    char *binding_name;
    IndexedStringDefinitionSource birth;
    IndexedStringDefinitionAssignment *assignments;
    size_t assignment_count, assignment_capacity, sealed_assignments;
    IndexedStringDefinitionUse *uses;
    size_t use_count, use_capacity, sealed_uses;
    IndexedStringFieldCall *use_calls;
    size_t use_call_count, use_call_capacity, sealed_use_calls;
    FunctionParamFlowIndexClaims *claims; /* original owner, borrowed until MOVE */
    bool selected, prepared, validating_current;
} IndexedStringDefinitionRecipe;

IndexedStringDefinitionRecord semantic_indexed_string_definition_record_birth(
    SemanticContext *ctx, const Symbol *binding, const ASTNode *declaration,
    const ASTNode *initializer);
IndexedStringDefinitionRecord semantic_indexed_string_definition_record_assignment(
    SemanticContext *ctx, const Symbol *binding, const ASTNode *assignment,
    const ASTNode *rhs);
IndexedStringDefinitionRecord semantic_indexed_string_definition_record_use(
    SemanticContext *ctx, const ASTNode *value, const ASTNode *sink,
    size_t actual_ordinal);
bool semantic_indexed_string_definition_protect_use(SemanticContext *ctx,
    const ASTNode *value, const ASTNode *sink, size_t actual_ordinal,
    bool require_assignment);
bool semantic_indexed_string_definition_selected(SemanticContext *ctx,
    const ASTNode *value);
bool semantic_indexed_string_definition_source_current(SemanticContext *ctx,
    const ASTNode *value, uint32_t *source);
bool indexed_string_definition_claims_roles(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, bool prepare);
bool indexed_string_definition_classify_uses(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, bool *protected_use);
bool semantic_indexed_string_definition_receipts(SemanticContext *ctx, bool prepare);
bool semantic_indexed_string_definition_syntax_current(
    const IndexedStringDefinitionRecipe *recipe);
void semantic_indexed_string_definition_destroy(IndexedStringDefinitionRecipe *recipe);

#endif
