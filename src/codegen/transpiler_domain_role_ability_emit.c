#include "transpiler_domain_role_ability_emit.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_role_ability_names.h"
#include "transpiler_format.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

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

        rendered = render_type_name_in_ctx(ctx, actual_type);
        if (rendered == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot render generic ability binding '%s' for ability '%s'",
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

char *
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
        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings,
            &binding_count);
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
        rendered = render_type_name_in_ctx(ctx, ability_ref);
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

bool
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
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot render ability vtable tag for ability reference");
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

void
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
    if (ast_generic_param_count(ability_generics) == 0)
        return;

    tag = render_effective_ability_ref_vtable_tag(ability_decl, ability_ref, ctx);
    if (tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot render ability vtable tag for ability '%s'",
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

    if (!ability_ref_vtable_typedef_name(ability_ref, typedef_name,
            sizeof(typedef_name), ctx)) {
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

        if (method == NULL || method->type != AST_FUNC_DECL
            || method_name == NULL) {
            continue;
        }

        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings,
            &binding_count);
        if (ctx != NULL && ctx->backend_error != NULL) {
            free(tag);
            return;
        }
        if (ast_func_return_type(method) != NULL) {
            ret_name = transpiler_render_type_name_with_bindings(ctx,
                ast_func_return_type(method), bindings, binding_count);
            if (transpiler_require_type_name_c_type_copy(ctx, ret_name,
                    "ability vtable return", ret_type_buf,
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
            codebuf_write(target, ", %s%s %s", pt,
                          pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(target, ");\n");
        free(ret_name);
    }

    codebuf_write(target, "} %s;\n", typedef_name);
    free(tag);
}
