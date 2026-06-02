#include "transpiler_zone_struct_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_type_require.h"

static bool
transpiler_zone_surface_desc(char *out, size_t out_size,
                             const char *surface_kind,
                             const char *zone_name,
                             const char *member_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s '%s.%s'",
        surface_kind,
        zone_name != NULL ? zone_name : "(anonymous)",
        member_name != NULL ? member_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_zone_surface_desc_too_long(TranspilerCtx *ctx,
                                      const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "zone");
}

bool
transpiler_emit_zone_struct_decl(TranspilerCtx *ctx, ASTNode *node,
                                 const char *name)
{
    TranspilerHostedDomainSlotView slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, name, node);
    size_t slot_count = slot_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    TranspilerHostedZoneLayerSlotView layer_view =
        transpiler_hosted_zone_layer_slot_view_from_decl(ctx, name, node);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone domain-slot metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return false;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared field metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return false;
    }
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone layer-slot metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return false;
    }
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(node, &state_count);

    codebuf_write(ctx->out, "\n/* Zone: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        char ft[256];
        char surface_desc[256];
        if (!transpiler_zone_surface_desc(surface_desc,
                sizeof(surface_desc), "zone slot", name,
                slot_name)) {
            transpiler_zone_surface_desc_too_long(ctx, "zone slot");
            return false;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                transpiler_hosted_domain_slot_view_type(&slot_view, i),
                surface_desc,
                ft,
                sizeof(ft))) {
            return false;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot_name);
        if (!transpiler_hosted_domain_slot_view_is_subject_like(
                &slot_view, i)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot_name);
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                slot_name);
            emit_hidden_provenance_fields(ctx, "projection", slot_name);
        }
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        ASTNode *slot =
            transpiler_hosted_zone_layer_slot_view_source_ast(&layer_view, i);
        const char *slot_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        const char *layer_type =
            transpiler_hosted_zone_layer_slot_view_type_name(&layer_view, i);

        if (slot_name == NULL || layer_type == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "C backend: zone '%s' layer slot[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-zone)",
                i);
            return false;
        }

        if (slot != NULL && ast_zone_layer_slot_is_pool(slot)) {
            int cap = ast_zone_layer_slot_pool_capacity(slot);
            if (cap <= 0)
                cap = 1;
            codebuf_write(ctx->out,
                "    struct { %s items[%d]; bool active[%d]; uint8_t count; uint8_t cap; } %s;\n",
                layer_type,
                cap,
                cap,
                slot_name);
        } else {
            codebuf_write(ctx->out, "    %s %s;\n",
                layer_type,
                slot_name);
        }
        codebuf_write(ctx->out, "    bool __layer_active_%s;\n",
            slot_name);
        emit_hidden_provenance_fields(ctx, "layer",
            slot_name);
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        ASTNode *shared_type =
            transpiler_hosted_shared_field_view_type(&shared_view, i);
        char ft[256];
        char surface_desc[256];
        if (shared_name == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: zone '%s' shared field[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-zone)",
                i);
            return false;
        }
        if (!transpiler_zone_surface_desc(surface_desc,
                sizeof(surface_desc), "zone shared field", name,
                shared_name)) {
            transpiler_zone_surface_desc_too_long(ctx, "zone shared field");
            return false;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                shared_type,
                surface_desc,
                ft,
                sizeof(ft))) {
            return false;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared_name);
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        codebuf_write(ctx->out, "    bool __state_%s;\n",
            ast_zone_state_name(state));
        emit_hidden_provenance_fields(ctx, "state",
            ast_zone_state_name(state));
    }

    codebuf_write(ctx->out, "    PGY_ZONE_LOCK_FIELD\n");
    codebuf_write(ctx->out, "    PGY_ZONE_GENERATION_FIELD\n");
    codebuf_write(ctx->out, "} %s;\n", name);
    return true;
}

void
transpiler_emit_zone_layer_accessors(TranspilerCtx *ctx, ASTNode *node,
                                     const char *name)
{
    size_t layer_slot_count = 0;
    TranspilerHostedZoneLayerSlotView layer_view =
        transpiler_hosted_zone_layer_slot_view_from_decl(ctx, name, node);

    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone layer-slot metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return;
    }

    layer_slot_count = layer_view.count;
    for (size_t i = 0; i < layer_slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);

        if (slot_name == NULL)
            continue;

        codebuf_write(ctx->out,
            "\nstatic inline bool\n%s_has_layer_%s(%s *self, uint32_t expected_gen)\n{\n",
            name, slot_name, name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "bool result;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_RDLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, \"%s.%s\");\n",
            name, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "result = self->__layer_active_%s;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return result;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}
