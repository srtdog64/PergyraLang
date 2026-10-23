#ifndef PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#define PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#include "collection_ownership_fact.h"

#define PGY_COLLECTION_MEMBER_PATH_LIMIT 64u

typedef struct {
    const ASTNode *root;
    uint32_t root_binding_id;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t field_count;
} PgyCollectionMemberPath;

/* Components share one collection fact store; these are module-private seams. */
uint32_t
current_function_syntax_id(const SemanticContext *ctx);

bool collection_array_string_type(const Type *type);

uint32_t
receiver_binding_syntax_id(ASTNode *receiver, const Symbol *binding);

PgyCollectionOwnershipFact *
receiver_collection_fact(ASTNode *receiver, SemanticContext *ctx,
                         Symbol **binding_out);

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name);

bool collection_private_push_recorded(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *receiver, uint32_t binding_id);

bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out);

bool collection_member_move_path_identity(const ASTNode *member_access,
    SemanticContext *ctx, PgyCollectionMemberPath *out);

bool collection_member_move_path_matches_fact(
    const PgyCollectionOwnershipFact *fact, const ASTNode *member_access,
    SemanticContext *ctx, bool overlap);

bool collection_member_move_path_matches_source(
    const ASTNode *source, uint32_t source_id, uint32_t root_binding_id,
    const ASTNode *member_access, SemanticContext *ctx, bool overlap);

bool
collection_member_move_from_current_inout_parameter(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx);

bool
collection_member_move_from_current_inout_parameter_at(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx,
    size_t *formal_out);

bool collection_member_move_from_current_default_parameter(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out);

bool
collection_member_move_release_path_current(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    const ASTNode *receiver,
    SemanticContext *ctx);
#endif
