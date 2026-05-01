#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include "type_checker_world_internal.h"

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.world_decl.name;
    ASTNode *saved_world = ctx->current_world;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_WORLD;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_WORLD_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of world '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    ctx->current_world = node;

    /* Check roster references */
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *ws = node->data.world_decl.rosters[i];
        if (ws->data.world_roster.roster_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope,
                ws->data.world_roster.roster_type);
            if (sys == NULL || sys->kind != SYMBOL_ROSTER) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
                    PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_DECLARE_OR_IMPORT_TYPE,
                    ws,
                    "Roster type '%s' not found for slot '%s'.\n"
                    "Reason:\n"
                    "- world roster slot '%s' must reference a visible roster declaration\n"
                    "- no roster named '%s' is visible at this point\n"
                    "Fix:\n"
                    "- declare roster '%s'\n"
                    "- or import/export the module that defines it",
                    ws->data.world_roster.roster_type,
                    ws->data.world_roster.slot_name,
                    ws->data.world_roster.slot_name,
                    ws->data.world_roster.roster_type,
                    ws->data.world_roster.roster_type);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        if (wz->data.world_zone.zone_type != NULL) {
            Symbol *zone = scope_lookup(ctx->scope,
                wz->data.world_zone.zone_type);
            ASTNode *zone_decl = find_domain_decl_by_name(
                ctx->program_root, AST_ZONE_DECL, wz->data.world_zone.zone_type);
            if ((zone == NULL || zone->kind != SYMBOL_ZONE)
                && !(zone_decl != NULL
                     && zone_decl->type == AST_ZONE_DECL
                     && explicit_type_reference_allowed(zone_decl, node, ctx))) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, wz,
                    "Zone type '%s' not found for slot '%s'.\n"
                    "Reason:\n"
                    "- world zone slot '%s' must reference a visible zone declaration\n"
                    "- no zone named '%s' is visible at this point\n"
                    "Fix:\n"
                    "- declare zone '%s'\n"
                    "- or import/export the module that defines it",
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.slot_name,
                    wz->data.world_zone.slot_name,
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.zone_type);
            }
        }
    }

    type_check_world_states(node, ctx);

    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *activate = node->data.world_decl.activations[i];
        const char *zone_slot_name = activate->data.world_activate.zone_slot_name;
        if (activate->data.world_activate.state_name != NULL) {
            if (!resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, activate,
                "World activate references unknown zone slot '%s'.\n"
                "Contract source:\n"
                "- world '%s' activate edge -> '%s'\n"
                "Reason:\n"
                "- activate needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- activate a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                node->data.world_decl.name != NULL ? node->data.world_decl.name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.activate_count; j++) {
            ASTNode *other = node->data.world_decl.activations[j];
            const char *other_zone = other->data.world_activate.zone_slot_name;
            if (other->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' activates zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is activated by multiple lifecycle entries\n"
                    "- repeated activation does not add meaning and obscures lifecycle intent\n"
                    "Fix:\n"
                    "- keep a single activate entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and deactivates zone '%s'; choose one lifecycle direction.\n"
                    "Reason:\n"
                    "- the same world zone slot appears in both activate and deactivate lifecycle paths\n"
                    "- this makes the lifecycle contract contradictory\n"
                    "Fix:\n"
                    "- keep only activate or deactivate for zone '%s'\n"
                    "- or split the behavior into separate world-state aliases",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *maintain = node->data.world_decl.maintained_zones[j];
            const char *other_zone = maintain->data.world_maintain.zone_slot_name;
            if (maintain->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, maintain,
                    maintain->data.world_maintain.state_name, ctx, "maintain",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and maintains zone '%s'; maintain already implies the continuing direction.\n"
                    "Reason:\n"
                    "- `maintain` already carries the continuing lifecycle direction for that zone\n"
                    "- adding an explicit activate for the same zone makes the contract redundant\n"
                    "Fix:\n"
                    "- keep `maintain` for zone '%s'\n"
                    "- or drop `maintain` and keep only the activate rule if that is the intended contract",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = node->data.world_decl.deactivations[i];
        const char *zone_slot_name = deactivate->data.world_deactivate.zone_slot_name;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            if (!resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, deactivate,
                "World deactivate references unknown zone slot '%s'.\n"
                "Contract source:\n"
                "- world '%s' deactivate edge -> '%s'\n"
                "Reason:\n"
                "- deactivate needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- deactivate a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                node->data.world_decl.name != NULL ? node->data.world_decl.name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *other = node->data.world_decl.deactivations[j];
            const char *other_zone = other->data.world_deactivate.zone_slot_name;
            if (other->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' deactivates zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is deactivated by multiple lifecycle entries\n"
                    "- repeated deactivation does not add meaning and obscures lifecycle intent\n"
                    "Fix:\n"
                    "- keep a single deactivate entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = node->data.world_decl.maintained_zones[i];
        const char *zone_slot_name = maintain->data.world_maintain.zone_slot_name;
        const char *state_name = maintain->data.world_maintain.state_name;
        if (state_name != NULL) {
            if (!resolve_world_zone_state(node, maintain, state_name, ctx, "maintain",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, maintain,
                "World maintain references unknown zone slot '%s'.\n"
                "Contract source:\n"
                "- world '%s' maintain edge -> '%s'\n"
                "Reason:\n"
                "- maintain needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- maintain a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                node->data.world_decl.name != NULL ? node->data.world_decl.name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *other = node->data.world_decl.maintained_zones[j];
            const char *other_zone = other->data.world_maintain.zone_slot_name;
            if (other->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_maintain.state_name, ctx, "maintain", &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' maintains zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is maintained by multiple lifecycle entries\n"
                    "- repeated maintain rules do not add meaning and obscure provenance\n"
                    "Fix:\n"
                    "- keep a single maintain entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and deactivates zone '%s'; choose one lifecycle direction.\n"
                    "Reason:\n"
                    "- `maintain` keeps the zone active across the world lifecycle\n"
                    "- `deactivate` closes that same lifecycle path\n"
                    "- using both on the same zone makes the contract contradictory\n"
                    "Fix:\n"
                    "- keep only maintain or deactivate for zone '%s'\n"
                    "- or split the behavior into separate world-state aliases",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.activate_count; j++) {
            ASTNode *activate = node->data.world_decl.activations[j];
            const char *other_zone = activate->data.world_activate.zone_slot_name;
            if (activate->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and activates zone '%s'; maintain already implies the continuing direction.\n"
                    "Reason:\n"
                    "- `maintain` already expresses the continuing active lifecycle for that zone\n"
                    "- adding activate for the same zone makes the contract redundant\n"
                    "Fix:\n"
                    "- keep `maintain` for zone '%s'\n"
                    "- or drop `maintain` and keep only the activate rule if that is the intended contract",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            world_resolve_type_ref(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        type_check_func_decl(node->data.world_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_world = saved_world;
    return !ctx->has_error;
}
