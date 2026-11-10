#ifndef PGY_FORMAL_STRING_STORE_LANDING_OWNER_H
#define PGY_FORMAL_STRING_STORE_LANDING_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct ArrayStorageReturnElementReceipt ArrayStorageReturnElementReceipt;

/* Stable meaning of a store landing across the semantic reload boundary.
 * VALUE_COPY wraps only the stored value. OWNED_PUSH transfers one already
 * independent String into a uniformly owned Array. OWNED_SET replaces an
 * element in an Array<String> whose old and new elements are both cleanup-
 * owned. The fixed point may refine VALUE_COPY to either owned operation for
 * the same store because both include the earlier copy obligation. */
typedef enum {
    PGY_STRING_STORE_LANDING_VALUE_COPY = 0,
    PGY_STRING_STORE_LANDING_OWNED_PUSH = 1,
    PGY_STRING_STORE_LANDING_OWNED_SET = 2,
    PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY = 3
} PgyStringStoreLandingKind;

/* A String of a by-value or `ref` formal that a store builtin puts into an
 * `inout` output leaves the routine; no proof admits it, nor one for a
 * literal stored as an `Array<String>` element there. Nor does the value
 * of a store call an earlier analysis selected, wherever its receiver is;
 * the selection planned only values Clone copies whole. Before the value
 * argument is checked, it is replaced by `Clone(value)`, an owned copy
 * (2026-10-19_string_landing_copy_then_move). Rechecking reuses a compiler
 * copy already built here. False leaves the original argument and synthetic
 * identity cursor unchanged when the required copy could not be built. */
bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx);

/* A planned ArrayPush whose receiver admits an owned String push lands as
 * ArrayPushOwnedString instead of a Clone: the collection then owns that
 * String as it owns its other elements, and no plain push mixes into it. */
bool semantic_formal_string_store_landing_owned_push(ASTNode *call, SemanticContext *ctx);
/* This failed discovery generation already selected this exact call as an
 * owned push.  Consumers may use the projection only to finish discovering
 * the same generation; replay remains the proof that the owned push landed. */
bool semantic_formal_string_store_landing_selected_owned_push(
    const SemanticContext *ctx, const ASTNode *call);
bool semantic_formal_string_store_landing_value_copy(const ASTNode *call,
    SemanticContext *ctx);
bool semantic_formal_string_store_landing_owned_array_plan_current(
    SemanticContext *ctx, const ASTNode *call);

/* A planned ArraySet on an owned Array<String> becomes the compiler-owned
 * replacement operation. The runtime copies before retiring the old element,
 * so no source-level Clone or manual drop is exposed. */
bool semantic_formal_string_store_landing_owned_set(ASTNode *call,
    SemanticContext *ctx);
bool semantic_formal_string_store_landing_select_owned_set(
    SemanticContext *ctx, const ASTNode *call);
/* The flow summary selected a shallow ArraySet through one exact
 * `inout Array<String>` formal.  Replay may lower only that planned call,
 * while the owned-element requirement owner validates every caller. */
bool semantic_formal_string_store_landing_inout_owned_set_current(
    ASTNode *call, SemanticContext *ctx);
bool semantic_formal_string_store_landing_inout_owned_set_for_formal(
    ASTNode *call, SemanticContext *ctx, ASTNode *routine,
    size_t formal_index);
bool semantic_formal_string_store_landing_inout_set_receiver_current(
    const ASTNode *call, SemanticContext *ctx);
bool semantic_formal_string_store_landing_select_inout_owned_set(
    SemanticContext *ctx, const ASTNode *call, ASTNode *routine,
    size_t formal_index);

/* After the legacy escape owner has finished selecting exact store calls,
 * consume the current per-routine program-point index and schedule the same
 * block's straight-line String stores. Exact scalar Array stores and pure
 * scalar/String lets are transparent; every scheduled store is type- and
 * ownership-checked on replay. This is a bounded landing batch, not another
 * syntax scan or cross-routine error tail. */
bool semantic_formal_string_store_landing_finalize(SemanticContext *ctx);

/* The flow summary of `routine` found its formal `root` escaping through
 * argument `ordinal` of a store builtin. When that argument is the stored
 * String, the call is selected for a second analysis to land, wherever the
 * receiver is: no proof admitted the store. A NULL `root` is the root of
 * that argument's path. False means the selected landing receipt could not
 * be recorded. */
bool semantic_formal_string_store_landing_select(SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, const ASTNode *root, const ASTNode *routine);

/* A by-value or `ref` formal's backing frame found the value of an Array
 * store (argument `value_ordinal`) carrying the formal's backing into an
 * `inout` output. The call is selected when Clone copies the value whole:
 * the next analysis stores that copy, and the output keeps nothing of the
 * formal. False means a required input or receipt was unavailable. */
bool semantic_formal_store_landing_select_held(SemanticContext *ctx, const ASTNode *call,
    size_t value_ordinal);

/* This analysis selected the store call `call`, and `value` is the value it
 * stores: the next analysis stores a Clone of it. */
