/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend lexical defer support.
 */

#include "transpiler_defer_emit.h"

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_type_require.h"

#include <stdio.h>
#include <stdlib.h>

void
transpiler_defer_scope_push(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    if (ctx->defer_scope_depth >= TRANSPILE_MAX_SCOPE_DEPTH) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer scope registry exceeded TRANSPILE_MAX_SCOPE_DEPTH");
        return;
    }
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

void
transpiler_defer_scope_pop(TranspilerCtx *ctx)
{
    if (ctx == NULL || ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

void
transpiler_register_defer(ASTNode *body, TranspilerCtx *ctx)
{
    int scope;
    int count;

    if (ctx == NULL || body == NULL)
        return;
    if (ctx->defer_scope_depth <= 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer statement has no active defer scope");
        return;
    }

    scope = ctx->defer_scope_depth - 1;
    count = ctx->defer_body_counts[scope];
    if (count >= TRANSPILE_MAX_DEFER_PER_SCOPE) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer registry exceeded TRANSPILE_MAX_DEFER_PER_SCOPE");
        return;
    }

    ctx->defer_bodies[scope][count] = body;
    ctx->defer_mir_instructions[scope][count] =
        ctx->active_mir_instruction;
    ctx->defer_body_counts[scope]++;
}

void
transpiler_emit_defers_from(TranspilerCtx *ctx, int start_depth)
{
    if (ctx == NULL)
        return;
    if (start_depth < 0)
        start_depth = 0;

    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            const MIRInstruction *saved_mir_instruction =
                ctx->active_mir_instruction;
            if (body != NULL) {
                ctx->active_mir_instruction =
                    ctx->defer_mir_instructions[depth][i];
                emit_statement(body, ctx);
                ctx->active_mir_instruction = saved_mir_instruction;
            }
        }
    }
}

void
transpiler_mut_ref_params_reset(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    ctx->mut_ref_param_count = 0;
}

void
transpiler_register_mut_ref_param(TranspilerCtx *ctx, const char *name,
    const char *ctype)
{
    if (ctx == NULL || name == NULL || ctype == NULL)
        return;
    if (ctx->mut_ref_param_count >= TRANSPILE_MAX_MUT_REF_PARAMS) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend inout parameter registry exceeded TRANSPILE_MAX_MUT_REF_PARAMS");
        return;
    }
    pergyra_str_copy(ctx->mut_ref_param_names[ctx->mut_ref_param_count],
        sizeof(ctx->mut_ref_param_names[ctx->mut_ref_param_count]), name);
    pergyra_str_copy(ctx->mut_ref_param_ctypes[ctx->mut_ref_param_count],
        sizeof(ctx->mut_ref_param_ctypes[ctx->mut_ref_param_count]), ctype);
    ctx->mut_ref_param_count++;
}

void
transpiler_emit_mut_ref_copyins(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    for (int i = 0; i < ctx->mut_ref_param_count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = *%s__mutref;\n",
            ctx->mut_ref_param_ctypes[i],
            ctx->mut_ref_param_names[i],
            ctx->mut_ref_param_names[i]);
    }
}

void
transpiler_emit_mut_ref_writebacks(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    for (int i = 0; i < ctx->mut_ref_param_count; i++) {
        const char *writeback_value = transpiler_resolve_active_ssa_name(
            ctx, ctx->mut_ref_param_names[i]);
        char *owned_writeback_value = NULL;
        if (writeback_value != NULL) {
            owned_writeback_value = transpiler_make_c_ssa_name(
                ctx, writeback_value);
            writeback_value = owned_writeback_value;
        }
        if (writeback_value == NULL) {
            writeback_value = ctx->mut_ref_param_names[i];
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "*%s__mutref = %s;\n",
            ctx->mut_ref_param_names[i], writeback_value);
        free(owned_writeback_value);
    }
}

const char *
transpiler_emit_mut_ref_return_capture(TranspilerCtx *ctx,
                                       const char *return_expr,
                                       char *temp_name,
                                       size_t temp_name_size)
{
    char ctype[256];
    int written;

    if (ctx == NULL || return_expr == NULL)
        return NULL;
    if (ctx->mut_ref_param_count == 0)
        return return_expr;
    if (temp_name == NULL || temp_name_size == 0)
        return NULL;
    if (!transpiler_require_type_name_c_type_copy(ctx,
            ctx->current_return_type, "inout return capture",
            ctype, sizeof(ctype))) {
        return NULL;
    }

    written = snprintf(temp_name, temp_name_size,
        "_pgy_return_value_%d", ++ctx->tmp_counter);
    if (written < 0 || (size_t)written >= temp_name_size) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend could not mint a bounded inout return temporary");
        return NULL;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "%s %s = %s;\n",
        ctype, temp_name, return_expr);
    return temp_name;
}
