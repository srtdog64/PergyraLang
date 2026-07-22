/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend context/output helpers.
 */

#ifndef PERGYRA_TRANSPILER_CONTEXT_H
#define PERGYRA_TRANSPILER_CONTEXT_H

#include "transpiler.h"

void transpiler_write_indent(TranspilerCtx *ctx);
void transpiler_write_indent_to(CodeBuf *buf, int indent);
void transpiler_set_backend_error(TranspilerCtx *ctx, const char *fmt, ...);
void transpiler_set_backend_error_with_hints(TranspilerCtx *ctx,
                                             const char *code,
                                             const char *cause_ir,
                                             const char *fix_source,
                                             const char *fmt, ...);
void transpiler_set_mir_inventory_missing(TranspilerCtx *ctx,
                                          const char *fmt, ...);
bool transpiler_machine_layer_projection_is_bound(
    const TranspilerCtx *ctx);
void transpiler_set_mir_topology_invalid(TranspilerCtx *ctx,
                                         const char *fmt, ...);
void transpiler_set_mir_intent_carrier_missing(TranspilerCtx *ctx,
                                               const char *fmt, ...);
char *transpiler_scratch_strdup(TranspilerCtx *ctx, const char *s);
char *transpiler_scratch_fmt(TranspilerCtx *ctx, const char *fmt, ...);

/* Region plan consumer.  These helpers own only the C spelling of the
 * function-scope lifetime; the disposition itself always comes from the
 * verified plan lookup. */
bool transpiler_region_scope_for_function_id(const TranspilerCtx *ctx,
                                             uint32_t function_syntax_id,
                                             uint32_t *scope_id_out);
void transpiler_region_scope_begin(TranspilerCtx *ctx, uint32_t scope_id);
void transpiler_region_scope_destroy(TranspilerCtx *ctx);
void transpiler_region_scope_end(TranspilerCtx *ctx);
char *transpiler_region_concat(TranspilerCtx *ctx,
                               const ASTNode *site,
                               const char *left,
                               const char *right);

void write_indent(TranspilerCtx *ctx);
void write_indent_to(CodeBuf *buf, int indent);

#endif /* PERGYRA_TRANSPILER_CONTEXT_H */
