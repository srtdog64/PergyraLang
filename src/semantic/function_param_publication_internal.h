/* Private implementation seams of the single parameter publication owner. */
#ifndef FUNCTION_PARAM_PUBLICATION_INTERNAL_H
#define FUNCTION_PARAM_PUBLICATION_INTERNAL_H

#include "function_param_publication_owner.h"

bool publication_generation_ready(const SemanticContext *ctx);
bool publication_physical_scalar(const Type *type);
bool publication_array_result_borrowed_formal(Type *function_type,
    const Type *formal_type, ParamMode mode, SemanticContext *ctx);
PublishedNode *publication_node(FunctionParamPublicationStore *store,
    const ASTNode *node, bool insert);
PublishedNode *publication_node_by_id(FunctionParamPublicationStore *store,
    uint32_t id);
bool semantic_param_publication_pending_projection_formal_root(SemanticContext *ctx,
    ASTNode *caller, const ASTNode *value, const ASTNode **root, size_t *formal);
bool publication_call_ready(const SemanticContext *ctx,
    const PublishedNode *row);
PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id);
PublishedNode *publication_checked_intrinsic(const SemanticContext *ctx,
    const ASTNode *call, uint32_t kind, const Type *type, size_t count);
bool publication_record_read_payload_loan(SemanticContext *ctx, ASTNode *call);
bool publication_record_private_array_push(SemanticContext *ctx, ASTNode *call,
    PublishedNode *row);
bool publication_record_private_physical_array_mutation(SemanticContext *ctx,
    ASTNode *call, PublishedNode *row);
bool publication_routine_header_current(const SemanticContext *ctx, const ASTNode *decl);
PublishedNode *publication_result_current(const SemanticContext *ctx,
    const ASTNode *call);
/* `row` is a current ArraySort or ArrayReverse call result. */
bool publication_array_permutation(const PublishedNode *row);
PublishedNode *publication_current_value(const SemanticContext *ctx,
    const ASTNode *value);
/* A whole-routine ownership proof may close an exact nominal result field
 * after the inline path-insensitive pass captured it conservatively. This
 * owner validates the immutable call/field/relation and transferred-element
 * identities before changing only that snapshot's String-element premise. */
bool publication_promote_exact_nominal_string_array_transfer(
    SemanticContext *ctx, ASTNode *call, size_t actual,
    uint32_t function_id, uint32_t binding_id);
bool publication_ref_call_tuple(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *site, const ASTNode *callee, size_t ordinal, size_t *formal);
bool publication_default_borrow_call_tuple(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *site, const ASTNode *callee,
    size_t ordinal, size_t *formal);
bool semantic_param_publication_ref_call_diagnostic_current(
    const SemanticContext *ctx, const PendingRefCall *row, size_t *formal);
bool semantic_param_publication_default_borrow_call_diagnostic_current(
    const SemanticContext *ctx, const PendingBorrowedDefaultCall *row,
    size_t *formal);
void semantic_published_unit_enum_selection_destroy(
    SemanticPublishedUnitEnumSelection *selection);

#endif
