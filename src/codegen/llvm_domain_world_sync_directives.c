#ifdef PGY_LLVM_ENABLED
#include "llvm_domain_world_sync_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "../compiler/mir_decl_headers.h"
#include "parser/ast_api.h"

static bool
llvm_world_sync_directive_field_name(char *out,
                                     size_t out_size,
                                     const char *kind,
                                     const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || slot_name == NULL)
        return false;
    written = snprintf(out, out_size, "__%s_%s", kind, slot_name);
    return written >= 0 && (size_t)written < out_size;
}

ASTNode *
llvm_world_sync_find_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    size_t state_count = 0;
    ASTNode **states = ast_world_states(world_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

bool
llvm_world_sync_has_zone_slot(LLVMGenCtx *ctx,
                              ASTNode *world_decl,
                              const char *slot_name)
{
    const char *world_name;
    LLVMHostedWorldZoneSlotView zone_view;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return false;

    world_name = llvm_decl_node_name(world_decl);
    zone_view = llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name,
        world_decl);
    if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world zone-slot metadata for '%s'",
            world_name != NULL ? world_name : "<anonymous>");
        return false;
    }

    for (size_t i = 0; i < zone_view.count; i++) {
        const char *zone_slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, i);
        if (zone_slot_name != NULL && strcmp(zone_slot_name, slot_name) == 0) {
            return true;
        }
    }

    return false;
}

static const char *
llvm_world_sync_resolve_zone_slot(LLVMGenCtx *ctx,
                                  ASTNode *stmt,
                                  const char *slot_name,
                                  const char *state_name)
{
    if (slot_name != NULL)
        return slot_name;
    if (state_name != NULL) {
        ASTNode *state = llvm_world_sync_find_state_decl(stmt, state_name);
        if (state != NULL)
            return ast_world_state_zone_slot_name(state);
        if (llvm_world_sync_has_zone_slot(ctx, stmt, state_name))
            return state_name;
    }
    return NULL;
}

static const char *
llvm_world_sync_resolve_mir_zone_slot(
    LLVMGenCtx *ctx,
    const MIRDeclHeader *header,
    const LLVMHostedWorldZoneSlotView *zone_view,
    const MIRDeclWorldDirective *directive,
    const char *world_name)
{
    const char *slot_name;
    const char *state_name;

    if (header == NULL || zone_view == NULL || directive == NULL)
        return NULL;
    slot_name = mir_decl_world_directive_zone_slot_name(directive);
    state_name = mir_decl_world_directive_state_name(directive);
    if (slot_name == NULL && state_name != NULL) {
        for (size_t i = 0; i < mir_decl_header_world_state_count(header); i++) {
            const MIRDeclWorldState *state =
                mir_decl_header_world_state(header, i);
            const char *state_decl_name = mir_decl_world_state_name(state);
            if (state_decl_name != NULL
                && strcmp(state_decl_name, state_name) == 0) {
                slot_name = mir_decl_world_state_zone_slot_name(state);
                break;
            }
        }
        if (slot_name == NULL) {
            for (size_t i = 0; i < zone_view->count; i++) {
                const char *candidate =
                    llvm_hosted_world_zone_slot_view_name(zone_view, i);
                if (candidate != NULL && strcmp(candidate, state_name) == 0) {
                    slot_name = candidate;
                    break;
                }
            }
        }
    }
    if (slot_name == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path world '%s' directive has no zone slot",
            world_name != NULL ? world_name : "(anonymous-world)");
        return NULL;
    }
    for (size_t i = 0; i < zone_view->count; i++) {
        const char *candidate =
            llvm_hosted_world_zone_slot_view_name(zone_view, i);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0)
            return candidate;
    }
    llvm_set_mir_inventory_missing(ctx,
        "MIR-only LLVM path world '%s' directive references unknown zone slot '%s'",
        world_name != NULL ? world_name : "(anonymous-world)", slot_name);
    return NULL;
}

static void
llvm_world_sync_store_zone_active(LLVMClassTypeEntry *decl_cls,
                                  LLVMValueRef sync_fn,
                                  LLVMGenCtx *ctx,
                                  const char *slot_name,
                                  LLVMValueRef active_val,
                                  int cause)
{
    char active_field[256];
    int active_idx;
    LLVMValueRef self_ptr;
    LLVMValueRef active_ptr;

    if (slot_name == NULL)
        return;

    if (!llvm_world_sync_directive_field_name(active_field,
            sizeof(active_field), "zone_active", slot_name))
        return;
    active_idx = llvm_class_field_index(decl_cls, active_field);
    if (active_idx < 0)
        return;

    self_ptr = LLVMGetParam(sync_fn, 0);
    active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
        self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, active_val, active_ptr);
    llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone", slot_name, cause);
}

