/* Native immutable inputs of the existing named String-result derivation.
 * Claims are borrowed until the collection snapshot moves them once. */
#ifndef PGY_INDEXED_STRING_RESULT_INPUTS_H
#define PGY_INDEXED_STRING_RESULT_INPUTS_H
#include "indexed_string_field_lineage_internal.h"

typedef struct IndexedStringOptionInputs IndexedStringOptionInputs;
typedef struct IndexedStringPermanentElementRecipe IndexedStringPermanentElementRecipe;
typedef struct IndexedStringResultInputs {
    IndexedStringFieldCall *calls;
    IndexedStringFieldDependency *dependencies;
    size_t call_count, call_capacity, sealed_calls;
    size_t dependency_count, dependency_capacity, sealed_dependencies;
    bool prepared;
    IndexedStringOptionInputs *option;
    const IndexedStringPermanentElementRecipe **permanents;
    size_t permanent_count, permanent_capacity, sealed_permanents;
} IndexedStringResultInputs;

/* Prepare accepts a zero object and may leave partial storage on failure;
 * destroy it before retry. Current never appends or demands. NULL inputs
 * preserves the existing b53 API. An issued source is not a cleanup right.
 * The consumer must separately join the top call/actual/identifier lineage
 * to its original caller claims. Option/permanent derivations additionally
 * include their exact local carrier caller in the same dependency union. */
bool semantic_indexed_string_result_source_prepare(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs);
bool semantic_indexed_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs);
bool semantic_indexed_string_result_inputs_syntax_current(const IndexedStringResultInputs *inputs);
void semantic_indexed_string_result_inputs_destroy(IndexedStringResultInputs *inputs);
const IndexedStringFieldDependency *semantic_indexed_string_result_inputs_dependencies(
    const IndexedStringResultInputs *inputs, size_t *count);
bool semantic_indexed_string_result_inputs_permanent_count(const IndexedStringResultInputs *inputs, size_t *count);
const IndexedStringPermanentElementRecipe *semantic_indexed_string_result_inputs_permanent_at(
    const IndexedStringResultInputs *inputs, size_t ordinal);

/* Candidate only: an issued checked Option ABI is not a String source proof. */
bool semantic_indexed_string_option_unwrap_candidate(SemanticContext *ctx, const ASTNode *call);
bool semantic_indexed_string_result_inputs_has_option(const IndexedStringResultInputs *inputs);
bool semantic_indexed_string_result_inputs_source_write_count(const IndexedStringResultInputs *inputs, size_t *count);
bool semantic_indexed_string_result_inputs_source_definition(const IndexedStringResultInputs *inputs,
    uint32_t caller, uint32_t binding, bool initialization, const ASTNode *statement,
    const ASTNode *place, const ASTNode *value, size_t *write_ordinal);
bool semantic_indexed_string_result_inputs_source_use(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *sink, size_t ordinal, const ASTNode *value);
bool semantic_indexed_string_result_inputs_source_value(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *value);
bool semantic_indexed_string_result_inputs_source_evaluation(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *value);
bool semantic_indexed_string_result_inputs_source_return(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *statement, const ASTNode *value);

bool indexed_string_field_call_capture(SemanticContext *ctx, ASTNode *call,
    IndexedStringFieldCall *out);
bool indexed_string_field_call_syntax(const IndexedStringFieldCall *row);
bool indexed_string_field_call_current(SemanticContext *ctx, const IndexedStringFieldCall *row);
void indexed_string_field_call_destroy(IndexedStringFieldCall *row);
bool indexed_string_result_capture_dependency(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowIndexClaims *claims, IndexedStringResultInputs *inputs, bool prepare);
bool indexed_string_result_capture_call(SemanticContext *ctx, const ASTNode *call,
    IndexedStringResultInputs *inputs, bool prepare);
bool indexed_string_result_capture_permanent(SemanticContext *ctx,
    const IndexedStringPermanentElementRecipe *recipe, IndexedStringResultInputs *inputs, bool prepare);
#endif
