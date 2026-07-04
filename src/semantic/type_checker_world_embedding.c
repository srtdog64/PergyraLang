/* Containment boundary-fork checks (docs/157, theorem T):
 * a live binding crossing a containment boundary (world > zone) forks or
 * exposes identity and must declare it -- Clone is the declared copy,
 * Channel the declared passage; undeclared crossings fail closed.
 * Two enforced faces live here:
 *   - world constructor embedding of a live zone binding   (original check)
 *   - zone constructor embedding of a live subject binding (S1, AC-3)
 * The S2 face (world member escape) is measured but deliberately not
 * enforced yet -- see the note at the end of this file.
 */
#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *world_decl;
    const char *world_name = NULL;

    if (call == NULL || call->type != AST_CALL || ctx == NULL
        || ast_call_callee(call) == NULL
        || ast_call_callee(call)->type != AST_IDENTIFIER) {
        return;
    }

    world_name = ast_identifier_name(ast_call_callee(call));
    world_decl = semantic_find_world_decl_by_name(ctx, world_name);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(world_decl, &zone_count);

    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        ASTNode *arg = ast_call_argument(call, i);
        const char *arg_name;
        Symbol *arg_sym;
        bool matched_zone_slot = false;

        if (arg == NULL || arg->type != AST_IDENTIFIER)
            continue;

        arg_name = ast_identifier_name(arg);
        arg_sym = scope_lookup(ctx->scope, arg_name);
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
                    arg_name != NULL ? arg_name : "<zone>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg_name,
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
                    arg_name != NULL ? arg_name : "<zone>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot_name != NULL ? zone_slot_name : "<slot>",
                    arg_name != NULL ? arg_name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg_name,
                    world_name,
                    zone_slot_name);
                break;
            }
        }
    }
}

/* S1 (AC-3): a zone constructor taking a live subject binding forks the
 * subject's identity into the zone-owned slot with no declaration -- the
 * zone-level twin of the world embedding check above. Clone(arg) or an
 * inline constructor argument is the declared form. */
void
semantic_reject_zone_subject_embedding(ASTNode *call, ASTNode *zone_decl,
                                       const char *zone_name,
                                       SemanticContext *ctx)
{
    size_t slot_count = 0;
    ASTNode **slots;

    if (call == NULL || call->type != AST_CALL || zone_decl == NULL
        || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    slots = ast_zone_slots(zone_decl, &slot_count);

    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        ASTNode *arg = ast_call_argument(call, i);
        const char *arg_name;
        Symbol *arg_sym;

        if (arg == NULL || arg->type != AST_IDENTIFIER)
            continue;
        arg_name = ast_identifier_name(arg);
        arg_sym = scope_lookup(ctx->scope, arg_name);
        if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE
            || arg_sym->type == NULL || arg_sym->type->name == NULL)
            continue;

        for (size_t si = 0; si < slot_count; si++) {
            ASTNode *slot = slots[si];
            Type *slot_type;
            const char *slot_name;

            if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                || !ast_domain_slot_is_subject(slot))
                continue;
            slot_name = ast_domain_slot_name(slot);
            slot_type = domain_lookup_slot_type_metadata(slot, ctx);
            if (slot_type == NULL || slot_type->name == NULL)
                continue;
            if (strcmp(arg_sym->type->name, slot_type->name) != 0)
                continue;
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                arg,
                "Zone constructor '%s' implicitly copies subject binding '%s' into slot '%s'.\n"
                "Reason:\n"
                "- a live subject binding crossing a containment boundary forks its identity\n"
                "- the fork is undeclared: '%s' and the zone-owned slot diverge silently afterwards\n"
                "- the boundary-fork rule requires the copy to be declared (docs/157)\n"
                "Fix:\n"
                "- use Clone(%s) for a declared, detached copy\n"
                "- or construct the subject inline in the constructor argument",
                zone_name != NULL ? zone_name : "<zone>",
                arg_name != NULL ? arg_name : "<subject>",
                slot_name != NULL ? slot_name : "<slot>",
                arg_name != NULL ? arg_name : "<subject>",
                arg_name != NULL ? arg_name : "<subject>");
            break;
        }
    }
}

/* S2 (AC-3, world member escape) is intentionally NOT enforced yet.
 * The corpus audit (2026-07-05) found the flagship examples' dominant
 * idiom passes world-owned zones through plain API-layer functions
 * (shopping_mall 7 sites, logistics 3, abi_pipeline/backend_compare
 * fixtures) -- blanket escape rejection would convert them all and shift
 * write-through-dependent goldens. The enforcement scope (full theorem-T
 * escape rule vs cross-world-mix-only) is a BDFL re-ratification cell;
 * see docs/157 and the axis-composition smoke, which keeps the measured
 * silent write-through (2/1/2) pinned until the direction lands. */
