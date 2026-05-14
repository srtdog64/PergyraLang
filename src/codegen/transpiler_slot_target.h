/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend slot target resolution helpers.
 */

#ifndef PERGYRA_TRANSPILER_SLOT_TARGET_H
#define PERGYRA_TRANSPILER_SLOT_TARGET_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

bool transpiler_c_expr_is_plain_identifier(const char *expr);
void transpiler_refine_slot_target_from_emitted_expr(TranspilerCtx *ctx,
                                                     const char *slot_expr,
                                                     const char **slot_name_io,
                                                     bool *secure_io);
bool transpiler_resolve_slot_target_copy(TranspilerCtx *ctx,
                                         ASTNode *slot_arg,
                                         char *inner_out,
                                         size_t inner_out_size,
                                         const char **slot_name_out,
                                         bool *secure_out);
bool transpiler_resolve_device_slot_inner_copy_or_error(
    TranspilerCtx *ctx,
    ASTNode *slot_arg,
    const char *operation,
    char *inner_out,
    size_t inner_out_size);

#endif /* PERGYRA_TRANSPILER_SLOT_TARGET_H */
