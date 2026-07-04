/*
 * Copyright (c) 2026 Pergyra Language Project
 * Generic function specialization emission for the C backend.
 */

#include "transpiler_generic_specialization_emit.h"

#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"
#include "transpiler_func_forward_helpers.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_mangled_name.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_inventory_view.h"

static bool
transpiler_generic_specialization_copy_name(char *out, size_t out_size,
                                            const char *name)
{
    size_t len;

    if (out == NULL || out_size == 0 || name == NULL)
        return false;
    len = strlen(name);
    if (len >= out_size)
        return false;
    memcpy(out, name, len + 1);
    return true;
}

static void
transpiler_generic_specialization_name_too_long(TranspilerCtx *ctx,
                                                const char *decl_name)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: generic function specialization name is too long for '%s'",
        decl_name != NULL ? decl_name : "(anonymous)");
}

/* Current C specialization substitutes bare type parameters only; a type
 * parameter nested inside a constructed type (Option<T>, Array<T>, ...)
 * would be rendered literally and produce broken C source. Detect that
 * shape in the signature so the call fails closed with a diagnostic,
 * mirroring the LLVM backend's concrete-metadata error. Returns the
 * offending type-parameter name, or NULL when the signature is safe. */
static const char *
transpiler_type_nested_generic_param(const ASTNode *type,
                                     GenericParams *gparams,
                                     bool at_top_level)
{
    GenericParams *args;
    size_t count;

    if (type == NULL || gparams == NULL)
        return NULL;
    if (!at_top_level) {
        const char *tname = ast_type_name(type);
        size_t gcount = ast_generic_param_count(gparams);
        for (size_t i = 0; i < gcount; i++) {
            const char *gname = ast_generic_param_name(
                ast_generic_param_at(gparams, i));
            if (gname != NULL && tname != NULL
                && strcmp(gname, tname) == 0)
                return gname;
        }
    }
    args = ast_type_generic_args(type);
    if (args == NULL)
        return NULL;
    count = ast_generic_param_count(args);
    for (size_t i = 0; i < count; i++) {
        ASTNode *arg_type = ast_generic_param_constraint(
            ast_generic_param_at(args, i));
        const char *hit = transpiler_type_nested_generic_param(
            arg_type, gparams, false);
        if (hit != NULL)
            return hit;
    }
    return NULL;
}

static bool
transpiler_type_name_boundary(char ch)
{
    return !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
             || (ch >= '0' && ch <= '9') || ch == '_');
}

static bool
transpiler_type_name_angle_arg_mentions_generic_param(const char *type_name,
                                                      const char *param_name)
{
    size_t param_len;
    int depth = 0;

    if (type_name == NULL || param_name == NULL || param_name[0] == '\0')
        return false;

    param_len = strlen(param_name);
    for (const char *p = type_name; *p != '\0'; p++) {
        if (*p == '<') {
            depth++;
            continue;
        }
        if (*p == '>') {
            if (depth > 0)
                depth--;
            continue;
        }
        if (depth <= 0 || strncmp(p, param_name, param_len) != 0)
            continue;
        {
            char before = p == type_name ? '\0' : p[-1];
            char after = p[param_len];
            if ((p == type_name || transpiler_type_name_boundary(before))
                && (after == '\0' || transpiler_type_name_boundary(after)))
                return true;
        }
    }
    return false;
}

static const char *
transpiler_mir_type_name_nested_generic_param(
    const char *type_name,
    const MIRDeclHeader *header)
{
    size_t generic_count;

    if (type_name == NULL || header == NULL)
        return NULL;

    generic_count = mir_decl_header_generic_param_count(header);
    for (size_t i = 0; i < generic_count; i++) {
        const MIRDeclGenericParam *param =
            mir_decl_header_generic_param(header, i);
        const char *param_name = mir_decl_generic_param_name(param);
        if (transpiler_type_name_angle_arg_mentions_generic_param(
                type_name, param_name)) {
            return param_name;
        }
    }
    return NULL;
}

