#ifndef PGY_TRANSPILER_LET_SLOT_EMIT_H
#define PGY_TRANSPILER_LET_SLOT_EMIT_H

/* Slot-related let-declaration lowering helpers.
 * Included inside transpiler.c before transpiler_let_emit.h. */

#include "codegen_slot_type_policy.h"

static bool
transpiler_let_slot_constructed_type_name(char *out, size_t out_size,
                                          const char *family,
                                          const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || family == NULL || inner == NULL)
        return false;

    written = snprintf(out, out_size, "%s<%s>", family, inner);
    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_let_slot_constructed_type_too_long(TranspilerCtx *ctx,
                                              const char *family)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s constructed type is too long for C backend emission",
        family != NULL ? family : "slot");
}

static const char *
transpiler_let_slot_keep_inner(TranspilerCtx *ctx, char *owned_inner)
{
    char *copy;

    if (owned_inner == NULL)
        return NULL;
    if (ctx == NULL)
        return owned_inner;
    copy = pgy_arena_strdup(&ctx->arena, owned_inner);
    free(owned_inner);
    return copy;
}

static const char *
transpiler_let_slot_inner_from_annotation(TranspilerCtx *ctx, ASTNode *ann)
{
    if (ann == NULL || ann->type != AST_TYPE || ann->data.type.name == NULL)
        return NULL;
    if (ann->data.type.generic_args != NULL
        && ann->data.type.generic_args->count > 0
        && ann->data.type.generic_args->params != NULL
        && ann->data.type.generic_args->params[0] != NULL) {
        GenericParam *param = ann->data.type.generic_args->params[0];
        if (param->constraint != NULL
            && param->constraint->type == AST_TYPE) {
            return transpiler_let_slot_keep_inner(ctx,
                render_type_name(param->constraint));
        }
        if (ctx == NULL)
            return param->name;
        return pgy_arena_strdup(&ctx->arena, param->name);
    }
    {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(ann->data.type.name, inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (ctx == NULL)
            return inner != NULL ? pergyra_strdup(inner) : NULL;
        if (inner == NULL)
            return NULL;
        return pgy_arena_strdup(&ctx->arena, inner);
    }
}

static const char *
transpiler_let_slot_inner_from_call_type_arg(TranspilerCtx *ctx, ASTNode *call)
{
    GenericParam *param;

    if (call == NULL || call->type != AST_CALL
        || call->data.call.generic_args == NULL
        || call->data.call.generic_args->count < 1
        || call->data.call.generic_args->params == NULL
        || call->data.call.generic_args->params[0] == NULL) {
        return NULL;
    }
    param = call->data.call.generic_args->params[0];
    if (param->constraint != NULL
        && param->constraint->type == AST_TYPE) {
        return transpiler_let_slot_keep_inner(ctx,
            render_type_name(param->constraint));
    }
    if (ctx == NULL)
        return param->name;
    return pgy_arena_strdup(&ctx->arena, param->name);
}

static bool
transpiler_try_emit_let_slot_claim(ASTNode *node,
                                  TranspilerCtx *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  ASTNode *ann,
                                  char **ann_type_name_io)
{
    bool is_slot = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *slot_inner = NULL;

    if (init == NULL || init->type != AST_CALL
        || init->data.call.callee == NULL
        || init->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee_name = init->data.call.callee->data.identifier.name;
    if (pgy_codegen_call_name_is_claim_slot(callee_name)) {
        is_slot = true;
    } else if (pgy_codegen_call_name_is_claim_secure_slot(callee_name)) {
        is_slot = true;
        is_secure_slot = true;
    } else if (pgy_codegen_call_name_is_claim_device_slot(callee_name)) {
        is_device_slot = true;
    } else {
        return false;
    }

    slot_inner = transpiler_let_slot_inner_from_annotation(ctx, ann);
    if (slot_inner == NULL && is_slot)
        slot_inner = transpiler_let_slot_inner_from_call_type_arg(ctx, init);
    if (slot_inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            is_device_slot
                ? "cannot emit device slot claim for '%s': missing explicit DeviceSlot<T> annotation"
                : "cannot emit slot claim for '%s': missing explicit Slot<T>/SecureSlot<T> annotation or ClaimSlot<T>() type argument",
            name != NULL ? name : "(anonymous)");
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        (void)node;
        return true;
    }

    if (is_slot) {
        register_slot_var(ctx, name, slot_inner, is_secure_slot, false);
        write_indent(ctx);
        if (is_secure_slot) {
            codebuf_write(ctx->out, "PgyToken_%s %s_token;\n", slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                slot_inner, name, slot_inner, name);
        } else {
            codebuf_write(ctx->out,
                "PgySlot_%s %s = pgy_claim_%s();\n",
                slot_inner, name, slot_inner);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", name);
        if (*ann_type_name_io != NULL)
            register_typed_var(ctx, name, *ann_type_name_io);
        else {
            char *slot_type = strdup_fmt("%s<%s>",
                is_secure_slot ? "SecureSlot" : "Slot", slot_inner);
            register_typed_var(ctx, name, slot_type);
            free(slot_type);
        }
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        return true;
    }

    if (is_device_slot) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyDeviceSlot_%s %s = pgy_claim_device_%s();\n",
            slot_inner, name, slot_inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", name);
        if (*ann_type_name_io != NULL) {
            register_typed_var(ctx, name, *ann_type_name_io);
        } else {
            char *device_type = strdup_fmt("DeviceSlot<%s>", slot_inner);
            register_typed_var(ctx, name, device_type);
            free(device_type);
        }
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        return true;
    }

    return false;
}

static bool
transpiler_try_emit_let_slot_view_or_move(TranspilerCtx *ctx,
                                         const char *name,
                                         ASTNode *init,
                                         ASTNode *ann,
                                         char **ann_type_name_io)
{
    if (ann == NULL || ann->type != AST_TYPE || ann->data.type.name == NULL)
        return false;

    const char *ann_name = ann->data.type.name;
    if (!(pgy_codegen_type_name_is_view(ann_name)
          || strcmp(ann_name, "MoveToken") == 0
          || strncmp(ann_name, "MoveToken<", 10) == 0)
        || init == NULL || init->type != AST_CALL
        || init->data.call.callee == NULL
        || init->data.call.callee->type != AST_IDENTIFIER
        || init->data.call.arg_count < 1
        || init->data.call.arguments[0] == NULL
        || init->data.call.arguments[0]->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee_name = init->data.call.callee->data.identifier.name;
    const char *source_name = init->data.call.arguments[0]->data.identifier.name;
    bool is_view_decl = pgy_codegen_call_name_is_view_constructor(callee_name);
    bool is_move_decl = strcmp(callee_name, "Move") == 0;
    if (!is_view_decl && !is_move_decl)
        return false;

    const char *inner = transpiler_let_slot_inner_from_annotation(ctx, ann);
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot emit %s declaration for '%s': missing explicit inner type",
            is_move_decl ? "move-token" : "view",
            name != NULL ? name : "(anonymous)");
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        return true;
    }

    bool source_secure = lookup_slot_is_secure(ctx, source_name);
    char ctype_buf[128];
    const char *ctype = NULL;
    if (source_secure) {
        char secure_name[128];
        if (!transpiler_let_slot_constructed_type_name(secure_name,
                sizeof(secure_name), "SecureSlot", inner)) {
            transpiler_let_slot_constructed_type_too_long(ctx, "SecureSlot");
            free(*ann_type_name_io);
            *ann_type_name_io = NULL;
            return true;
        }
        if (pergyra_type_to_c_copy(secure_name, ctype_buf, sizeof(ctype_buf)))
            ctype = ctype_buf;
    } else {
        char slot_name_buf[128];
        if (!transpiler_let_slot_constructed_type_name(slot_name_buf,
                sizeof(slot_name_buf), "Slot", inner)) {
            transpiler_let_slot_constructed_type_too_long(ctx, "Slot");
            free(*ann_type_name_io);
            *ann_type_name_io = NULL;
            return true;
        }
        if (pergyra_type_to_c_copy(slot_name_buf, ctype_buf, sizeof(ctype_buf)))
            ctype = ctype_buf;
    }
    if (ctype == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot lower %s declaration '%s' with inner type '%s' to a concrete slot type",
            is_move_decl ? "move-token" : "view",
            name != NULL ? name : "(anonymous)",
            inner);
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        return true;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "%s %s = %s;\n", ctype, name, source_name);
    if (source_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
            inner, name, source_name);
    }
    register_view_like_var(ctx, name,
        *ann_type_name_io != NULL ? *ann_type_name_io : ann_name,
        source_name, source_secure, is_move_decl);
    if (is_move_decl) {
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, source_name) == 0) {
                ctx->slot_vars[i].released = true;
                break;
            }
        }
    }
    free(*ann_type_name_io);
    *ann_type_name_io = NULL;
    return true;
}

