#include "transpiler_world_select_event_emit.h"

#include "../compiler/mir_decl_headers.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_world_select_event_emit_internal.h"

void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_WORLD_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;
    transpiler_emit_world_decl_impl(node, NULL, name, ctx);
}

void
emit_world_decl_from_mir_header(const MIRDeclHeader *header,
                                TranspilerCtx *ctx)
{
    const char *name;

    if (header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing world declaration header");
        return;
    }
    name = mir_decl_header_name(header);
    if (name == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing world declaration header name");
        return;
    }
    transpiler_emit_world_decl_impl(NULL, header, name, ctx);
}
