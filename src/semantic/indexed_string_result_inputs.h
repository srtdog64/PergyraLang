/* Native immutable inputs of the existing named String-result derivation.
 * Claims are borrowed until the collection snapshot moves them once. */
#ifndef PGY_INDEXED_STRING_RESULT_INPUTS_H
#define PGY_INDEXED_STRING_RESULT_INPUTS_H
#include "indexed_string_field_lineage_internal.h"

typedef struct {
    IndexedStringFieldCall *calls;
    IndexedStringFieldDependency *dependencies;
    size_t call_count, call_capacity, sealed_calls;
    size_t dependency_count, dependency_capacity, sealed_dependencies;
    bool prepared;
} IndexedStringResultInputs;

/* Prepare accepts a zero object and may leave partial storage on failure;
 * destroy it before retry. Current never appends or demands. NULL inputs
 * preserves the existing b53 API. An issued source is not a cleanup right.
 * The consumer must separately join the top call/actual/identifier lineage
 * to its original caller claims; these inputs own only the reached callees. */
bool semantic_indexed_string_result_source_prepare(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs);
bool semantic_indexed_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs);
bool semantic_indexed_string_result_inputs_syntax_current(const IndexedStringResultInputs *inputs);
void semantic_indexed_string_result_inputs_destroy(IndexedStringResultInputs *inputs);
const IndexedStringFieldDependency *semantic_indexed_string_result_inputs_dependencies(
    const IndexedStringResultInputs *inputs, size_t *count);

bool indexed_string_field_call_capture(SemanticContext *ctx, ASTNode *call,
    IndexedStringFieldCall *out);
bool indexed_string_field_call_syntax(const IndexedStringFieldCall *row);
bool indexed_string_field_call_current(SemanticContext *ctx, const IndexedStringFieldCall *row);
void indexed_string_field_call_destroy(IndexedStringFieldCall *row);
#endif
