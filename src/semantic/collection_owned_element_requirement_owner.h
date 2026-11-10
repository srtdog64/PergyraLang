#ifndef PERGYRA_COLLECTION_OWNED_ELEMENT_REQUIREMENT_OWNER_H
#define PERGYRA_COLLECTION_OWNED_ELEMENT_REQUIREMENT_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include "../parser/ast.h"

typedef struct SemanticContext SemanticContext;
typedef struct Type Type;
typedef struct CollectionOwnedElementRequirementStore
    CollectionOwnedElementRequirementStore;

/* Bootstrap refusal ratchet, not an element-ownership permission source.
 * One analyzed program owns the store; no rows escape to HIR/MIR. */
bool semantic_collection_owned_element_requirements_begin(
    ASTNode *program, SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_deep_drop(
    ASTNode *call, ASTNode *receiver, const Type *array_type,
    SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_storage_drop(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx);
/* Resolve a direct Array<String> formal or an exact immutable local carrier
 * rooted at one formal. Missing typed publication, a mutable/rebound carrier,
 * or a cycle grants nothing. `parameter_carrier_out` distinguishes a local
 * value from a carrier whose exact proof failed, so mutation admission cannot
 * fall back to the private-local path. */
bool semantic_collection_owned_element_parameter_carrier_current(
    ASTNode *value, ASTNode *function_decl, SemanticContext *ctx,
    size_t *formal_index_out, ParamMode *formal_mode_out,
    bool *parameter_carrier_out);
/* An owned-element mutation through `own`, `inout`, or a moving default
 * Array<String> formal requires every concrete caller to provide current
 * uniformly owned elements. This grants no storage release or transfer
 * authority. */
bool semantic_collection_owned_element_requirement_record_parameter_mutation(
    ASTNode *call, ASTNode *receiver, ASTNode *function_decl,
    size_t formal_index, SemanticContext *ctx);
/* An inout formal may be selected for the retiring owned-set operation only
 * when every non-forwarding caller issued an owned-element receipt.  The
 * store finalizer replays those receipts after result facts are sealed;
 * missing or borrowed caller evidence grants no lowering permission. */
bool semantic_collection_owned_inout_actuals_issued(
    SemanticContext *ctx, ASTNode *function_decl, size_t formal_index);
/* Whether the current typed requirement store records a storage-release
 * obligation for this exact own formal.  Missing or stale rows grant
 * nothing; callers use this to keep provisional storage borrowed. */
bool semantic_collection_owned_parameter_releases_storage(
    SemanticContext *ctx, ASTNode *function_decl, size_t formal_index);
bool semantic_collection_owned_element_requirement_record_member_push(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx);
bool semantic_collection_owned_element_member_push_provisional(
    ASTNode *receiver, SemanticContext *ctx);
/* Revalidate one recorded shallow String-store candidate after parameter-flow
 * and aggregate-element requirements are sealed.  The pending member premise
 * may already have been withdrawn by the store itself; this query uses the
 * immutable path/provenance owners and grants no mutation effect. */
bool semantic_collection_owned_element_member_value_copy_candidate_current(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx,
    bool planned_replay);
/* Record one exact moved-member default/ref observation.  This receipt grants
 * no preservation by itself; it is checked only if a later member mutation
 * consumes the same owned-element premise. */
bool semantic_collection_owned_element_requirement_record_member_read(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index,
    ASTNode *argument, ParamMode parameter_mode, SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_argument(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index, size_t formal_index,
    ASTNode *argument, const Type *argument_type, ParamMode parameter_mode,
    SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_return(
    ASTNode *statement, ASTNode *value, const Type *value_type,
    SemanticContext *ctx);
bool semantic_collection_owned_element_requirements_finalize(
    SemanticContext *ctx);
void semantic_collection_owned_element_requirements_destroy(
    CollectionOwnedElementRequirementStore *store);

#endif
