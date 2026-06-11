#include "transpiler_roster_decl_emit.h"

#include <stddef.h>

#include "../parser/ast_api.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_nominal_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_type_require.h"

void
emit_roster_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_ROSTER_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    TranspilerHostedRosterSlotView roster_view =
        transpiler_hosted_roster_slot_view_from_decl(ctx, name, node);
    if (transpiler_hosted_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing roster-slot declaration metadata for roster '%s'",
            name != NULL ? name : "(anonymous-roster)");
        return;
    }
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared-field declaration metadata for roster '%s'",
            name != NULL ? name : "(anonymous-roster)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for roster '%s'",
            name != NULL ? name : "(anonymous-roster)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for roster '%s'",
            name != NULL ? name : "(anonymous-roster)")) {
        return;
    }

    codebuf_write(ctx->out, "\n/* Roster: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < roster_view.count; i++) {
        codebuf_write(ctx->out, "    %s %s;\n",
            transpiler_hosted_roster_slot_view_type_name(&roster_view, i),
            transpiler_hosted_roster_slot_view_name(&roster_view, i));
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        char field_type[256];
        char surface_desc[256];
        if (!transpiler_domain_nominal_surface_desc(surface_desc,
                sizeof(surface_desc), "roster shared field", name,
                shared_name,
                NULL)) {
            transpiler_domain_nominal_surface_desc_too_long(
                ctx, "roster shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                transpiler_hosted_shared_field_view_type(&shared_view, i),
                surface_desc,
                field_type,
                sizeof(field_type))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n",
            field_type, shared_name != NULL ? shared_name : "field");
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for roster '%s'",
                name != NULL ? name : "(anonymous-roster)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-roster)",
        "roster", &method_view, ctx);
}