static bool
transpiler_mir_signature_nested_param(TranspilerCtx *ctx,
                                      ASTNode *decl,
                                      const char **nested_out)
{
    const char *decl_name;
    const char *diagnostic_name;
    const MIRDeclHeader *header;
    const MIRRoutine *routine;
    const char *nested;

    if (nested_out != NULL)
        *nested_out = NULL;
    if (ctx == NULL || decl == NULL || nested_out == NULL)
        return true;

    routine = transpiler_find_mir_function(ctx, decl);
    decl_name = routine != NULL
        ? transpiler_mir_routine_name(routine)
        : NULL;
    header = decl_name != NULL
        ? transpiler_active_decl_header_of_type(ctx, AST_FUNC_DECL, decl_name)
        : NULL;

    if (header == NULL || routine == NULL
        || !transpiler_mir_routine_has_signature(routine)) {
        diagnostic_name = decl_name != NULL
            ? decl_name
            : ast_declaration_name(decl);
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing generic function specialization metadata for '%s'",
            diagnostic_name != NULL ? diagnostic_name : "(anonymous)");
        return false;
    }

    /* G-1 (docs/151 §8): constructed-over-T is OPEN in return position —
     * emission substitutes bindings through the type-require and
     * expr-infer choke points. PARAM position stays fail-closed: binding
     * inference reads call-site argument types and gives up on
     * constructed-over-T params (G-2 owns that cell). */
    nested = NULL;
    for (size_t i = 0; nested == NULL
         && i < transpiler_mir_routine_param_count(routine); i++) {
        nested = transpiler_mir_type_name_nested_generic_param(
            transpiler_mir_routine_param_type_name(routine, i), header);
    }

    *nested_out = nested;
    return true;
}

static bool
transpiler_generic_signature_nested_param(TranspilerCtx *ctx,
                                          ASTNode *decl,
                                          const char **nested_out)
{
    GenericParams *gparams;
    size_t param_count;
    const char *nested;

    if (nested_out != NULL)
        *nested_out = NULL;
    if (ctx == NULL || decl == NULL || nested_out == NULL)
        return true;

    if (transpiler_active_has_mir(ctx))
        return transpiler_mir_signature_nested_param(ctx, decl, nested_out);

    gparams = ast_declaration_generic_params(decl);
    param_count = ast_func_param_count(decl);
    /* Mirror of the MIR-path G-1 rule: return position open, params guarded. */
    nested = NULL;
    for (size_t i = 0; nested == NULL && i < param_count; i++) {
        FuncParam *param = ast_func_param(decl, i);
        nested = transpiler_type_nested_generic_param(
            param != NULL ? param->type : NULL, gparams, true);
    }
    *nested_out = nested;
    return true;
}

const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;
    const char *decl_name;
    CodeBuf *name_buf;
    GenericSpecializationEntry *entry;
    TranspilerGenericBindingSnapshot generic_binding_snapshot;

    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;

    decl_name = ast_declaration_name(decl);
    if (decl_name == NULL)
        return NULL;
    /* The signature guard needs no bindings, and binding inference itself
     * gives up on constructed-over-T params -- so it must run FIRST, or an
     * inference failure would return NULL before the guard and let the
     * caller fall back to the silent raw-name emission this guard exists
     * to prevent. */
    {
        const char *nested = NULL;
        if (!transpiler_generic_signature_nested_param(ctx, decl, &nested))
            return NULL;
        if (nested != NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generic function '%s' uses type parameter '%s' inside a constructed type; C specialization does not substitute nested type parameters yet -- use a per-type function",
                decl_name, nested);
            return NULL;
        }
    }

    if (!transpiler_infer_generic_call_bindings(ctx, decl, call, bindings,
            &binding_count))
        return NULL;

    name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl_name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            const char *result = entry->specialized_name;
            codebuf_destroy(name_buf);
            return result;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend generic function specialization registry exceeded MAX_GENERIC_SPECIALIZATIONS while lowering '%s'",
            decl_name);
        codebuf_destroy(name_buf);
        return NULL;
    }
    if (binding_count > MAX_GENERIC_BINDINGS
        || ctx->generic_binding_count > (int)(MAX_GENERIC_BINDINGS - binding_count)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend generic binding registry exceeded MAX_GENERIC_BINDINGS while lowering '%s'",
            decl_name);
        codebuf_destroy(name_buf);
        return NULL;
    }

    entry = &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    if (!transpiler_generic_specialization_copy_name(
            entry->specialized_name, sizeof(entry->specialized_name),
            name_buf->data)) {
        ctx->generic_specialization_count--;
        transpiler_generic_specialization_name_too_long(ctx, decl_name);
        codebuf_destroy(name_buf);
        return NULL;
    }
    entry->emitting = true;
    codebuf_destroy(name_buf);

    generic_binding_snapshot = transpiler_generic_binding_snapshot(ctx);
    for (size_t i = 0; i < binding_count; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    transpiler_generic_binding_restore(ctx, generic_binding_snapshot);
    entry->emitting = false;
    return entry->specialized_name;
}
