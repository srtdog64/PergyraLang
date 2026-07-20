#include "transpiler_zone_decl_emit.h"

#include "../compiler/mir_decl_headers.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_zone_decl_emit_internal.h"

void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_ZONE_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;
    transpiler_emit_zone_decl_impl(node, NULL, name, ctx);
}

void
emit_zone_decl_from_mir_header(const MIRDeclHeader *header,
                               TranspilerCtx *ctx)
{
    const char *name;
    ASTNode *node;

    if (header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing zone declaration header");
        return;
    }
    name = mir_decl_header_name(header);
    if (name == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing zone declaration header name");
        return;
    }
    node = transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, name);
    if (node == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing AST compatibility surface for zone '%s'",
            name);
        return;
    }
    transpiler_emit_zone_decl_impl(node, header, name, ctx);
}
