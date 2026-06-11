#include "transpiler_generic_class_specialization.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_generic_class_naming.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_func_emit.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

typedef struct TranspilerGenericClassSpecSnapshot {
    int class_spec_count;
    size_t helpers_len;
    TranspilerGenericBindingSnapshot generic_binding;
} TranspilerGenericClassSpecSnapshot;

static TranspilerGenericClassSpecSnapshot
transpiler_generic_class_spec_snapshot(TranspilerCtx *ctx)
{
    TranspilerGenericClassSpecSnapshot snapshot;

    snapshot.class_spec_count =
        ctx != NULL ? ctx->generic_class_spec_count : 0;
    snapshot.helpers_len =
        ctx != NULL && ctx->helpers != NULL ? ctx->helpers->len : 0;
    snapshot.generic_binding = transpiler_generic_binding_snapshot(ctx);
    return snapshot;
}

static void
transpiler_generic_class_spec_rollback(
    TranspilerCtx *ctx,
    TranspilerGenericClassSpecSnapshot snapshot)
{
    if (ctx == NULL)
        return;

    ctx->generic_class_spec_count = snapshot.class_spec_count;
    codebuf_truncate(ctx->helpers, snapshot.helpers_len);
    transpiler_generic_binding_restore(ctx, snapshot.generic_binding);
}