void
llvm_world_sync_emit_directives(const MIRDeclHeader *header,
                                ASTNode *stmt,
                                LLVMClassTypeEntry *decl_cls,
                                LLVMValueRef sync_fn,
                                LLVMGenCtx *ctx)
{
    const char *world_name = stmt != NULL ? llvm_decl_node_name(stmt) : NULL;
    LLVMHostedWorldZoneSlotView zone_view;

    if (llvm_active_has_mir(ctx)) {
        size_t directive_count;

        if (header == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing declaration header for world directives '%s'",
                world_name != NULL ? world_name : "(anonymous-world)");
            return;
        }
        zone_view = llvm_hosted_world_zone_slot_view_from_decl(
            ctx, world_name, stmt);
        if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing world zone-slot metadata for '%s'",
                world_name != NULL ? world_name : "(anonymous-world)");
            return;
        }
        directive_count = mir_decl_header_world_directive_count(header);
        if (directive_count
            != mir_decl_header_world_directive_declared_count(header)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path world '%s' has inconsistent directive metadata count",
                world_name != NULL ? world_name : "(anonymous-world)");
            return;
        }
        for (size_t i = 0; i < directive_count; i++) {
            const MIRDeclWorldDirective *directive =
                mir_decl_header_world_directive(header, i);
            const char *slot_name = llvm_world_sync_resolve_mir_zone_slot(
                ctx, header, &zone_view, directive, world_name);
            LLVMValueRef active_val;
            int cause;

            if (directive == NULL || slot_name == NULL)
                return;
            switch (mir_decl_world_directive_kind(directive)) {
            case MIR_DECL_WORLD_DIRECTIVE_ACTIVATE:
                active_val = LLVMConstInt(ctx->type_i1, 1, 0);
                cause = PGY_PROP_CAUSE_WORLD_ACTIVATE;
                break;
            case MIR_DECL_WORLD_DIRECTIVE_MAINTAIN:
                active_val = LLVMConstInt(ctx->type_i1, 1, 0);
                cause = PGY_PROP_CAUSE_WORLD_MAINTAIN;
                break;
            case MIR_DECL_WORLD_DIRECTIVE_DEACTIVATE:
                active_val = LLVMConstInt(ctx->type_i1, 0, 0);
                cause = PGY_PROP_CAUSE_WORLD_DEACTIVATE;
                break;
            default:
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path world '%s' has unknown directive kind",
                    world_name != NULL ? world_name : "(anonymous-world)");
                return;
            }
            llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx,
                slot_name, active_val, cause);
        }
        return;
    }

    size_t activate_count = 0;
    ASTNode **activations = ast_world_activations(stmt, &activate_count);
    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *act = activations[i];
        const char *slot_name = act != NULL
            ? llvm_world_sync_resolve_zone_slot(ctx, stmt,
                ast_world_directive_zone_slot_name(act),
                ast_world_directive_state_name(act))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 1, 0), PGY_PROP_CAUSE_WORLD_ACTIVATE);
    }

    size_t maintained_zone_count = 0;
    ASTNode **maintained_zones =
        ast_world_maintained_zones(stmt, &maintained_zone_count);
    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *mnt = maintained_zones[i];
        const char *slot_name = mnt != NULL
            ? llvm_world_sync_resolve_zone_slot(ctx, stmt,
                ast_world_directive_zone_slot_name(mnt),
                ast_world_directive_state_name(mnt))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 1, 0), PGY_PROP_CAUSE_WORLD_MAINTAIN);
    }

    size_t deactivate_count = 0;
    ASTNode **deactivations = ast_world_deactivations(stmt, &deactivate_count);
    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *act = deactivations[i];
        const char *slot_name = act != NULL
            ? llvm_world_sync_resolve_zone_slot(ctx, stmt,
                ast_world_directive_zone_slot_name(act),
                ast_world_directive_state_name(act))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 0, 0), PGY_PROP_CAUSE_WORLD_DEACTIVATE);
    }
}

#endif /* PGY_LLVM_ENABLED */
