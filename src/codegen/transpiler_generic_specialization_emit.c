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
#include "transpiler_generic_param_query.h"
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

/* MIR-only discipline: generic specialization emission consumes MIR
 * signature metadata; a generic decl without it must fail loudly, never
 * fall back to AST-shape emission. (The former nested-param signature
 * guard lived here until G-2 opened param position — binding inference
 * now performs structural matching, so the guard's job moved to the
 * post-inference diagnostic in ensure_generic_specialization.) */
static bool
transpiler_mir_generic_metadata_present(TranspilerCtx *ctx, ASTNode *decl)
{
    const char *decl_name;
    const char *diagnostic_name;
    const MIRDeclHeader *header;
    const MIRRoutine *routine;
    bool mir_active;

    if (ctx == NULL || decl == NULL)
        return false;

    mir_active = transpiler_active_has_mir(ctx);
    routine = NULL;
    decl_name = NULL;
    header = NULL;
    if (mir_active) {
        routine = transpiler_find_mir_function(ctx, decl);
        decl_name = routine != NULL
            ? transpiler_mir_routine_name(routine)
            : NULL;
        header = decl_name != NULL
            ? transpiler_active_decl_header_of_type(ctx, AST_FUNC_DECL, decl_name)
            : NULL;
    }

    if (mir_active && header != NULL && routine != NULL
        && transpiler_mir_routine_has_signature(routine))
        return true;

    diagnostic_name = decl_name != NULL
        ? decl_name
        : ast_declaration_name(decl);
    transpiler_set_mir_inventory_missing(
        ctx,
        "MIR-only C path missing generic function specialization metadata for '%s'",
        diagnostic_name != NULL ? diagnostic_name : "(anonymous)");
    return false;
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
    /* Non-generic decls simply don't specialize (callers rely on NULL to
     * mean "use the original name"). Everything past this point is a
     * genuine generic call site, where silence is forbidden. */
    if (!transpiler_func_has_generic_params(decl))
        return NULL;

    if (!transpiler_mir_generic_metadata_present(ctx, decl))
        return NULL;

    /* G-2: binding inference performs structural matching (bare T and
     * constructed-over-T params) with unification. A failure here means
     * the call site cannot bind every type parameter (conflict or unbound
     * without a default) -- fail closed with a diagnostic instead of the
     * old silent raw-name fallback that died at the native stage. */
    if (!transpiler_infer_generic_call_bindings(ctx, decl, call, bindings,
            &binding_count)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: cannot bind generic parameter(s) of '%s' from the call site -- argument types conflict or leave a parameter unbound (bind it via an argument or a default type argument)",
            decl_name);
        return NULL;
    }

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
