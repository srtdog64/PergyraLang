/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend hosted-method forward declaration emission.
 */

#include "../common/string_compat.h"
#include <stdio.h>
#include <string.h>

#include "transpiler_func_forward_metadata.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

void
emit_hosted_method_forward_decl_from_metadata(const char *host_name,
                                              const MIRDeclMethod *method_meta,
                                              ASTNode *method,
                                              bool pointer_self,
                                              CodeBuf *buf,
                                              TranspilerCtx *ctx)
{
    const char *method_name;
    const char *return_type_name;
    ASTNode *return_type;
    size_t param_count;
    char ret_type_buf[256];
    const char *ret_type = "void";

    if (host_name == NULL || buf == NULL || ctx == NULL)
        return;
    if (method_meta == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata for '%s'",
                host_name != NULL ? host_name : "(anonymous-host)");
            return;
        }
        if (method == NULL || method->type != AST_FUNC_DECL)
            return;
    }

    method_name = method_meta != NULL
        ? transpiler_mir_decl_method_name(method_meta)
        : NULL;
    return_type_name = method_meta != NULL
        ? transpiler_mir_decl_method_return_type_name(method_meta)
        : NULL;
    return_type = method_meta != NULL
        ? transpiler_mir_decl_method_return_type(method_meta)
        : NULL;
    param_count = method_meta != NULL
        ? transpiler_mir_decl_method_param_count(method_meta)
        : 0;
    if (method_name == NULL && method != NULL)
        method_name = ast_declaration_name(method);
    if (return_type == NULL && method_meta == NULL && method != NULL
        && !transpiler_active_has_mir(ctx))
        return_type = ast_func_return_type(method);
    if (param_count == 0 && method_meta == NULL && method != NULL
        && !transpiler_active_has_mir(ctx))
        param_count = ast_func_param_count(method);
    if (method_name == NULL)
        return;
    if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
            method_meta,
            host_name,
            method_name,
            TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing hosted method forward return type-name metadata for '%s.%s'",
            "MIR-only C path missing hosted method forward parameter type-name metadata for '%s.%s'")) {
        return;
    }
    ensure_type_specializations_from_ast(ctx, return_type);
    if (return_type_name != NULL) {
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "hosted method return '%s.%s'",
            host_name != NULL ? host_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
        if (!transpiler_require_type_name_c_type_copy(ctx, return_type_name,
                surface_desc, ret_type_buf, sizeof(ret_type_buf))) {
            return;
        }
        ret_type = ret_type_buf;
    } else if (return_type != NULL
        && pergyra_ast_type_to_c_copy_in_ctx(ctx, return_type,
            ret_type_buf,
            sizeof(ret_type_buf))) {
        ret_type = ret_type_buf;
    }

    codebuf_write(buf, "\n%s\n%s_%s(%s%s",
                  ret_type, host_name, method_name, host_name,
                  pointer_self ? " *self" : " self");

    for (size_t j = 0; j < param_count; j++) {
        FuncParam *p = transpiler_mir_decl_method_param(method_meta, j);
        const char *param_type_name =
            transpiler_mir_decl_method_param_type_name(method_meta, j);
        char pt[256];
        char surface_desc[256];

        if (p == NULL && method_meta == NULL && method != NULL
            && !transpiler_active_has_mir(ctx))
            p = ast_func_param(method, j);
        if (p == NULL || p->name == NULL)
            continue;
        if (strcmp(p->name, "self") == 0)
            continue;

        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        snprintf(surface_desc, sizeof(surface_desc),
            "hosted method parameter '%s.%s(%s)'",
            host_name != NULL ? host_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)",
            p->name != NULL ? p->name : "(anonymous)");
        if (param_type_name != NULL) {
            if (!transpiler_require_type_name_c_type_copy(ctx,
                    param_type_name, surface_desc, pt, sizeof(pt))) {
                return;
            }
        } else {
            if (!transpiler_require_ast_c_type_copy(ctx,
                    p->type, surface_desc, pt, sizeof(pt))) {
                return;
            }
        }
        if (ctx->generic_binding_count > 0 && p->type != NULL) {
            /* Specialization: re-render via the binding-aware AST path so
             * nested generic args (Array<T>, Map<K, V>) substitute. */
            char pt_ast[256];
            if (transpiler_require_ast_c_type_copy(ctx, p->type,
                    surface_desc, pt_ast, sizeof(pt_ast))
                && pt_ast[0] != '\0') {
                pergyra_str_copy(pt, sizeof(pt), pt_ast);
            }
        }
        {
            const char *ptn = param_type_name != NULL
                ? param_type_name
                : (p->type != NULL
                    ? transpiler_render_type_name_local(ctx, p->type)
                    : NULL);
            bool subj_param = ptn != NULL
                && is_pointer_self_host_type_name(ctx, ptn);
            if (subj_param)
                codebuf_write(buf, ", %s *%s", pt, p->name);
            else
                codebuf_write(buf, ", %s %s", pt, p->name);
        }
    }
    codebuf_write(buf, ");\n");
}
