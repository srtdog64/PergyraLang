#ifndef PGY_FORMAL_STRING_STORE_LANDING_OWNER_H
#define PGY_FORMAL_STRING_STORE_LANDING_OWNER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

/* A String of a by-value or `ref` formal that a store builtin puts into an
 * `inout` output leaves the routine; no proof admits it. Before the value
 * argument is checked, it is replaced by `Clone(value)`, an owned copy
 * (2026-10-19_string_landing_copy_then_move). False only when the landing
 * was due and could not be built. */
bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx);

#endif
