#include "transpiler_domain_ability_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_nominal_emit.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

typedef struct TranspilerAbilityMethodView
{
    const MIRDeclHeader *decl_header;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} TranspilerAbilityMethodView;

static TranspilerAbilityMethodView
transpiler_ability_method_view_from_decl(const TranspilerCtx *ctx,
                                         const char *ability_name)
{
    TranspilerAbilityMethodView view;

    view.decl_header = NULL;
    view.count = 0;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = true;

    if (transpiler_active_has_mir(ctx)) {
        view.decl_header = transpiler_active_decl_header_of_type(
            ctx, AST_ABILITY_DECL, ability_name);
        if (view.decl_header != NULL) {
            view.count = mir_decl_header_method_count(view.decl_header);
            view.uses_mir_metadata = true;
        }
        return view;
    }

    return view;
}

static const MIRDeclMethod *
transpiler_ability_method_view_metadata(
    const TranspilerAbilityMethodView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_method(view->decl_header, index);
}

static bool
transpiler_require_ability_method_view_rows(
    TranspilerCtx *ctx,
    const TranspilerAbilityMethodView *view,
    const char *ability_name)
{
    if (view != NULL && view->requires_mir_metadata
        && !view->uses_mir_metadata) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing ability vtable method metadata for '%s'",
            ability_name != NULL ? ability_name : "(anonymous-ability)");
        return false;
    }
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        if (view->uses_mir_metadata
            && transpiler_ability_method_view_metadata(view, i) == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path has invalid ability vtable method metadata row for '%s'",
                ability_name != NULL ? ability_name : "(anonymous-ability)");
            return false;
        }
    }
    return true;
}

void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_ability_name(node);
    TranspilerAbilityMethodView methods =
        transpiler_ability_method_view_from_decl(ctx, name);
    size_t generic_param_count = 0;

    if (!transpiler_require_ability_method_view_rows(ctx, &methods, name))
        return;
    if (mir_decl_header_name(methods.decl_header) != NULL) {
        name = mir_decl_header_name(methods.decl_header);
    }
    generic_param_count =
        mir_decl_header_generic_param_count(methods.decl_header);

    if (generic_param_count > 0) {
        codebuf_write(ctx->out,
            "\n/* Generic ability: %s (vtable emitted per concrete ability reference) */\n",
            name != NULL ? name : "<anonymous>");
        return;
    }

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < methods.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_ability_method_view_metadata(&methods, i);
        const char *method_name;
        char ret_type_buf[256];
        const char *ret_type = "void";
        const char *return_type_name;

        if (method_meta == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing ability vtable method metadata row for '%s'",
                name != NULL ? name : "(anonymous-ability)");
            return;
        }
        method_name = transpiler_mir_decl_method_name(method_meta);
        return_type_name =
            transpiler_mir_decl_method_return_type_name(method_meta);
        if (method_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing ability vtable method name metadata for '%s'",
                name != NULL ? name : "(anonymous-ability)");
            return;
        }
        if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                name,
                method_name,
                TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only C path missing ability vtable return type-name metadata for '%s.%s'",
                "MIR-only C path missing ability vtable parameter type-name metadata for '%s.%s'")) {
            return;
        }
        if (return_type_name != NULL) {
            if (!transpiler_require_type_name_c_type_copy(
                    ctx, return_type_name, "ability method return",
                    ret_type_buf, sizeof(ret_type_buf))) {
                return;
            }
            ret_type = ret_type_buf;
        }

        codebuf_write(ctx->out, "    %s (*%s)(void *self",
                      ret_type, method_name);

        for (size_t j = 0;
             j < transpiler_mir_decl_method_param_count(method_meta);
             j++) {
            FuncParam *p = transpiler_mir_decl_method_param(method_meta, j);
            const char *param_type_name =
                transpiler_mir_decl_method_param_type_name(method_meta, j);
            char pt[256];
            bool pointer_param = false;
            char surface_desc[256];
            if (p == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing ability vtable parameter metadata for '%s.%s'",
                    name != NULL ? name : "(anonymous-ability)",
                    method_name != NULL ? method_name : "(anonymous)");
                return;
            }
            if (p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            if (!transpiler_domain_nominal_surface_desc(surface_desc,
                    sizeof(surface_desc), "ability method parameter",
                    name, method_name, p != NULL ? p->name : NULL)) {
                transpiler_domain_nominal_surface_desc_too_long(
                    ctx, "ability method parameter");
                return;
            }
            if (param_type_name != NULL) {
                if (!transpiler_require_type_name_c_type_copy(ctx,
                        param_type_name, surface_desc, pt, sizeof(pt))) {
                    return;
                }
            }
            pointer_param =
                is_pointer_self_host_type_name(ctx, param_type_name);
            codebuf_write(ctx->out, ", %s%s %s", pt,
                pointer_param ? " *" : "", p->name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}