static bool
transpiler_try_emit_let_slot_sugar(TranspilerCtx *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  ASTNode *ann,
                                  char **ann_type_name_io)
{
    bool is_slot_sugar = false;
    bool sugar_secure = false;
    const char *ann_name = NULL;
    const char *sugar_inner = NULL;

    if (ann == NULL || ann->type != AST_TYPE)
        return false;
    ann_name = ann->data.type.name;
    if (ann_name == NULL)
        return false;
    if (strcmp(ann_name, "Slot") == 0 || strncmp(ann_name, "Slot<", 5) == 0) {
        is_slot_sugar = true;
    } else if (strcmp(ann_name, "SecureSlot") == 0
               || strncmp(ann_name, "SecureSlot<", 11) == 0) {
        is_slot_sugar = true;
        sugar_secure = true;
    }
    if (!is_slot_sugar)
        return false;

    sugar_inner = transpiler_let_slot_inner_from_annotation(ctx, ann);
    if (sugar_inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot emit slot sugar declaration for '%s': missing explicit Slot<T>/SecureSlot<T> inner type",
            name != NULL ? name : "(anonymous)");
        free(*ann_type_name_io);
        *ann_type_name_io = NULL;
        return true;
    }

    if (init != NULL && init->type == AST_IDENTIFIER) {
        TypedVarEntry *move_entry = lookup_typed_entry(ctx, init->data.identifier.name);
        if (move_entry != NULL && move_entry->is_move_token) {
            write_indent(ctx);
            if (sugar_secure) {
                codebuf_write(ctx->out, "PgySecureSlot_%s %s = %s;\n",
                    sugar_inner, name, init->data.identifier.name);
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                    sugar_inner, name, init->data.identifier.name);
            } else {
                codebuf_write(ctx->out, "PgySlot_%s %s = %s;\n",
                    sugar_inner, name, init->data.identifier.name);
            }
            register_slot_var(ctx, name, sugar_inner, sugar_secure, false);
            if (*ann_type_name_io != NULL)
                register_typed_var(ctx, name, *ann_type_name_io);
            free(*ann_type_name_io);
            *ann_type_name_io = NULL;
            return true;
        }
    }

    register_slot_var(ctx, name, sugar_inner, sugar_secure, false);
    write_indent(ctx);
    if (sugar_secure) {
        codebuf_write(ctx->out, "PgyToken_%s %s_token;\n", sugar_inner, name);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            sugar_inner, name, sugar_inner, name);
    } else {
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            sugar_inner, name, sugar_inner);
    }

    if (init != NULL) {
        char *init_expr = emit_expression(init, ctx);
        write_indent(ctx);
        if (sugar_secure) {
            codebuf_write(ctx->out,
                "pgy_secure_write_%s(&%s, %s, &%s_token);\n",
                sugar_inner, name, init_expr, name);
        } else {
            codebuf_write(ctx->out,
                "pgy_write_%s(&%s, %s);\n",
                sugar_inner, name, init_expr);
        }
        free(init_expr);
    }

    if (*ann_type_name_io != NULL)
        register_typed_var(ctx, name, *ann_type_name_io);
    free(*ann_type_name_io);
    *ann_type_name_io = NULL;
    return true;
}

#endif /* PGY_TRANSPILER_LET_SLOT_EMIT_H */
