#ifndef PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#define PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#include "collection_ownership_fact.h"

/* Components share one collection fact store; these are module-private seams. */
uint32_t
current_function_syntax_id(const SemanticContext *ctx);

uint32_t
receiver_binding_syntax_id(ASTNode *receiver, const Symbol *binding);

PgyCollectionOwnershipFact *
receiver_collection_fact(ASTNode *receiver, SemanticContext *ctx,
                         Symbol **binding_out);

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name);

bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out);
#endif
