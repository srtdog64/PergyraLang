#include "transpiler_let_slot_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

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
    GenericParams *generic_args;

    if (ann == NULL || ann->type != AST_TYPE || ast_type_name(ann) == NULL)
        return NULL;
    generic_args = ast_type_generic_args(ann);
    if (ast_generic_param_count(generic_args) > 0) {
        GenericParam *param = ast_generic_param_at(generic_args, 0);
        ASTNode *constraint = ast_generic_param_constraint(param);
        if (constraint != NULL && constraint->type == AST_TYPE) {
            return transpiler_let_slot_keep_inner(ctx,
                render_type_name_in_ctx(ctx, constraint));
        }
        if (ast_generic_param_name(param) == NULL)
            return NULL;
        if (ctx == NULL)
            return ast_generic_param_name(param);
        return pgy_arena_strdup(&ctx->arena, ast_generic_param_name(param));
    }
    {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(ast_type_name(ann), inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (ctx == NULL)
            return inner != NULL ? pergyra_strdup(inner) : NULL;
        if (inner == NULL)
            return NULL;
        return pgy_arena_strdup(&ctx->arena, inner);
    }
}

const char *
transpiler_let_slot_inner_from_call_type_arg(TranspilerCtx *ctx, ASTNode *call)
{
    GenericParam *param;

    if (call == NULL || call->type != AST_CALL
        || ast_call_generic_arg_count(call) < 1
        || ast_call_generic_arg(call, 0) == NULL) {
        return NULL;
    }
    param = ast_call_generic_arg(call, 0);
    ASTNode *constraint = ast_generic_param_constraint(param);
    if (constraint != NULL && constraint->type == AST_TYPE) {
        return transpiler_let_slot_keep_inner(ctx,
            render_type_name_in_ctx(ctx, constraint));
    }
    if (ast_generic_param_name(param) == NULL)
        return NULL;
    if (ctx == NULL)
        return ast_generic_param_name(param);
    return pgy_arena_strdup(&ctx->arena, ast_generic_param_name(param));
}

bool
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
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee_name = ast_identifier_name(ast_call_callee(init));
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

bool
transpiler_try_emit_let_slot_view_or_move(TranspilerCtx *ctx,
                                         const char *name,
                                         ASTNode *init,
                                         ASTNode *ann,
                                         char **ann_type_name_io)
{
    if (ann == NULL || ann->type != AST_TYPE || ast_type_name(ann) == NULL)
        return false;

    const char *ann_name = ast_type_name(ann);
    if (!(pgy_codegen_type_name_is_view(ann_name)
        || strcmp(ann_name, "MoveToken") == 0
        || strncmp(ann_name, "MoveToken<", 10) == 0)
        || init == NULL || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER
        || ast_call_arg_count(init) < 1
        || ast_call_argument(init, 0) == NULL
        || ast_call_argument(init, 0)->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee_name = ast_identifier_name(ast_call_callee(init));
    const char *source_name = ast_identifier_name(ast_call_argument(init, 0));
    bool is_view_decl = pgy_codegen_call_name_is_view_constructor(callee_name);
    bool is_move_decl = pgy_codegen_call_name_is_move(callee_name);
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
        if (transpiler_require_type_name_c_type_copy(ctx, secure_name,
                "secure slot view declaration", ctype_buf, sizeof(ctype_buf)))
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
        if (transpiler_require_type_name_c_type_copy(ctx, slot_name_buf,
                "slot view declaration", ctype_buf, sizeof(ctype_buf)))
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

bool
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
    ann_name = ast_type_name(ann);
    if (ann_name == NULL)
        return false;
    if (pgy_codegen_type_name_is_slot(ann_name)) {
        is_slot_sugar = true;
    } else if (pgy_codegen_type_name_is_secure_slot(ann_name)) {
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
        const char *init_name = ast_identifier_name(init);
        TypedVarEntry *move_entry = lookup_typed_entry(ctx, init_name);
        if (move_entry != NULL && move_entry->is_move_token) {
            write_indent(ctx);
            if (sugar_secure) {
                codebuf_write(ctx->out, "PgySecureSlot_%s %s = %s;\n",
                    sugar_inner, name, init_name);
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                    sugar_inner, name, init_name);
            } else {
                codebuf_write(ctx->out, "PgySlot_%s %s = %s;\n",
                    sugar_inner, name, init_name);
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
