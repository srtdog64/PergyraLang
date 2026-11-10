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

/* Exact checked spelling identity for collection builtins; consumers do not
 * recreate the unshadowed-call rule from callee text. */
bool collection_call_is_unshadowed_builtin(const ASTNode *expr,
    const char *expected_name);
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
    PGY_COLLECTION_ORIGIN_CLONE,
    PGY_COLLECTION_ORIGIN_CALL_RESULT,
    PGY_COLLECTION_ORIGIN_OWNED_LITERAL
} PgyCollectionOrigin;

typedef enum
{
    PGY_COLLECTION_EFFECT_NONE = 0,
    PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH,
    PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
    PGY_COLLECTION_EFFECT_DROP,
    PGY_COLLECTION_EFFECT_OWNED_STRING_SET,
    PGY_COLLECTION_EFFECT_OWNED_STRING_POP
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
    /* An owned mutation/drop was admitted from the provisional declared-call
     * result premise. Finalization must either promote that exact result to a
     * durable call-result fact or fail closed before HIR consumes the call. */
    bool call_result_effect_pending;
    /* MEMBER_MOVE only, and only while type checking: the source Symbol's
     * birth_serial when the field moved (0 when unknown). HIR/MIR ignore it. */
    uint64_t source_birth_serial;
    /* The original typed member expression owns the complete root-to-leaf
     * path identity during semantic analysis. It is never projected or freed
     * by this fact store; the program AST owns its lifetime. */
    const ASTNode *member_move_source;
    uint32_t member_move_source_id;
    /* Path-local validity of the source member's original element premise.
     * A shallow element store revokes it; owned mutations and exact preserving
     * inout calls keep it. Resource-flow snapshots own branch/loop merging. */
    bool member_move_elements_current;
} PgyCollectionOwnershipFact;

bool semantic_collection_ownership_initialize_binding(
    Symbol *binding,
    const ASTNode *initializer,
    const Type *binding_type,
    SemanticContext *ctx);

/* Finalize an Array<String> local born from one declared callable only after
 * the array-storage owner has sealed the callee result.  The durable fact
 * carries the callee identity; the source-local MIR receipt separately keeps
 * the exact initializer-call identity. */
bool semantic_collection_ownership_finalize_call_result_births(
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
    bool prior_exclusive_receipt,
    SemanticContext *ctx);

/* True only while `value` is the exact live Array<String> carrier detached
 * from an aggregate member and its element-lifetime owner has proved that
 * every stored String is owned.  The final aggregate-move owner consumes
 * this fact when a nominal constructor takes the carrier; type checking must
 * not eagerly retire it before later-use/copy selection is complete. */
bool semantic_collection_owned_member_constructor_source(
    const ASTNode *value,
    SemanticContext *ctx);

/* An owned Array<String> replacement keeps uniform element ownership by
 * copying the incoming String before retiring the overwritten element. */
bool semantic_collection_owned_string_set_required(
    ASTNode *receiver,
    SemanticContext *ctx);
/* The sealed collection and publication owners identify this receiver as the
 * exact live local Array<String> declared from an empty literal in `routine`.
 * This origin fact is not by itself permission for an owned store. */
bool semantic_collection_owned_string_empty_local_origin_for_routine(
    const ASTNode *receiver,
    const ASTNode *routine,
    SemanticContext *ctx);
/* The same exact birth identity while the routine scope is still live and
 * before publication is sealed.  The caller captures the returned initializer
 * as immutable call-entry evidence; later replacement cannot reconstruct it. */
const ASTNode *semantic_collection_owned_string_empty_local_initializer_at_call_entry(
    const ASTNode *receiver,
    const Symbol *binding,
    const ASTNode *routine,
    SemanticContext *ctx);
bool semantic_collection_admit_owned_string_set(
    ASTNode *receiver,
    SemanticContext *ctx);

/* An owned pop preserves the Array owner but retires exactly its removed
 * String.  The ordinary shallow pop remains the contract for borrowed
 * elements. */
