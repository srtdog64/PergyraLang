#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include "type_checker_world_internal.h"

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_world_name(node);
    ASTNode *saved_world = ctx->current_world;
    ASTNode **rosters;
    ASTNode **zones;
    ASTNode **activations;
    ASTNode **deactivations;
    ASTNode **maintained_zones;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t roster_count;
    size_t zone_count;
    size_t activate_count;
    size_t deactivate_count;
    size_t maintained_zone_count;
    size_t shared_count;
    size_t method_count;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;
    symbol_mark_declaration(sym, ast_node_stable_id(node), false);

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (symbol_is_forward_declaration_for(existing,
            SYMBOL_CLASS, ast_node_stable_id(node))) {
        existing->kind = SYMBOL_WORLD;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_complete_forward_declaration(existing);
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
    rosters = ast_world_rosters(node, &roster_count);
    zones = ast_world_zones(node, &zone_count);
    activations = ast_world_activations(node, &activate_count);
    deactivations = ast_world_deactivations(node, &deactivate_count);
    maintained_zones = ast_world_maintained_zones(node,
                                                  &maintained_zone_count);
    shared_fields = ast_world_shared_fields(node, &shared_count);
    methods = ast_world_methods(node, &method_count);

    /* Check roster references */
    for (size_t i = 0; i < roster_count; i++) {
        ASTNode *ws = rosters[i];
        const char *roster_type = ast_world_roster_type_name(ws);
        const char *slot_name = ast_world_roster_slot_name(ws);
        if (roster_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope, roster_type);
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
                    roster_type,
                    slot_name,
                    slot_name,
                    roster_type,
                    roster_type);
            }
        }
    }

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *wz = zones[i];
        const char *zone_type = ast_world_zone_type_name(wz);
        const char *slot_name = ast_world_zone_slot_name(wz);
        if (zone_type != NULL) {
            Symbol *zone = scope_lookup(ctx->scope, zone_type);
            ASTNode *zone_decl = semantic_find_zone_decl_by_name(ctx, zone_type);
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
                    zone_type,
                    slot_name,
                    slot_name,
                    zone_type,
                    zone_type);
            }
        }
    }

    type_check_world_states(node, ctx);

    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *activate = activations[i];
        const char *zone_slot_name = ast_world_directive_zone_slot_name(activate);
        if (ast_world_directive_state_name(activate) != NULL) {
            if (!resolve_world_zone_state(node, activate,
                    ast_world_directive_state_name(activate), ctx, "activate",
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
                name != NULL ? name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < activate_count; j++) {
            ASTNode *other = activations[j];
            const char *other_zone = ast_world_directive_zone_slot_name(other);
            if (ast_world_directive_state_name(other) != NULL) {
                resolve_world_zone_state(node, other,
                    ast_world_directive_state_name(other), ctx, "activate",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < deactivate_count; j++) {
            ASTNode *deactivate = deactivations[j];
            const char *other_zone = ast_world_directive_zone_slot_name(deactivate);
            if (ast_world_directive_state_name(deactivate) != NULL) {
                resolve_world_zone_state(node, deactivate,
                    ast_world_directive_state_name(deactivate), ctx, "deactivate",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < maintained_zone_count; j++) {
            ASTNode *maintain = maintained_zones[j];
            const char *other_zone = ast_world_directive_zone_slot_name(maintain);
            if (ast_world_directive_state_name(maintain) != NULL) {
                resolve_world_zone_state(node, maintain,
                    ast_world_directive_state_name(maintain), ctx, "maintain",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *deactivate = deactivations[i];
        const char *zone_slot_name = ast_world_directive_zone_slot_name(deactivate);
        if (ast_world_directive_state_name(deactivate) != NULL) {
            if (!resolve_world_zone_state(node, deactivate,
                    ast_world_directive_state_name(deactivate), ctx, "deactivate",
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
                name != NULL ? name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < deactivate_count; j++) {
            ASTNode *other = deactivations[j];
            const char *other_zone = ast_world_directive_zone_slot_name(other);
            if (ast_world_directive_state_name(other) != NULL) {
                resolve_world_zone_state(node, other,
                    ast_world_directive_state_name(other), ctx, "deactivate",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *maintain = maintained_zones[i];
        const char *zone_slot_name = ast_world_directive_zone_slot_name(maintain);
        const char *state_name = ast_world_directive_state_name(maintain);
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
                name != NULL ? name : "<world>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < maintained_zone_count; j++) {
            ASTNode *other = maintained_zones[j];
            const char *other_zone = ast_world_directive_zone_slot_name(other);
            if (ast_world_directive_state_name(other) != NULL) {
                resolve_world_zone_state(node, other,
                    ast_world_directive_state_name(other), ctx, "maintain", &other_zone);
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < deactivate_count; j++) {
            ASTNode *deactivate = deactivations[j];
            const char *other_zone = ast_world_directive_zone_slot_name(deactivate);
            if (ast_world_directive_state_name(deactivate) != NULL) {
                resolve_world_zone_state(node, deactivate,
                    ast_world_directive_state_name(deactivate), ctx, "deactivate",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < activate_count; j++) {
            ASTNode *activate = activations[j];
            const char *other_zone = ast_world_directive_zone_slot_name(activate);
            if (ast_world_directive_state_name(activate) != NULL) {
                resolve_world_zone_state(node, activate,
                    ast_world_directive_state_name(activate), ctx, "activate",
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
                    name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (ast_party_shared_type(shared) != NULL)
            world_resolve_type_ref(ast_party_shared_type(shared), ctx);
        if (ast_party_shared_initializer(shared) != NULL)
            type_check_expression(ast_party_shared_initializer(shared), ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < method_count; i++) {
        type_check_func_decl(methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_world = saved_world;
    return !ctx->has_error;
}