bool semantic_formal_store_landing_selects_value(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *value);

/* Whether one exact store selected in this failed discovery analysis takes
 * `value` as its stored operand.  This is a planning projection only: it lets
 * later landing owners avoid scheduling a second copy for the same value;
 * the reloaded AST must still replay and validate the store landing. */
bool semantic_formal_store_landing_value_selected(
    const SemanticContext *ctx, const ASTNode *value);

/* The same for an assignment: `value` carries the backing of the frame's
 * input, the formal `input` of `routine`, into another `inout` formal,
 * whole or through a field or element. The value is selected for a next
 * analysis to assign as `Clone(value)` when that Clone copies it whole; a
 * local assigned back, or a value the input assigns to itself, lands
 * nothing. False only when the selection could not be recorded. */
bool semantic_formal_store_landing_select_assigned(SemanticContext *ctx, const ASTNode *routine,
    uint32_t input, const ASTNode *target, const ASTNode *value);

/* A plain String push into an Array<String> that owns its elements is
 * refused; the call is selected so that a next analysis lands it as an
 * owned String push (ArrayPushOwnedString) when the receiver admits one. */
bool semantic_formal_string_store_landing_select_owned(SemanticContext *ctx, const ASTNode *call);
/* A plain ArrayPush was provisionally sealed as an owned-result transfer, but
 * the exact non-cyclic result candidate did not survive final flow proof.
 * Record the ordinary owned-copy store plan without treating that withdrawn
 * candidate as ownership. */
bool semantic_formal_string_store_landing_select_deferred_result_copy(
    SemanticContext *ctx, const ASTNode *call);

/* A sealed inout requirement found that one exact empty local Array<String>
 * must carry only owned elements. Find a current shallow push on that same
 * binding and delegate the receiver-wide rewrite to select_owned. This
 * selects a replay plan only; it grants no permission to the current round. */
bool semantic_formal_string_store_landing_select_empty_local_receiver(
    SemanticContext *ctx, const ASTNode *receiver,
    const ArrayStorageReturnElementReceipt *receipt);

/* Select an exact value-only copy. Unlike select_owned, this plan must never
 * be reinterpreted from the receiver's transient state on the next analysis. */
bool semantic_formal_string_store_landing_select_value_copy(
    SemanticContext *ctx, const ASTNode *call);

/* Record no permission: a sealed generation later proves that this exact
 * shallow push targets an initializer-proven Array<String> which owns all of
 * its elements. */
bool semantic_formal_string_store_landing_record_owned_array_candidate(
    SemanticContext *ctx, const ASTNode *call);

/* Record the first shallow String store into an aggregate-member carrier.
 * This grants no copy: the sealed collection requirement must still prove the
 * member path, and the default-aggregate move pass may block the candidate
 * when a caller already makes that callee irreparably unsafe. */
bool semantic_formal_string_store_landing_record_member_value_copy_candidate(
    SemanticContext *ctx, const ASTNode *call);

/* Whether this exact shallow push already carries the deferred member-copy
 * obligation recorded above. This grants no owned mutation effect: it only
 * lets the collection lifetime owner keep the pre-store member-path premise
 * until the sealed finalizer either selects this copy or emits a refusal. */
bool semantic_formal_string_store_landing_member_value_copy_candidate_recorded(
    const SemanticContext *ctx, const ASTNode *call);

/* The sealed member-path proof selected a member VALUE_COPY for this store.
 * Replay consumes this receipt instead of reconstructing the pre-copy member
 * premise after the copy has deliberately changed the stored expression. */
bool semantic_formal_string_store_landing_member_value_copy_selected(
    const ASTNode *call, SemanticContext *ctx);

/* The landing-only aggregate move walk found an uncopyable reuse of a value
 * moved into `routine`.  Member-store candidates inside that exact routine
 * cannot repair the caller and must not create a speculative replay. */
void semantic_formal_string_store_landing_block_member_value_copy_candidates(
    SemanticContext *ctx, const ASTNode *routine);

/* The final parameter-flow summary found an Array result still depending on
 * default formal `formal_id`.  Select every current plain String store whose
 * value is rooted at that exact formal and whose receiver can replay as an
 * owned-String Array.  The next analysis must remove the return dependency;
 * no candidate, or a remaining non-store path, stays a declaration refusal.
 * False means the program-point evidence or landing receipt was unavailable. */
bool semantic_formal_string_store_landing_select_return_dependency(
    SemanticContext *ctx, ASTNode *routine, uint32_t formal_id,
    bool *selected_out);

/* Whether this analysis already selected the seed store for the current
 * routine.  After another routine owns the first refusal, the indexed String
 * checker may discover one seed here; the program-point owner, not a later
 * checker walk, decides which immediately following stores share its bounded
 * snapshot. */
bool semantic_formal_string_store_landing_current_routine_seeded(
    const SemanticContext *ctx);

/* The same selection while the store call is checked, before it has a
 * publication row: no owner proves its String alive for the receiver, and
 * no owned-copy store of it was admitted. */
bool semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal);

#endif
