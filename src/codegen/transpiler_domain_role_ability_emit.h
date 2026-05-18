#ifndef PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static bool
transpiler_role_ability_copy_name(char *out, size_t out_size,
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

static bool
transpiler_role_ability_host_method_name(char *out, size_t out_size,
                                         const char *host_name,
                                         const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || host_name == NULL
        || method_name == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s_%s", host_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_role_ability_vtable_typedef_name(char *out, size_t out_size,
                                            const char *tag)
{
    int written;

    if (out == NULL || out_size == 0 || tag == NULL)
        return false;
    written = snprintf(out, out_size, "%s_vtable", tag);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_role_operator_alias_name(char *out, size_t out_size,
                                    const char *suffix,
                                    const char *for_type)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL || for_type == NULL)
        return false;
    written = snprintf(out, out_size, "operator_%s_%s", suffix, for_type);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_role_ability_surface_desc(char *out, size_t out_size,
                                     const char *prefix,
                                     const char *owner_name,
                                     const char *method_name,
                                     const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL)
        return false;
    written = snprintf(out, out_size, "%s '%s.%s(%s)'",
        prefix,
        owner_name != NULL ? owner_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)",
        param_name != NULL ? param_name : "(anonymous)");
    return written >= 0 && (size_t)written < out_size;
}

static void
emit_hosted_methods_from_mir_or_error_local(const char *host_name,
                                            const char *anonymous_host_name,
                                            const char *host_kind,
                                            const TranspilerHostedMethodView *method_view,
                                            TranspilerCtx *ctx)
{
    size_t method_count = method_view != NULL ? method_view->count : 0;

    if (transpiler_hosted_method_view_missing_mir_metadata(method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for %s methods '%s'",
            host_kind != NULL ? host_kind : "host",
            host_name != NULL ? host_name : anonymous_host_name);
        return;
    }

    for (size_t i = 0; i < method_count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        const MIRRoutine *mir_method = NULL;
        const char *method_name = NULL;
        char emitted_name[256];

        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        method_name = transpiler_mir_decl_method_name(method_meta);
        if (method_name == NULL)
            method_name = ast_declaration_name(method);

        mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
        if (mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for %s method '%s.%s'",
                host_kind != NULL ? host_kind : "host",
                host_name != NULL ? host_name : anonymous_host_name,
                method_name != NULL
                    ? method_name
                    : "(anonymous)");
            return;
        }

        if (!transpiler_role_ability_host_method_name(
                emitted_name, sizeof(emitted_name), host_name, method_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: hosted method symbol name is too long for %s.%s",
                host_name != NULL ? host_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
        emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }
}

static void
build_ability_ref_bindings(ASTNode *ability_decl,
                           ASTNode *ability_ref,
                           TranspilerCtx *ctx,
                           GenericBindingEntry *bindings,
                           size_t *binding_count)
{
    size_t out = 0;
    GenericParams *ability_generics = ast_ability_generic_params(ability_decl);

    if (binding_count != NULL)
        *binding_count = 0;
    if (ability_decl == NULL || ability_ref == NULL
        || ability_decl->type != AST_ABILITY_DECL
        || ability_ref->type != AST_TYPE
        || ability_generics == NULL) {
        return;
    }

    size_t ability_generic_count = ast_generic_param_count(ability_generics);
    GenericParams *actual_args = ast_type_generic_args(ability_ref);
    for (size_t i = 0;
         i < ability_generic_count && out < MAX_GENERIC_BINDINGS;
         i++) {
        GenericParam *formal = ast_generic_param_at(ability_generics, i);
        GenericParam *actual = NULL;
        ASTNode *actual_type = NULL;
        char *rendered = NULL;

        if (ast_generic_param_name(formal) == NULL)
            continue;
        actual = ast_generic_param_at(actual_args, i);
        actual_type = ast_generic_param_constraint(actual);
        if (actual_type == NULL)
            actual_type = ast_generic_param_default_type(formal);
        if (actual_type == NULL)
            continue;

        rendered = render_type_name(actual_type);
        if (rendered == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render generic ability binding '%s' for ability '%s'",
                ast_generic_param_name(formal) != NULL
                    ? ast_generic_param_name(formal) : "<param>",
                ast_ability_name(ability_decl) != NULL
                    ? ast_ability_name(ability_decl) : "<ability>");
            return;
        }
        if (!transpiler_role_ability_copy_name(
                bindings[out].name, sizeof(bindings[out].name),
                ast_generic_param_name(formal))
            || !transpiler_role_ability_copy_name(
                bindings[out].concrete_type,
                sizeof(bindings[out].concrete_type), rendered)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generic ability binding name is too long for ability '%s'",
                ast_ability_name(ability_decl) != NULL
                    ? ast_ability_name(ability_decl) : "<ability>");
            free(rendered);
            return;
        }
        free(rendered);
        out++;
    }

    if (binding_count != NULL)
        *binding_count = out;
}

