#ifndef PERGYRA_COLLECTION_OWNERSHIP_FACT_H
#define PERGYRA_COLLECTION_OWNERSHIP_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
typedef struct Type Type;
typedef struct PgyCollectionEffectSnapshot PgyCollectionEffectSnapshot;
typedef struct {
    const ASTNode *decl, *body;
    uint32_t decl_id, body_id;
    size_t independent_string_result_count;
    size_t independent_string_write_count;
    size_t independent_string_execution_count;
    size_t independent_string_write_execution_count;
} PgyCollectionEffectCaller;
typedef struct {
    const ASTNode *call;
    uint32_t id, kind, receiver, source;
    size_t caller;
} PgyCollectionExpectedEffect;
typedef struct {
    const PgyCollectionEffectCaller *callers;
    size_t caller_count;
    const PgyCollectionExpectedEffect *effects;
    size_t effect_count;
    size_t source_use_count;
    size_t nonowner_risk_count;
    size_t string_definition_count;
    size_t string_write_count;
    size_t permanent_element_count;
    size_t independent_string_result_count;
} PgyCollectionEffectView;
/* Moves existing caller claims after the last semantic consumer. No further
 * semantic queries may run after a successful take. This grants no permission. */
bool semantic_collection_effect_snapshot_take(SemanticContext *ctx,
    PgyCollectionEffectSnapshot **out);
bool semantic_collection_effect_snapshot_current(const PgyCollectionEffectSnapshot *snapshot,
    const ASTNode *program, PgyCollectionEffectView *optional_out);
/* MIR validates one routine at its boundary; the returned view still points
 * to the complete immutable ledger. A routine with no expected caller is valid. */
bool semantic_collection_effect_snapshot_routine_current(
    const PgyCollectionEffectSnapshot *snapshot, const ASTNode *program,
    uint32_t routine_id, PgyCollectionEffectView *optional_out);
/* The same synchronous validation must first pass snapshot_current or
 * snapshot_routine_current. This consumes one actual call's cached membership,
 * not another whole-body replay. */
bool semantic_collection_effect_snapshot_call(const PgyCollectionEffectSnapshot *snapshot,
    size_t caller_ordinal, const ASTNode *actual_call, size_t *effect_ordinal);
/* Pure selected-field evidence is reusable, not an affine effect. These
 * queries validate actual source identities; absence never grants source0. */
bool semantic_collection_effect_snapshot_source_root(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *root);
bool semantic_collection_effect_snapshot_source_use(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *sink, size_t actual_ordinal, const ASTNode *value);
bool semantic_collection_effect_snapshot_source_value(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *value);
bool semantic_collection_effect_snapshot_source_evaluation(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *value);
/* A reusable String result has one original static executable CALL occurrence.
 * Unrelated values return SIZE_MAX; callers consume an admitted ordinal once. */
bool semantic_collection_effect_snapshot_source_occurrence(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *value, const ASTNode *execution_root, size_t *ordinal);
/* After the synchronous snapshot boundary, executable visits must match the
 * original required static ReadFile CALL and direct-definition set exactly. */
bool semantic_collection_effect_snapshot_source_execution_complete(
    const PgyCollectionEffectSnapshot *snapshot, uint32_t caller_id,
    const bool *occurrences, size_t occurrence_count, const bool *writes, size_t write_count);
bool semantic_collection_effect_snapshot_source_definition(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, uint32_t binding_id, bool initialization,
    const ASTNode *statement, const ASTNode *place, const ASTNode *value,
    size_t *write_ordinal);
bool semantic_collection_effect_snapshot_source_return(const PgyCollectionEffectSnapshot *snapshot,
    uint32_t caller_id, const ASTNode *statement, const ASTNode *value);
PgyCollectionEffectSnapshot *semantic_collection_effect_snapshot_retain(
    PgyCollectionEffectSnapshot *snapshot);
void semantic_collection_effect_snapshot_destroy(PgyCollectionEffectSnapshot *snapshot);

/* Element-lifetime provenance for Array<String>.  This is deliberately
 * orthogonal to Slot and to the physical array descriptor: it records which
 * binding owns the pointed-to strings. */
typedef enum
{
    PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN = 0,
    PGY_STRING_ARRAY_BORROWED_ELEMENTS,
    PGY_STRING_ARRAY_OWNED_ELEMENTS,
    PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
} PgyStringArrayOwnership;

