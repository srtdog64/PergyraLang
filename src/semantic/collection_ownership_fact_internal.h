#ifndef PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#define PGY_COLLECTION_OWNERSHIP_FACT_INTERNAL_H
#include "collection_ownership_fact.h"
#include "../parser/ast_types.h"

#define PGY_COLLECTION_MEMBER_PATH_LIMIT 64u

typedef struct {
    const ASTNode *root;
    uint32_t root_binding_id;
    const Type *root_type;
    const Type *leaf_receiver_type;
    const Type *result_type;
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

bool collection_private_push_recorded(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *receiver, uint32_t binding_id);

bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out);

bool collection_member_move_path_identity(const ASTNode *member_access,
    SemanticContext *ctx, PgyCollectionMemberPath *out);

/* Replays one exact typed projection from the sealed publication owner. It
 * grants no ownership and accepts any result type so carrier prefixes can be
 * composed before a specialized consumer checks its leaf type. */
bool collection_published_typed_member_path_current(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, PgyCollectionMemberPath *path_out);

/* Resolves a direct member receiver rooted at one current default/inout
 * aggregate formal.  The exact typed projection and leaf declaration identity
 * come from the existing publication/path owners; a local, mutable, consumed,
 * or ambiguously declared root grants nothing. */
bool collection_current_parameter_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out);
bool collection_published_parameter_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out);

/* Resolves the same exact typed projection for an immutable local aggregate
 * root. The pending query uses the live binding only to reject a mutable,
 * consumed or host root; the published query replays the sealed declaration
 * and projection identities after lexical scopes have retired. */
bool collection_current_local_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    PgyCollectionMemberPath *path_out);
bool collection_published_local_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, PgyCollectionMemberPath *path_out);

/* Resolves an exact moved member through immutable local aggregate carriers
 * to one current default/inout formal.  The returned path is formal-relative;
 * missing typed publication, mutable/rebound carriers and cycles grant
 * nothing. */
bool collection_member_move_current_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out);
/* Resolves the same current moved-member path while admitting a new call
 * entry. Earlier deferred call receipts may exist; their preservation is
 * checked by the array-storage finalizer before this provisional path can
 * authorize cleanup or mutation. */
bool collection_member_move_call_entry_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out);
/* Finalized counterpart for immutable local aggregate carriers. The path
 * keeps the exact immediate extraction root, while its field sequence is
 * extended to the one default/inout formal proved by sealed publication. */
bool collection_published_member_move_parameter_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out);
bool collection_member_move_pending_call_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, const ASTNode *call, const ASTNode *callee_decl,
    size_t argument_ordinal, size_t target_formal_ordinal,
    size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out,
    size_t *pending_sequence_out);

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
