/* Private implementation seams of the single parameter publication owner. */
#ifndef FUNCTION_PARAM_PUBLICATION_INTERNAL_H
#define FUNCTION_PARAM_PUBLICATION_INTERNAL_H

#include "function_param_publication_owner.h"

bool publication_generation_ready(const SemanticContext *ctx);
bool publication_physical_scalar(const Type *type);
PublishedNode *publication_node(FunctionParamPublicationStore *store,
    const ASTNode *node, bool insert);
PublishedNode *publication_node_by_id(FunctionParamPublicationStore *store, uint32_t id);
bool publication_call_ready(const PublishedNode *row);
PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id);
PublishedNode *publication_checked_intrinsic(const SemanticContext *ctx,
    const ASTNode *call, uint32_t kind, const Type *type, size_t count);
bool publication_record_read_payload_loan(SemanticContext *ctx, ASTNode *call);
bool publication_record_private_array_push(SemanticContext *ctx, ASTNode *call,
    PublishedNode *row);
bool publication_routine_header_current(const SemanticContext *ctx, const ASTNode *decl);
PublishedNode *publication_result_current(const SemanticContext *ctx,
    const ASTNode *call);
PublishedNode *publication_current_value(const SemanticContext *ctx,
    const ASTNode *value);
bool publication_ref_call_tuple(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *site, const ASTNode *callee, size_t ordinal, size_t *formal);
bool semantic_param_publication_ref_call_diagnostic_current(
    const SemanticContext *ctx, const PendingRefCall *row, size_t *formal);
void semantic_published_unit_enum_selection_destroy(
    SemanticPublishedUnitEnumSelection *selection);

#endif
