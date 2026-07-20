#include "transpiler_roster_decl_emit.h"

#include <stddef.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_nominal_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_type_require.h"

static void
emit_roster_decl_impl(ASTNode *node,
                      const MIRDeclHeader *mir_header,
                      const char *mir_name,
                      TranspilerCtx *ctx)
{
    const char *name = mir_name;
    ASTNode *inventory_decl;

    if (transpiler_active_has_mir(ctx)) {
        if (mir_header == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx, "MIR-only C path missing roster declaration header");
            return;
        }
        name = mir_decl_header_name(mir_header);
        if (name == NULL || name[0] == '\0') {
            transpiler_set_mir_inventory_missing(
                ctx, "MIR-only C path missing roster declaration header name");
            return;
        }
        node = NULL;
    } else {
        name = transpiler_decl_name_local(node);
        if (name == NULL)
            return;
        inventory_decl = transpiler_find_named_decl_local(
            ctx, AST_ROSTER_DECL, name);
        if (inventory_decl != NULL)
            node = inventory_decl;
    }

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
        const MIRDeclField *shared_meta =
            transpiler_hosted_shared_field_view_metadata(&shared_view, i);
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
        {
            const char *shared_type_name =
                transpiler_mir_decl_field_type_name(shared_meta);
            if (shared_type_name != NULL) {
                if (!transpiler_require_type_name_c_type_copy(
                        ctx, shared_type_name, surface_desc,
                        field_type, sizeof(field_type)))
                    return;
            } else if (shared_view.requires_mir_metadata) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing roster shared-field type-name metadata for '%s.%s'",
                    name != NULL ? name : "(anonymous-roster)",
                    shared_name != NULL ? shared_name : "(anonymous)");
                return;
            } else if (!transpiler_require_ast_c_type_copy(
                    ctx,
                    transpiler_hosted_shared_field_view_type(&shared_view, i),
                    surface_desc,
                    field_type,
                    sizeof(field_type))) {
                return;
            }
        }
        codebuf_write(ctx->out, "    %s %s;\n",
            field_type, shared_name != NULL ? shared_name : "field");
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for roster '%s'",
                name != NULL ? name : "(anonymous-roster)");
            return;
        }
        if (method_meta == NULL) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            NULL, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-roster)",
        "roster", &method_view, ctx);
}

void
emit_roster_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_roster_decl_impl(node, NULL, NULL, ctx);
}

void
emit_roster_decl_from_mir_header(const MIRDeclHeader *header,
                                 TranspilerCtx *ctx)
{
    if (header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing roster declaration header");
        return;
    }
    emit_roster_decl_impl(NULL, header, mir_decl_header_name(header), ctx);
}
