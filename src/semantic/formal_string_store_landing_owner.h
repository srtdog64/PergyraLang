#ifndef PGY_FORMAL_STRING_STORE_LANDING_OWNER_H
#define PGY_FORMAL_STRING_STORE_LANDING_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

/* A String of a by-value or `ref` formal that a store builtin puts into an
 * `inout` output leaves the routine; no proof admits it, nor one for a
 * literal stored as an `Array<String>` element there. Nor does the value
 * of a store call an earlier analysis selected, wherever its receiver is;
 * the selection planned only values Clone copies whole. Before the value
 * argument is checked, it is replaced by `Clone(value)`, an owned copy
 * (2026-10-19_string_landing_copy_then_move). False only when the landing
 * was due and could not be built. */
bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx);

/* A planned ArrayPush whose receiver admits an owned String push lands as
 * ArrayPushOwnedString instead of a Clone: the collection then owns that
 * String as it owns its other elements, and no plain push mixes into it. */
bool semantic_formal_string_store_landing_owned_push(ASTNode *call, SemanticContext *ctx);

/* A planned ArraySet on an owned Array<String> becomes the compiler-owned
 * replacement operation. The runtime copies before retiring the old element,
 * so no source-level Clone or manual drop is exposed. */
bool semantic_formal_string_store_landing_owned_set(ASTNode *call,
    SemanticContext *ctx);
bool semantic_formal_string_store_landing_select_owned_set(
    SemanticContext *ctx, const ASTNode *call);

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
 * that argument's path. */
void semantic_formal_string_store_landing_select(SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, const ASTNode *root, const ASTNode *routine);

/* A by-value or `ref` formal's backing frame found the value of an Array
 * store (argument `value_ordinal`) carrying the formal's backing into an
 * `inout` output. The call is selected when Clone copies the value whole:
 * the next analysis stores that copy, and the output keeps nothing of the
 * formal. */
void semantic_formal_store_landing_select_held(SemanticContext *ctx, const ASTNode *call, size_t value_ordinal);

/* This analysis selected the store call `call`, and `value` is the value it
 * stores: the next analysis stores a Clone of it. */
bool semantic_formal_store_landing_selects_value(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *value);

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

/* The indexed String owner proved that `risk_owner_binding_id` is a live
 * Array<String> binding whose owned elements make storing this formal-rooted
 * value require an independent copy.  The receipt is transient and can only
 * batch immediately following stores in the same routine. */
bool semantic_formal_string_store_landing_record_risk_owner(
    SemanticContext *ctx, const ASTNode *call, const ASTNode *value,
    uint32_t risk_owner_binding_id);

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
void semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal);

#endif
