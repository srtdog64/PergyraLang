#ifndef PGY_FORMAL_STRING_STORE_LANDING_OWNER_H
#define PGY_FORMAL_STRING_STORE_LANDING_OWNER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

/* A String of a by-value or `ref` formal that a store builtin puts into an
 * `inout` output leaves the routine; no proof admits it. Nor does the value
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

/* The same selection while the store call is checked, before it has a
 * publication row: no owner proves its String alive for the receiver, and
 * no owned-copy store of it was admitted. */
void semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal);

#endif