static char *
render_effective_ability_ref_vtable_tag(ASTNode *ability_decl,
                                        ASTNode *ability_ref,
                                        TranspilerCtx *ctx)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;
    char *rendered = NULL;
    char suffix[128];
    size_t len;
    GenericParams *ability_generics = ast_ability_generic_params(ability_decl);

    if (ability_ref == NULL)
        return NULL;

    if (ability_decl != NULL && ability_decl->type == AST_ABILITY_DECL
        && ability_generics != NULL
        && ast_generic_param_count(ability_generics) > 0) {
        CodeBuf *buf = codebuf_create();
        if (buf == NULL)
            return NULL;
        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings, &binding_count);
        if (ctx != NULL && ctx->backend_error != NULL) {
            codebuf_destroy(buf);
            return NULL;
        }
        codebuf_write(buf, "%s", ast_type_name(ability_ref) != NULL
                               ? ast_type_name(ability_ref)
                               : "Ability");
        if (binding_count > 0) {
            codebuf_write(buf, "<");
            for (size_t i = 0; i < binding_count; i++) {
                if (i > 0)
                    codebuf_write(buf, ", ");
                codebuf_write(buf, "%s", bindings[i].concrete_type);
            }
            codebuf_write(buf, ">");
        }
        rendered = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
    } else {
        rendered = render_type_name(ability_ref);
    }

    if (rendered == NULL)
        return NULL;
    sanitize_c_suffix(rendered, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    free(rendered);
    if (len == 0)
        return NULL;
    return pergyra_strdup(suffix);
}

static bool
ability_ref_vtable_typedef_name(ASTNode *ability_ref,
                                char *buf,
                                size_t buf_size,
                                TranspilerCtx *ctx)
{
    char *tag;
    ASTNode *ability_decl = NULL;

    if (buf == NULL || buf_size == 0)
        return false;

    if (ctx != NULL && ability_ref != NULL && ability_ref->type == AST_TYPE)
        ability_decl = find_ability_decl(ctx, ast_type_name(ability_ref));
    tag = render_effective_ability_ref_vtable_tag(ability_decl, ability_ref, ctx);
    if (tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for ability reference");
        return false;
    }
    if (!transpiler_role_ability_vtable_typedef_name(buf, buf_size, tag)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: ability vtable typedef name is too long");
        free(tag);
        return false;
    }
    free(tag);
    return true;
}