typedef enum
{
    PGY_COLLECTION_DISPOSITION_LIVE = 1,
    PGY_COLLECTION_DISPOSITION_RETIRED,
    /* The backing and owned elements were retired, but an empty descriptor
     * still has to return to the exact aggregate field it was moved from. */
    PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE
} PgyCollectionDisposition;

typedef enum
{
    PGY_COLLECTION_ORIGIN_UNKNOWN = 0,
    PGY_COLLECTION_ORIGIN_BORROWED_LITERAL,
    PGY_COLLECTION_ORIGIN_MAP_KEYS,
    PGY_COLLECTION_ORIGIN_BINDING,
    PGY_COLLECTION_ORIGIN_EMPTY_LITERAL,
    PGY_COLLECTION_ORIGIN_MEMBER_MOVE,
    PGY_COLLECTION_ORIGIN_CLONE
} PgyCollectionOrigin;

typedef enum
{
    PGY_COLLECTION_EFFECT_NONE = 0,
    PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH,
    PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
    PGY_COLLECTION_EFFECT_DROP
} PgyCollectionOwnershipEffectKind;

/* Semantic-owned stable row.  Symbol pointers are permitted only as a
 * transient scope lookup while recording or locating this row; they are not
 * semantic identity and are never projected to HIR/MIR. */
typedef struct PgyCollectionOwnershipFact
{
    uint32_t function_syntax_id;
    uint32_t binding_syntax_id;
    uint32_t origin_syntax_id;
    uint32_t source_binding_syntax_id;
    PgyStringArrayOwnership element_ownership;
    PgyCollectionDisposition disposition;
    PgyCollectionOrigin origin;
    /* Native RETURN-element premise; not a HIR/MIR ownership classification. */
    bool return_elements_owned;
    const ASTNode *return_elements_source;
    uint32_t return_elements_source_id;
    /* MEMBER_MOVE only, and only while type checking: the source Symbol's
     * birth_serial when the field moved (0 when unknown). HIR/MIR ignore it. */
    uint64_t source_birth_serial;
} PgyCollectionOwnershipFact;

bool semantic_collection_ownership_initialize_binding(
    Symbol *binding,
    const ASTNode *initializer,
    const Type *binding_type,
    SemanticContext *ctx);

bool semantic_collection_reject_unsafe_owned_string_mutation(
    ASTNode *receiver,
    const char *operation,
    SemanticContext *ctx);

bool semantic_collection_reject_moved_member_use(
    ASTNode *member_access,
    SemanticContext *ctx);

bool semantic_collection_admit_owned_string_push(
    ASTNode *receiver,
    SemanticContext *ctx);

/* Admission's escape and uniform-ownership conditions, without a refusal:
 * the preserved-storage condition stays with admission, which records it. */
bool semantic_collection_owned_string_push_admissible(
    ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_restore_moved_member(
    ASTNode *target,
    ASTNode *value,
    SemanticContext *ctx);

/* A released field carrier is not a general readable value.  This query
 * admits only the exact move-back assignment long enough to type that empty
 * descriptor; semantic_collection_restore_moved_member closes the fact. */
Type *semantic_collection_released_member_restore_value_type(
    ASTNode *target,
    ASTNode *value,
    SemanticContext *ctx);

bool semantic_collection_require_no_released_member_move(
    ASTNode *function_decl,
    SemanticContext *ctx);

bool semantic_collection_admit_owned_string_drop(
    ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_reject_unsafe_string_array_assignment(
    ASTNode *target,
    ASTNode *value,
    const Type *target_type,
    const Type *value_type,
    SemanticContext *ctx);

bool semantic_collection_record_call_effect(
    ASTNode *call,
    ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind,
    SemanticContext *ctx);

bool semantic_collection_record_call_effect_from(
    ASTNode *call,
    ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind,
    uint32_t source_syntax_id,
    SemanticContext *ctx);

bool semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out);

void semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx);

const PgyCollectionOwnershipFact *semantic_collection_ownership_fact_find(
    const SemanticContext *ctx,
    uint32_t function_syntax_id,
    uint32_t binding_syntax_id);

void pgy_collection_ownership_facts_destroy(
    PgyCollectionOwnershipFact *facts,
    size_t count);

#endif
