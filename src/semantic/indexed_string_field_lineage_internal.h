/* Joined recipes in the existing indexed-String source family. They classify
 * only Array-element lifetime independence; no cleanup authority is carried. */
#ifndef PGY_INDEXED_STRING_FIELD_LINEAGE_INTERNAL_H
#define PGY_INDEXED_STRING_FIELD_LINEAGE_INTERNAL_H
#include "function_param_flow/function_param_flow_summary_internal.h"

typedef struct {
    const ASTNode *decl, *body;
    uint32_t decl_id, body_id;
    FunctionParamFlowIndexClaims *claims; /* borrowed until the snapshot take */
    size_t no_store_call_count;
} IndexedStringFieldDependency;

typedef struct {
    const ASTNode *statement, *constructor, *source, *input;
    uint32_t statement_id, constructor_id, source_id, input_id;
    size_t constructor_actual, formal_ordinal;
    uint32_t formal_id;
    SemanticNominalResultSnapshot *mapping; /* original owner payload */
} IndexedStringFieldReturn;

typedef struct {
    const ASTNode *actual, *type_node;
    uint32_t actual_id, formal_id, type_id;
    size_t formal_ordinal;
    ParamMode mode;
    char *name, *type_syntax, *argument_name;
} IndexedStringFieldFormal;

typedef struct {
    const ASTNode *call, *target, *callee, *body, *result_type;
    uint32_t call_id, target_id, target_binding_id, callee_id, body_id, result_type_id;
    size_t actual_count, formal_count;
    ASTNodeType target_shape;
    bool builtin, stdlib, declared_callable;
    uint32_t builtin_kind;
    char *callee_name, *result_syntax;
    IndexedStringFieldFormal *formals;
} IndexedStringFieldCall;

typedef struct IndexedStringFieldRecipe {
    const ASTNode *value, *sink, *caller, *root, *definition, *producer, *producer_decl;
    uint32_t value_id, sink_id, caller_id, root_id, binding_id;
    uint32_t definition_id, producer_id, producer_decl_id;
    size_t sink_ordinal;
    PublishedProjection projection; /* original strings move only at take */
    IndexedStringFieldDependency *dependencies;
    size_t dependency_count, dependency_capacity, sealed_dependencies;
    IndexedStringFieldReturn *returns;
    size_t return_count, return_capacity, sealed_returns;
    IndexedStringFieldCall *calls;
    size_t call_count, call_capacity, sealed_calls;
    bool issued;
} IndexedStringFieldRecipe;

FunctionParamFlowProgramPointIndex *indexed_string_field_flow_index(SemanticContext *ctx,
    ASTNode *decl);
bool indexed_string_field_flow_dependency(IndexedStringFieldRecipe *recipe,
    const FunctionParamFlowProgramPointIndex *index);
bool indexed_string_field_flow_uses_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, uint32_t binding,
    bool aggregate, IndexedStringFieldRecipe *recipe, bool prepare);
bool indexed_string_field_flow_call_current(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, ASTNode *call, bool prepare);
bool indexed_string_field_flow_closure_current(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, ASTNode *decl, bool prepare);
bool indexed_string_field_flow_syntax_current(const IndexedStringFieldRecipe *recipe);
void indexed_string_field_flow_destroy(IndexedStringFieldRecipe *recipe);

bool semantic_indexed_string_field_record(SemanticContext *ctx, ASTNode *value,
    ASTNode *sink, size_t ordinal);
bool semantic_indexed_string_field_prepare(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe);
bool semantic_indexed_string_field_current(SemanticContext *ctx,
    const IndexedStringFieldRecipe *recipe);
bool semantic_indexed_string_field_syntax_current(const IndexedStringFieldRecipe *recipe);
void semantic_indexed_string_field_destroy(IndexedStringFieldRecipe *recipe);
bool semantic_indexed_string_field_receipts(SemanticContext *ctx, bool prepare);

#endif
