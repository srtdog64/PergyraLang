#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *world_decl;
    const char *world_name = NULL;

    if (call == NULL || call->type != AST_CALL || ctx == NULL
        || call->data.call.callee == NULL
        || call->data.call.callee->type != AST_IDENTIFIER) {
        return;
    }

    world_name = call->data.call.callee->data.identifier.name;
    world_decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
        world_name);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(world_decl, &zone_count);

    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        ASTNode *arg = call->data.call.arguments[i];
        Symbol *arg_sym;
        bool matched_zone_slot = false;

        if (arg == NULL || arg->type != AST_IDENTIFIER)
            continue;

        arg_sym = scope_lookup(ctx->scope, arg->data.identifier.name);
        if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE
            || arg_sym->type == NULL || arg_sym->type->name == NULL) {
            if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE)
                continue;
        }

        if (i < zone_count) {
            ASTNode *zone_slot = zones[i];
            const char *zone_type = ast_world_zone_type_name(zone_slot);
            const char *zone_slot_name = ast_world_zone_slot_name(zone_slot);
            if (zone_slot != NULL && zone_slot->type == AST_WORLD_ZONE
                && zone_type != NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "Contract source:\n"
                    "- world '%s' zone slot '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                    "- owned embedding hands authority-bearing visibility to the world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the world-owned handoff destination\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot_name);
                matched_zone_slot = true;
            }
        }

        if (matched_zone_slot)
            continue;

        for (size_t zi = 0; zi < zone_count; zi++) {
            ASTNode *zone_slot = zones[zi];
            const char *zone_type = ast_world_zone_type_name(zone_slot);
            const char *zone_slot_name = ast_world_zone_slot_name(zone_slot);
            if (zone_slot == NULL || zone_slot->type != AST_WORLD_ZONE
                || zone_type == NULL) {
                continue;
            }
            if (arg_sym->type != NULL && arg_sym->type->name != NULL
                && strcmp(arg_sym->type->name, zone_type) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "Contract source:\n"
                    "- world '%s' zone slot '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                    "- owned embedding hands authority-bearing visibility to the world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the world-owned handoff destination\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot_name);
                break;
            }
        }
    }
}
