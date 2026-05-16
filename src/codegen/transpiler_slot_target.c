/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend slot target resolution helpers.
 */

#include <string.h>

#include "codegen_slot_type_policy.h"
#include "transpiler_context.h"
#include "transpiler_slot_target.h"
#include "transpiler_symbols.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

bool
transpiler_c_expr_is_plain_identifier(const char *expr)
{
    if (expr == NULL || expr[0] == '\0')
        return false;
    if (!((expr[0] >= 'A' && expr[0] <= 'Z')
          || (expr[0] >= 'a' && expr[0] <= 'z')
          || expr[0] == '_')) {
        return false;
    }
    for (const char *p = expr + 1; *p != '\0'; p++) {
        if (!((*p >= 'A' && *p <= 'Z')
              || (*p >= 'a' && *p <= 'z')
              || (*p >= '0' && *p <= '9')
              || *p == '_')) {
            return false;
        }
    }
    return true;
}

void
transpiler_refine_slot_target_from_emitted_expr(TranspilerCtx *ctx,
                                                const char *slot_expr,
                                                const char **slot_name_io,
                                                bool *secure_io)
{
    if (ctx == NULL || slot_expr == NULL || slot_name_io == NULL || secure_io == NULL)
        return;
    if (*secure_io)
        return;
    if (!transpiler_c_expr_is_plain_identifier(slot_expr))
        return;
    if (*slot_name_io != NULL && strcmp(*slot_name_io, slot_expr) == 0)
        return;
    if (lookup_slot_is_secure(ctx, slot_expr)) {
        *slot_name_io = slot_expr;
        *secure_io = true;
        return;
    }
    const char *type_name = lookup_typed_var(ctx, slot_expr);
    if (pgy_codegen_type_name_is_secure_slot(type_name)) {
        *slot_name_io = slot_expr;
        *secure_io = true;
    }
}

bool
transpiler_resolve_slot_target_copy(TranspilerCtx *ctx,
                                    ASTNode *slot_arg,
                                    char *inner_out,
                                    size_t inner_out_size,
                                    const char **slot_name_out,
                                    bool *secure_out)
{
    const char *slot_name = NULL;
    bool secure = false;

    if (inner_out == NULL || inner_out_size == 0)
        return false;
    inner_out[0] = '\0';

    if (slot_arg == NULL)
        return false;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *id = ast_identifier_name(slot_arg);
        TypedVarEntry *entry = lookup_typed_entry(ctx, id);
        if (entry != NULL && (entry->is_view || entry->is_move_token)
            && entry->source_slot[0] != '\0') {
            slot_name = entry->source_slot;
            secure = entry->source_secure || lookup_slot_is_secure(ctx, entry->source_slot);
            if (!secure) {
                const char *source_type = lookup_typed_var(ctx, entry->source_slot);
                if (source_type != NULL
                    && (strcmp(source_type, "SecureSlot") == 0
                        || strncmp(source_type, "SecureSlot<", 11) == 0)) {
                    secure = true;
                }
            }
            (void)slot_inner_type_name_copy(entry->type_name,
                inner_out,
                inner_out_size);
        } else {
            slot_name = id;
            (void)lookup_slot_type_copy(ctx, id, inner_out, inner_out_size);
            secure = lookup_slot_is_secure(ctx, id);
        }
    } else if (slot_arg->type == AST_CALL
               && ast_call_callee(slot_arg) != NULL
               && ast_call_callee(slot_arg)->type == AST_IDENTIFIER
               && ast_call_arg_count(slot_arg) >= 1
               && ast_call_argument(slot_arg, 0) != NULL
               && ast_call_argument(slot_arg, 0)->type == AST_IDENTIFIER) {
        const char *callee = ast_identifier_name(ast_call_callee(slot_arg));
        const char *src = ast_identifier_name(ast_call_argument(slot_arg, 0));
        if (pgy_codegen_call_name_is_slot_source(callee)) {
            slot_name = src;
            (void)lookup_slot_type_copy(ctx, src, inner_out, inner_out_size);
            secure = lookup_slot_is_secure(ctx, src);
        }
    }

    if (inner_out[0] == '\0') {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot determine slot payload type for '%s'",
            slot_name != NULL ? slot_name : "<slot>");
        return false;
    }
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (secure_out != NULL)
        *secure_out = secure;
    return slot_name != NULL;
}

bool
transpiler_resolve_device_slot_inner_copy_or_error(TranspilerCtx *ctx,
                                                   ASTNode *slot_arg,
                                                   const char *operation,
                                                   char *inner_out,
                                                   size_t inner_out_size)
{
    const char *type_name = NULL;

    if (inner_out == NULL || inner_out_size == 0)
        return false;
    inner_out[0] = '\0';

    if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
        type_name = lookup_typed_var(ctx, ast_identifier_name(slot_arg));
    if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
        (void)slot_inner_type_name_copy(type_name, inner_out, inner_out_size);

    if (inner_out[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: %s requires concrete DeviceSlot<T> metadata",
            operation != NULL ? operation : "DeviceSlot operation");
        return false;
    }
    return true;
}
