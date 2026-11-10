#ifndef PGY_DEFAULT_AGGREGATE_MOVE_RULE_H
#define PGY_DEFAULT_AGGREGATE_MOVE_RULE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Type Type;

/* A value of `type` is an aggregate: copying it copies a struct, tuple,
 * collection or payload enum by value and shares the storage it holds. */
bool semantic_default_aggregate_value(SemanticContext *ctx, const Type *type);
/* An own formal consumes its aggregate; a default formal moves when its
 * final flow summary changes, keeps, returns or releases shared storage. */
bool semantic_default_aggregate_formal_moves(SemanticContext *ctx, ASTNode *callee, size_t formal);
/* A borrowed caller may hand storage through this default formal, but the
 * parameter-flow owner must carry the escape to the caller; this is not a
 * mutation/return/release that the move owner may repair with a copy. */
bool semantic_default_aggregate_formal_borrow_handoff(
    SemanticContext *ctx, ASTNode *callee, size_t formal);
/* The callee only reads that formal and returns it or a part of it. */
bool semantic_default_aggregate_formal_lends(SemanticContext *ctx, ASTNode *callee, size_t formal);

#endif
