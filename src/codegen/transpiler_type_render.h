/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type rendering seam.
 */

#ifndef PERGYRA_TRANSPILER_TYPE_RENDER_H
#define PERGYRA_TRANSPILER_TYPE_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

char *render_type_name(ASTNode *type_node);
char *render_type_name_in_ctx(TranspilerCtx *ctx, ASTNode *type_node);
const char *transpiler_render_type_name_local(TranspilerCtx *ctx,
                                              ASTNode *type_node);
bool pergyra_ast_type_to_c_copy(ASTNode *type_node, char *out,
                                size_t out_size);
bool pergyra_ast_type_to_c_copy_in_ctx(TranspilerCtx *ctx,
                                       ASTNode *type_node,
                                       char *out,
                                       size_t out_size);
void ensure_type_specializations_from_ast_to(TranspilerCtx *ctx, CodeBuf *dst,
                                             ASTNode *type_node);
void ensure_type_specializations_from_ast(TranspilerCtx *ctx,
                                          ASTNode *type_node);

#endif /* PERGYRA_TRANSPILER_TYPE_RENDER_H */