static void
transpiler_generic_class_spec_commit(
    TranspilerCtx *ctx,
    TranspilerGenericClassSpecSnapshot snapshot)
{
    transpiler_generic_binding_restore(ctx, snapshot.generic_binding);
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name. The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = ast_declaration_generic_params(class_decl);
    GenericParams *ga = ast_type_generic_args(ann);
    bool has_effective_args = false;
    const char *base_class_name = transpiler_decl_name_local(class_decl);

    if (gp == NULL)
        return base_class_name;

    char *specialization_name = transpiler_generic_class_specialization_name(
        ctx, class_decl, ann, &has_effective_args);
    size_t formal_count = ast_generic_param_count(gp);

    if (specialization_name == NULL && has_effective_args) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: cannot allocate generic class specialization name for '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous)");
        return NULL;
    }

    if (!has_effective_args) {
        free(specialization_name);
        return base_class_name;
    }

    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name,
                specialization_name) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            free(specialization_name);
            return result;
        }
    }

    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend generic class specialization registry exceeded MAX_GENERIC_CLASS_SPECIALIZATIONS while lowering '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous)");
        free(specialization_name);
        return NULL;
    }
    if (formal_count > MAX_GENERIC_BINDINGS
        || ctx->generic_binding_count > (int)(MAX_GENERIC_BINDINGS - formal_count)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend generic binding registry exceeded MAX_GENERIC_BINDINGS while lowering class '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous)");
        free(specialization_name);
        return NULL;
    }

    TranspilerGenericClassSpecSnapshot spec_snapshot =
        transpiler_generic_class_spec_snapshot(ctx);
    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    if (!transpiler_generic_class_copy_name(
            entry->specialized_name, sizeof(entry->specialized_name),
            specialization_name)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: generic class specialization name is too long for '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous)");
        transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
        free(specialization_name);
        return NULL;
    }
    free(specialization_name);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    for (size_t i = 0; i < formal_count; i++) {
        GenericParam *formal = ast_generic_param_at(gp, i);
        GenericParam *garg = ast_generic_param_at(ga, i);
        char *effective_name = NULL;

        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        if (!transpiler_generic_class_copy_name(
                b->name, sizeof(b->name),
                ast_generic_param_name(formal) != NULL
                    ? ast_generic_param_name(formal) : "T")) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generic class binding name is too long");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        effective_name = transpiler_generic_param_effective_arg_name_in_ctx(
            ctx, formal, garg);

        if (effective_name != NULL) {
            if (!transpiler_generic_class_copy_name(
                    b->concrete_type, sizeof(b->concrete_type),
                    effective_name)) {
                free(effective_name);
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: generic class concrete binding is too long for specialization '%s'",
                    spec_name != NULL ? spec_name : "(anonymous)");
                transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
                return NULL;
            }
            free(effective_name);
        } else {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot resolve generic class binding '%s' for specialization '%s'",
                ast_generic_param_name(formal) != NULL
                    ? ast_generic_param_name(formal) : "(anonymous)",
                base_class_name != NULL ? base_class_name : "(anonymous)");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }

        entry->bindings[i] = *b;
    }
    entry->binding_count = formal_count;

    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    TranspilerHostedFieldView field_view =
        transpiler_hosted_class_field_view_from_decl(ctx, base_class_name,
            class_decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing field declaration metadata for generic class '%s' specialization '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous-class)",
            spec_name != NULL ? spec_name : "(anonymous-specialization)");
        transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
        return NULL;
    }
    for (size_t i = 0; i < field_view.count; i++) {
        const char *field_name =
            transpiler_hosted_field_view_name(&field_view, i);
        ASTNode *field_type =
            transpiler_hosted_field_view_type(&field_view, i);
        char ft[256];
        char surface_desc[256];
        if (field_name == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: generic class '%s' field[%zu] is missing declaration field metadata",
                base_class_name != NULL ? base_class_name : "(anonymous-class)",
                i);
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        if (!transpiler_generic_class_surface_desc(
                surface_desc, sizeof(surface_desc),
                "generic class field",
                spec_name,
                field_name != NULL ? field_name : "(anonymous)",
                NULL)) {
            transpiler_generic_class_format_too_long(
                ctx, "generic class field diagnostic surface");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        if (!transpiler_require_ast_c_type_copy(ctx,
                field_type,
                surface_desc,
                ft,
                sizeof(ft))) {
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, field_name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    codebuf_write(ctx->helpers,
        "\n#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, base_class_name,
            class_decl);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for generic class '%s' specialization '%s'",
            base_class_name != NULL ? base_class_name : "(anonymous-class)",
            spec_name != NULL ? spec_name : "(anonymous-specialization)");
        transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
        return NULL;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for generic class specialization '%s'",
            spec_name != NULL ? spec_name : "(anonymous-specialization)")) {
        transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
        return NULL;
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for generic class '%s'",
                spec_name != NULL ? spec_name : "(anonymous-generic-class)");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing method body metadata row for generic class '%s'",
                spec_name != NULL ? spec_name : "(anonymous-generic-class)");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(spec_name, method_meta,
            method, use_self_cell, ctx->helpers, ctx);
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        const char *method_name;
        const MIRRoutine *mir_method;
        method_name = transpiler_mir_decl_method_name(method_meta);
        mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
        if (method == NULL && mir_method != NULL)
            method = transpiler_mir_routine_source_ast_of_type(
                mir_method, MIR_SCOPE_METHOD, AST_FUNC_DECL);
        if (method_name == NULL && method != NULL)
            method_name = ast_declaration_name(method);
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }

        if (transpiler_active_has_mir(ctx) && mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for generic class method '%s.%s' specialization '%s'",
                base_class_name != NULL
                    ? base_class_name
                    : "(anonymous-class)",
                method_name != NULL ? method_name : "(anonymous)",
                spec_name != NULL ? spec_name : "(anonymous-specialization)");
            transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
            return NULL;
        }

        if (mir_method != NULL) {
            char emitted_name[256];
            if (!transpiler_generic_class_method_name(
                    emitted_name, sizeof(emitted_name), spec_name,
                    method_name)) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: generic class method symbol is too long for '%s.%s'",
                    spec_name != NULL ? spec_name : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)");
                transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
                return NULL;
            }
            ctx->active_generic_class_base_name = base_class_name;
            ctx->active_generic_class_spec_name = spec_name;
            emit_func_decl_from_mir_named(method, mir_method, emitted_name,
                ctx->helpers, ctx);
            ctx->active_generic_class_base_name = NULL;
            ctx->active_generic_class_spec_name = NULL;
            continue;
        }

        char ret_type_buf[256];
        const char *ret_type = "void";
        if (ast_func_return_type(method) != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx,
                ast_func_return_type(method),
                ret_type_buf,
                sizeof(ret_type_buf))) {
            ret_type = ret_type_buf;
        }

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
        }

        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            char pt[256];
            char surface_desc[256];
            if (!transpiler_generic_class_surface_desc(
                    surface_desc, sizeof(surface_desc),
                    "generic class method parameter",
                    spec_name,
                    method_name,
                    p != NULL && p->name != NULL ? p->name : "(anonymous)")) {
                transpiler_generic_class_format_too_long(
                    ctx, "generic class method parameter diagnostic surface");
                transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
                return NULL;
            }
            if (!transpiler_require_ast_c_type_copy(ctx,
                    p != NULL ? p->type : NULL,
                    surface_desc,
                    pt,
                    sizeof(pt))) {
                transpiler_generic_class_spec_rollback(ctx, spec_snapshot);
                return NULL;
            }
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        transpiler_emit_host_method_body_local(
            ctx, class_decl, spec_name, method, ctx->helpers, false);

        codebuf_write(ctx->helpers, "}\n");
    }

    transpiler_generic_class_spec_commit(ctx, spec_snapshot);

    return spec_name;
}

ASTNode *
transpiler_generic_class_spec_base_decl(const TranspilerCtx *ctx,
                                        const char *specialized_name)
{
    if (ctx == NULL || specialized_name == NULL)
        return NULL;

    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name,
                specialized_name) == 0)
            return (ASTNode *)ctx->generic_class_specs[i].class_decl;
    }
    return NULL;
}
