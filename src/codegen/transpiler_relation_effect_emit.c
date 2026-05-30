#include "transpiler_relation_effect_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_type_require.h"

static bool
transpiler_relation_effect_surface_desc(char *out, size_t out_size,
                                        const char *surface_kind,
                                        const char *owner_name,
                                        const char *member_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s '%s.%s'",
        surface_kind,
        owner_name != NULL ? owner_name : "(anonymous)",
        member_name != NULL ? member_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_relation_effect_surface_desc_too_long(TranspilerCtx *ctx,
                                                 const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "relation/effect");
}

void
emit_relation_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_RELATION_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    size_t slot_count = 0;
    ASTNode **slots = ast_relation_slots(node, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation slot", name,
                ast_domain_slot_name(slot))) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation slot");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                ast_domain_slot_type(slot),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft,
            ast_domain_slot_name(slot));
        if (!ast_domain_slot_is_subject(slot)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                ast_domain_slot_name(slot));
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                ast_domain_slot_name(slot));
            emit_hidden_provenance_fields(ctx, "projection",
                ast_domain_slot_name(slot));
        }
    }

    PgyHostSharedFieldsCompatView shared_view =
        pgy_host_shared_fields_compat_view_from_decl(node);
    size_t shared_count = shared_view.count;
    ASTNode **shared_fields = shared_view.fields;
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation shared field", name,
                ast_party_shared_name(shared))) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                ast_party_shared_type(shared),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft,
            ast_party_shared_name(shared));
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_relation_refreshes(node, &refresh_count);
    emit_domain_projection_sync_loop(ctx,
        slots,
        slot_count,
        refreshes,
        refresh_count,
        "relation_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for relation '%s'",
            name != NULL ? name : "(anonymous-relation)");
        return;
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-relation)",
        "relation", &method_view, ctx);
}

void
emit_effect_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_EFFECT_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    size_t slot_count = 0;
    ASTNode **slots = ast_effect_slots(node, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect slot", name,
                ast_domain_slot_name(slot))) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect slot");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                ast_domain_slot_type(slot),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft,
            ast_domain_slot_name(slot));
        if (!ast_domain_slot_is_subject(slot)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                ast_domain_slot_name(slot));
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                ast_domain_slot_name(slot));
            emit_hidden_provenance_fields(ctx, "projection",
                ast_domain_slot_name(slot));
        }
    }

    PgyHostSharedFieldsCompatView shared_view =
        pgy_host_shared_fields_compat_view_from_decl(node);
    size_t shared_count = shared_view.count;
    ASTNode **shared_fields = shared_view.fields;
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect shared field", name,
                ast_party_shared_name(shared))) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                ast_party_shared_type(shared),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft,
            ast_party_shared_name(shared));
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_effect_refreshes(node, &refresh_count);
    emit_domain_projection_sync_loop(ctx,
        slots,
        slot_count,
        refreshes,
        refresh_count,
        "effect_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for effect '%s'",
            name != NULL ? name : "(anonymous-effect)");
        return;
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-effect)",
        "effect", &method_view, ctx);
}
