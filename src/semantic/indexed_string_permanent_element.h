/* Exact permanent-literal inputs of the indexed String source family.
 * Neither this recipe nor a derived zero source grants element cleanup. */
#ifndef PGY_INDEXED_STRING_PERMANENT_ELEMENT_H
#define PGY_INDEXED_STRING_PERMANENT_ELEMENT_H
#include "indexed_string_result_inputs.h"

typedef struct IndexedStringPermanentElementRecipe IndexedStringPermanentElementRecipe;
typedef enum {
    INDEXED_STRING_PERMANENT_NOT_APPLICABLE,
    INDEXED_STRING_PERMANENT_RECORDED,
    INDEXED_STRING_PERMANENT_FAILED
} IndexedStringPermanentRecord;

IndexedStringPermanentRecord semantic_indexed_string_permanent_record(SemanticContext *ctx,
    ASTNode *value, ASTNode *some, size_t ordinal);
bool semantic_indexed_string_permanent_some_boundary(const ASTNode *some);
bool semantic_indexed_string_permanent_receipts(SemanticContext *ctx, bool prepare);
bool semantic_indexed_string_permanent_current(SemanticContext *ctx,
    const IndexedStringPermanentElementRecipe *recipe);
bool semantic_indexed_string_permanent_syntax_current(const IndexedStringPermanentElementRecipe *recipe);
const IndexedStringFieldDependency *semantic_indexed_string_permanent_dependencies(
    const IndexedStringPermanentElementRecipe *recipe, size_t *count);
const IndexedStringResultInputs *semantic_indexed_string_permanent_inputs(
    const IndexedStringPermanentElementRecipe *recipe);
void semantic_indexed_string_permanent_destroy(IndexedStringPermanentElementRecipe *recipe);
/* Only the exact Some payload slot may consume this derived lifetime proof. */
const IndexedStringPermanentElementRecipe *semantic_indexed_string_permanent_for_use(
    SemanticContext *ctx, const ASTNode *some, size_t ordinal, const ASTNode *value);
bool semantic_indexed_string_permanent_source_write_count(const IndexedStringPermanentElementRecipe *recipe, size_t *count);
bool semantic_indexed_string_permanent_source_root(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value);
bool semantic_indexed_string_permanent_source_use(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *sink, size_t ordinal, const ASTNode *value);
bool semantic_indexed_string_permanent_source_value(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value);
bool semantic_indexed_string_permanent_source_evaluation(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value);
bool semantic_indexed_string_permanent_source_definition(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, uint32_t binding, bool initialization, const ASTNode *statement,
    const ASTNode *place, const ASTNode *value, size_t *write_ordinal);
bool semantic_indexed_string_permanent_source_return(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *statement, const ASTNode *value);
#endif