static void
ensure_ability_ref_vtable_decl(ASTNode *ability_ref, TranspilerCtx *ctx)
{
    ASTNode *ability_decl;
    const char *ability_name;
    char typedef_name[128];
    char *tag = NULL;
    bool already_emitted = false;
    CodeBuf *target;
    GenericParams *ability_generics = NULL;

    if (ctx == NULL || ability_ref == NULL
        || ability_ref->type != AST_TYPE || ast_type_name(ability_ref) == NULL) {
        return;
    }
    target = ctx->out != NULL ? ctx->out : ctx->decls;
    if (target == NULL)
        return;

    ability_name = ast_type_name(ability_ref);
    ability_decl = find_ability_decl(ctx, ability_name);
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL)
        return;

    ability_generics = ast_ability_generic_params(ability_decl);
    if (ast_generic_param_count(ability_generics) == 0) {
        return;
    }

    tag = render_effective_ability_ref_vtable_tag(ability_decl, ability_ref, ctx);
    if (tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for ability '%s'",
            ability_name != NULL ? ability_name : "<ability>");
        return;
    }
    for (int i = 0; i < ctx->ability_vtable_spec_count; i++) {
        if (strcmp(ctx->ability_vtable_specs[i].name, tag) == 0) {
            already_emitted = true;
            break;
        }
    }
    if (already_emitted) {
        free(tag);
        return;
    }

    if (ctx->ability_vtable_spec_count < MAX_ABILITY_VTABLE_SPECIALIZATIONS) {
        if (!transpiler_role_ability_copy_name(
                ctx->ability_vtable_specs[ctx->ability_vtable_spec_count].name,
                sizeof(ctx->ability_vtable_specs[0].name), tag)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: ability vtable specialization name is too long");
            free(tag);
            return;
        }
        ctx->ability_vtable_spec_count++;
    }

    if (!ability_ref_vtable_typedef_name(ability_ref, typedef_name, sizeof(typedef_name), ctx)) {
        free(tag);
        return;
    }
    codebuf_write(target, "\ntypedef struct\n{\n");

    for (size_t i = 0; i < ast_ability_method_count(ability_decl); i++) {
        ASTNode *method = ast_ability_method(ability_decl, i);
        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
        size_t binding_count = 0;
        char *ret_name = NULL;
        char ret_type_buf[256];
        const char *ret_type = "void";
        const char *method_name = ast_declaration_name(method);

        if (method == NULL || method->type != AST_FUNC_DECL || method_name == NULL)
            continue;

        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings, &binding_count);
        if (ctx != NULL && ctx->backend_error != NULL) {
            free(tag);
            return;
        }
        if (ast_func_return_type(method) != NULL) {
            ret_name = transpiler_render_type_name_with_bindings(ctx,
                ast_func_return_type(method), bindings, binding_count);
            if (pergyra_type_to_c_copy(ret_name, ret_type_buf,
                    sizeof(ret_type_buf))) {
                ret_type = ret_type_buf;
            }
        }

        codebuf_write(target, "    %s (*%s)(void *self",
            ret_type, method_name);

        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            char *param_name = NULL;
            char pt_buf[256];
            const char *pt = NULL;
            bool pointer_param = false;
            char surface_desc[256];
            if (p == NULL)
                continue;
            if (p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            if (p->type != NULL) {
                param_name = transpiler_render_type_name_with_bindings(
                    ctx, p->type, bindings, binding_count);
                pointer_param = param_name != NULL
                    && is_pointer_self_host_type_name(ctx, param_name);
            }
            if (!transpiler_role_ability_surface_desc(surface_desc,
                    sizeof(surface_desc), "ability vtable parameter",
                    ability_name, method_name, p->name)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: ability vtable parameter diagnostic is too long");
                free(param_name);
                free(ret_name);
                free(tag);
                return;
            }
            if (transpiler_require_type_name_c_type_copy(ctx, param_name,
                    surface_desc, pt_buf, sizeof(pt_buf))) {
                pt = pt_buf;
            }
            if (pt == NULL) {
                free(param_name);
                free(ret_name);
                free(tag);
                return;
            }
            codebuf_write(target, ", %s%s %s", pt, pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(target, ");\n");
        free(ret_name);
    }

    codebuf_write(target, "} %s;\n", typedef_name);
    free(tag);
}

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H */
