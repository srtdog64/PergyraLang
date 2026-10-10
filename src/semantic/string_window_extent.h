#ifndef PERGYRA_STRING_WINDOW_EXTENT_H
#define PERGYRA_STRING_WINDOW_EXTENT_H

#include <stdbool.h>
#include <stdint.h>
#include "../parser/ast.h"

typedef struct SemanticContext SemanticContext;
typedef struct StringWindowExtentStore StringWindowExtentStore;

/* String-window extent owner (docs/agent_work_directives/
 * string_window_extent_closure_2026-10-10.md). The seven window builtins
 * read their source through a caller-supplied extent; this owner admits a
 * call only when that extent is proven to be the source's StringLength.
 * Pass 2 records declarations, writes and calls once; finalization decides
 * after every routine is seen, so requirements do not depend on order. */
bool semantic_string_window_extent_begin(SemanticContext *ctx);
void semantic_string_window_extent_record_routine(SemanticContext *ctx,
                                                  ASTNode *func_decl);
void semantic_string_window_extent_record_let(SemanticContext *ctx,
                                              ASTNode *let_decl);
void semantic_string_window_extent_record_assignment(SemanticContext *ctx,
                                                     ASTNode *target);
void semantic_string_window_extent_record_call(SemanticContext *ctx,
                                               ASTNode *call);
/* A nominal record construction; `decl` is its class or struct. */
void semantic_string_window_extent_record_constructor(SemanticContext *ctx,
                                                      ASTNode *call,
                                                      ASTNode *decl);
void semantic_string_window_extent_record_function_value(
    SemanticContext *ctx, ASTNode *identifier, uint32_t decl_id);
bool semantic_string_window_extent_finalize(SemanticContext *ctx);
void semantic_string_window_extent_destroy(StringWindowExtentStore *store);

#endif
