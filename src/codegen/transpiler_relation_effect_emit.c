#include "transpiler_relation_effect_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
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
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_RELATION_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared field metadata for relation '%s'",
            name != NULL ? name : "(anonymous-relation)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for relation '%s'",
            name != NULL ? name : "(anonymous-relation)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for relation '%s'",
            name != NULL ? name : "(anonymous-relation)")) {
        return;
    }
    TranspilerHostedDomainSlotView slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, name, node);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing domain-slot metadata for relation '%s'",
            name != NULL ? name : "(anonymous-relation)");
        return;
    }

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    size_t slot_count = slot_view.count;
    for (size_t i = 0; i < slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation slot", name,
                slot_name)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation slot");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                transpiler_hosted_domain_slot_view_type(&slot_view, i),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
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
                "C backend: relation '%s' shared field[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-relation)",
                i);
            return;
        }
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation shared field", name,
                shared_name)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                shared_type,
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared_name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_relation_refreshes(node, &refresh_count);
    emit_domain_projection_sync_loop_from_view(ctx,
        &slot_view,
        refreshes,
        refresh_count,
        "relation_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for relation '%s'",
                name != NULL ? name : "(anonymous-relation)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
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
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_EFFECT_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared field metadata for effect '%s'",
            name != NULL ? name : "(anonymous-effect)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for effect '%s'",
            name != NULL ? name : "(anonymous-effect)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for effect '%s'",
            name != NULL ? name : "(anonymous-effect)")) {
        return;
    }
    TranspilerHostedDomainSlotView slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, name, node);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing domain-slot metadata for effect '%s'",
            name != NULL ? name : "(anonymous-effect)");
        return;
    }

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    size_t slot_count = slot_view.count;
    for (size_t i = 0; i < slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        char ft[256];
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect slot", name,
                slot_name)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect slot");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                transpiler_hosted_domain_slot_view_type(&slot_view, i),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
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
                "C backend: effect '%s' shared field[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-effect)",
                i);
            return;
        }
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect shared field", name,
                shared_name)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                shared_type,
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared_name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_effect_refreshes(node, &refresh_count);
    emit_domain_projection_sync_loop_from_view(ctx,
        &slot_view,
        refreshes,
        refresh_count,
        "effect_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for effect '%s'",
                name != NULL ? name : "(anonymous-effect)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-effect)",
        "effect", &method_view, ctx);
}
