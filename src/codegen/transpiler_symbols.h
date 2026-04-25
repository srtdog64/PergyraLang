/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend local symbol, slot, and alias tracking.
 */

#ifndef PERGYRA_TRANSPILER_SYMBOLS_H
#define PERGYRA_TRANSPILER_SYMBOLS_H

#include "transpiler.h"

void register_slot_var(TranspilerCtx *ctx, const char *name,
                       const char *inner_type, bool is_secure,
                       bool is_indirect);
void set_slot_token_name(TranspilerCtx *ctx, const char *slot_name,
                         const char *token_name);
const char *lookup_slot_type(TranspilerCtx *ctx, const char *var_name);
bool lookup_slot_is_secure(TranspilerCtx *ctx, const char *var_name);
const char *lookup_slot_token_name(TranspilerCtx *ctx, const char *var_name);
bool lookup_slot_is_indirect(TranspilerCtx *ctx, const char *var_name);
char *slot_ref_expr(TranspilerCtx *ctx, const char *slot_name,
                    const char *slot_expr);
bool is_slot_var(TranspilerCtx *ctx, const char *var_name);

void register_typed_var(TranspilerCtx *ctx, const char *name,
                        const char *type_name);
void register_alias_var(TranspilerCtx *ctx, const char *name,
                        ASTNode *target_expr);
ASTNode *lookup_alias_expr(TranspilerCtx *ctx, const char *var_name);
TypedVarEntry *lookup_typed_entry(TranspilerCtx *ctx, const char *var_name);
const char *lookup_typed_var(TranspilerCtx *ctx, const char *var_name);
void register_view_like_var(TranspilerCtx *ctx, const char *name,
                            const char *type_name,
                            const char *source_slot,
                            bool source_secure,
                            bool is_move_token);
void register_projection_borrow_var(TranspilerCtx *ctx, const char *name,
                                    const char *type_name,
                                    const char *source_name);

#endif /* PERGYRA_TRANSPILER_SYMBOLS_H */
