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
/* The callee changes, keeps, returns or releases the storage it receives
 * through its default-mode formal `formal`. */
bool semantic_default_aggregate_formal_moves(SemanticContext *ctx, ASTNode *callee, size_t formal);
/* The callee only reads that formal and returns it or a part of it. */
bool semantic_default_aggregate_formal_lends(SemanticContext *ctx, ASTNode *callee, size_t formal);

#endif