bool semantic_collection_owned_string_pop_required(
    ASTNode *receiver,
    SemanticContext *ctx);
bool semantic_collection_admit_owned_string_pop(
    ASTNode *receiver,
    SemanticContext *ctx);

/* Admission's escape and uniform-ownership conditions, without a refusal:
 * the preserved-storage condition stays with admission, which records it. */
bool semantic_collection_owned_string_push_admissible(
    ASTNode *receiver,
    SemanticContext *ctx);

/* An owned String may transfer into an `own`, `inout`, or moving default
 * Array<String> formal, including through its exact immutable local carrier,
 * only after the formal requirement owner records that every concrete caller
 * must provide uniformly owned elements. This is an evidence-producing
 * admission, not a pure query. `parameter_receiver_out` distinguishes a
 * private local from a formal carrier whose evidence was refused, so callers
 * cannot fall back to the private-push path. */
bool semantic_collection_owned_string_parameter_push_admit(
    ASTNode *call,
    ASTNode *receiver,
    SemanticContext *ctx,
    bool *parameter_receiver_out);
/* The same caller-owned element requirement for a pop through a parameter.
 * Admission only selects the retiring pop when every concrete caller's
 * element premise is later sealed by the requirement owner. */
bool semantic_collection_owned_string_parameter_pop_admit(
    ASTNode *call,
    ASTNode *receiver,
    SemanticContext *ctx,
    bool *parameter_receiver_out);

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

/* Final fixed-point consumer for a typed aggregate-member value-copy plan.
 * The candidate owner has already revalidated the current aggregate path;
 * this seals the exact lowered call receipt without reviving the carrier's
 * end-of-routine disposition. */
bool semantic_collection_record_member_value_copy_replay_effect(
    ASTNode *call,
    ASTNode *receiver,
    SemanticContext *ctx);

/* Revalidate a selected member-value copy after the function publication is
 * sealed.  Either the original aggregate member path still proves owned
 * elements, or a compiler-owned whole-array Clone has replaced that carrier
 * with an independent owned Array in the same fixed point. */
bool semantic_collection_member_value_copy_replay_ready(
    ASTNode *call,
    ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out);

/* Exact current owned-String mutation receipts.  The push query accepts both
 * lowering shapes: ArrayPushOwnedString transfers an owned value with source
 * zero, while a general ArrayPush stores one independently-produced String
 * and names that producer.  The set query accepts only the compiler-owned
 * replacement lowering.  Consumers do not reconstruct either fact from a
 * callee spelling. */
bool semantic_collection_owned_string_push_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_owned_string_set_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_owned_string_pop_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx);

/* Backend-visible sealed receipt shape.  Semantic analysis remains the only
 * issuer; emitters may only choose the owned-pop ABI for this exact call and
 * receiver identity. */
bool semantic_collection_owned_string_pop_receipt_ready(
    const ASTNode *call,
    const ASTNode *receiver);

void semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx);
/* Pending-only exact call link.  This does not publish ownership; it lets the
 * existing array deferral owner wait for the result receipt's final proof. */
bool semantic_collection_owned_string_result_candidate(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out);
/* The callee published an owned-result candidate, but its sealed source graph
 * could not prove a base.  This is distinct from a callee that never owned a
 * candidate: callers may copy the latter, but must not turn a cyclic proof
 * into ownership. */
bool semantic_collection_owned_string_result_cycle_withdrawn_current(
    const ASTNode *expression, SemanticContext *ctx);
/* Demand only the sealed parameter-flow rows needed by result candidates
 * whose callee has no owned-result receipt of its own. */
bool semantic_collection_owned_string_result_flow_demands_prepare(
    SemanticContext *ctx);
bool semantic_collection_owned_string_results_prepare(SemanticContext *ctx);

const PgyCollectionOwnershipFact *semantic_collection_ownership_fact_find(
    const SemanticContext *ctx,
    uint32_t function_syntax_id,
    uint32_t binding_syntax_id);

void pgy_collection_ownership_facts_destroy(
    PgyCollectionOwnershipFact *facts,
    size_t count);

#endif
