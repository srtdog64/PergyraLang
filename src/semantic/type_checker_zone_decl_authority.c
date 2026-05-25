#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_module_contract_diag_internal.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

void
type_check_zone_authorities(ASTNode *zone, SemanticContext *ctx)
{
    ASTNode **authorities;
    size_t authority_count;

    authorities = ast_zone_authorities(zone, &authority_count);
    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities[i];
        const char *subject_slot = ast_zone_authority_subject_slot_name(authority);
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL || subject_slot == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone, subject_slot);
        if (slot == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, authority,
                "Zone authority references unknown subject slot '%s'",
                subject_slot);
            continue;
        }
        if (!ast_domain_slot_is_subject(slot)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, authority,
                "Zone authority '%s' must reference a subject slot",
                subject_slot);
        }
        slot_type = domain_lookup_slot_type_metadata(slot, ctx);
        for (size_t j = 0; j < ast_zone_authority_ability_count(authority); j++) {
            ASTNode *ability_ref = ast_zone_authority_required_ability(authority, j);
            const char *ability_name = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                authority,
                subject_slot != NULL ? subject_slot : "<authority>",
                ability_ref,
                "zone authority ability consumer lookup");
            if (resolve_required_ability_decl(
                    ability_ref, authority, ctx, "Zone authority",
                    subject_slot) == NULL) {
                free(required_text);
                continue;
            }
            if (slot_type == NULL || slot_type == TYPE_UNKNOWN
                || slot_type->name == NULL) {
                free(required_text);
                continue;
            }
            if (!semantic_subject_type_has_ability(ctx, slot_type->name, ability_ref)) {
                ASTNode *actual_impl =
                    semantic_subject_type_find_base_ability_impl(
                        ctx, slot_type->name, ability_name);
                char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                report_subject_ability_requirement_mismatch(
                    ctx, authority, "Zone authority",
                    subject_slot,
                    "subject type", slot_type->name,
                    required_text != NULL ? required_text : ability_name,
                    actual_text,
                    "change/remove the zone authority requirement");
                free(actual_text);
            }
            free(required_text);
        }
        for (size_t j = i + 1; j < authority_count; j++) {
            ASTNode *other = authorities[j];
            const char *other_subject_slot =
                ast_zone_authority_subject_slot_name(other);
            if (other != NULL
                && other_subject_slot != NULL
                && strcmp(subject_slot, other_subject_slot) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, other,
                    "Zone '%s' declares authority '%s' more than once.\n"
                    "Reason:\n"
                    "- zone authority must be uniquely attributable per subject slot\n"
                    "- duplicate authority declarations make provenance and diagnostics ambiguous\n"
                    "Fix:\n"
                    "- keep one authority declaration for '%s'\n"
                    "- or merge the required ability list into a single authority clause",
                    ast_zone_name(zone),
                    subject_slot,
                    subject_slot);
            }
        }
    }
}

void
type_check_zone_lifecycle_authority_presence(ASTNode *zone,
                                             ASTNode *site,
                                             const char *participant_slot_name,
                                             SemanticContext *ctx,
                                             const char *action_name,
                                             const char *lifecycle_kind,
                                             const char *primary_slot_name,
                                             const char *secondary_slot_name)
{
    size_t authority_count;
    const char *zone_name = zone != NULL && ast_zone_name(zone) != NULL
        ? ast_zone_name(zone) : "<zone>";
    const char *action = action_name != NULL ? action_name : "<action>";
    const char *kind = lifecycle_kind != NULL ? lifecycle_kind : "effect";
    ast_zone_authorities(zone, &authority_count);

    if (zone == NULL || site == NULL || ctx == NULL
        || authority_count == 0
        || participant_slot_name != NULL) {
        return;
    }

    if (strcmp(action, "apply") == 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone apply must specify 'by <subjectSlot>' when authority is declared.\n"
            "Reason:\n"
            "- zone '%s' declares authority and apply mutates effect state on '%s'\n"
            "- without an approving subject slot, contract provenance becomes incomplete\n"
            "Contract source:\n"
            "- zone authority declaration on this zone\n"
            "- apply effect lifecycle mutation requires an approving subject slot\n"
            "Fix:\n"
            "- add 'by <subjectSlot>' to this apply clause\n"
            "- or remove zone authority if this rule is intentionally authority-free",
            zone_name,
            secondary_slot_name != NULL ? secondary_slot_name : "<target>");
        return;
    }

    if (strcmp(action, "link") == 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone link must specify 'by <subjectSlot>' when authority is declared.\n"
            "Reason:\n"
            "- zone '%s' declares authority and link mutates relation state\n"
            "- relation provenance must record the approving subject slot\n"
            "Contract source:\n"
            "- zone authority declaration on this zone\n"
            "- link relation lifecycle mutation requires an approving subject slot\n"
            "Fix:\n"
            "- add 'by <subjectSlot>' to this link clause\n"
            "- or remove zone authority if this relation edge is intentionally authority-free",
            zone_name);
        return;
    }

    if (strcmp(action, "detach") == 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone detach must specify 'by <subjectSlot>' when authority is declared.\n"
            "Reason:\n"
            "- zone '%s' declares authority and detach mutates effect lifecycle on '%s'\n"
            "- detachment provenance must record the approving subject slot\n"
            "Contract source:\n"
            "- zone authority declaration on this zone\n"
            "- detach effect lifecycle mutation requires an approving subject slot\n"
            "Fix:\n"
            "- add 'by <subjectSlot>' to this detach clause\n"
            "- or remove zone authority if this detach rule is intentionally authority-free",
            zone_name,
            secondary_slot_name != NULL ? secondary_slot_name : "<target>");
        return;
    }

    if (strcmp(action, "unlink") == 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone unlink must specify 'by <subjectSlot>' when authority is declared.\n"
            "Reason:\n"
            "- zone '%s' declares authority and unlink mutates relation lifecycle\n"
            "- unlink provenance must record the approving subject slot\n"
            "Contract source:\n"
            "- zone authority declaration on this zone\n"
            "- unlink relation lifecycle mutation requires an approving subject slot\n"
            "Fix:\n"
            "- add 'by <subjectSlot>' to this unlink clause\n"
            "- or remove zone authority if this unlink rule is intentionally authority-free",
            zone_name);
        return;
    }

    if (strcmp(action, "maintain") == 0 && strcmp(kind, "relation") == 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
            "Reason:\n"
            "- zone '%s' declares authority and maintain keeps relation '%s' active\n"
            "- persistent relation lifecycle rules must record the approving subject slot\n"
            "Contract source:\n"
            "- zone authority declaration on this zone\n"
            "- maintained relation lifecycle requires an approving subject slot\n"
            "Fix:\n"
            "- add 'by <subjectSlot>' to this maintain clause\n"
            "- or remove zone authority if this maintenance rule is intentionally authority-free",
            zone_name,
            primary_slot_name != NULL ? primary_slot_name : "<relation>");
        return;
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
        "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
        "Reason:\n"
        "- zone '%s' declares authority and maintain keeps effect '%s' active on '%s'\n"
        "- persistent lifecycle rules must record the approving subject slot\n"
        "Contract source:\n"
        "- zone authority declaration on this zone\n"
        "- maintained effect lifecycle requires an approving subject slot\n"
        "Fix:\n"
        "- add 'by <subjectSlot>' to this maintain clause\n"
        "- or remove zone authority if this maintenance rule is intentionally authority-free",
        zone_name,
        primary_slot_name != NULL ? primary_slot_name : "<effect>",
        secondary_slot_name != NULL ? secondary_slot_name : "<target>");
}

void
type_check_zone_layer_slots(ASTNode *zone, SemanticContext *ctx)
{
    ASTNode **layer_slots;
    size_t layer_slot_count;

    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *layer_slot = layer_slots[i];
        const char *type_name = ast_zone_layer_slot_layer_type(layer_slot);
        Symbol *sym = type_name != NULL ? scope_lookup(ctx->scope, type_name) : NULL;
        ASTNodeType decl_type = ast_zone_layer_slot_is_relation(layer_slot)
            ? AST_RELATION_DECL
            : AST_EFFECT_DECL;
        ASTNode *decl = type_name != NULL
            ? (ast_zone_layer_slot_is_relation(layer_slot)
                ? semantic_find_relation_decl_by_name(ctx, type_name)
                : semantic_find_effect_decl_by_name(ctx, type_name))
            : NULL;
        SymbolKind expected = ast_zone_layer_slot_is_relation(layer_slot)
            ? SYMBOL_RELATION
            : SYMBOL_EFFECT;
        const char *kind_name = ast_zone_layer_slot_is_relation(layer_slot)
            ? "relation"
            : "effect";
        if ((sym == NULL || sym->kind != expected)
            && !(decl != NULL
                 && decl->type == decl_type
                 && explicit_type_reference_allowed(decl, zone, ctx))) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, layer_slot,
                "Zone layer slot '%s' references unknown %s type '%s'.\n"
                "Reason:\n"
                "- zone layer slots must bind to declared %s contracts before semantic closure\n"
                "- unresolved layer types would force later lowering/runtime paths to guess the contract\n"
                "Fix:\n"
                "- declare %s '%s' before using it in zone '%s'\n"
                "- or change the layer slot type to an existing %s declaration",
                ast_zone_layer_slot_name(layer_slot) != NULL
                    ? ast_zone_layer_slot_name(layer_slot)
                    : "<slot>",
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                kind_name,
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
                kind_name);
        }
        if (ast_zone_layer_slot_is_relation(layer_slot)
            && ast_zone_layer_slot_is_pool(layer_slot)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, layer_slot,
                "Zone relation pool '%s' is not supported yet.\n"
                "Reason:\n"
                "- relation pools need explicit propagation/ownership semantics before backend emission can be trusted\n"
                "Contract source:\n"
                "- zone relation pool declaration\n"
                "Fix:\n"
                "- declare individual relation slots for the beta stable subset\n"
                "- or move pooled relation semantics to beta-out-of-scope documentation",
                ast_zone_layer_slot_name(layer_slot) != NULL
                    ? ast_zone_layer_slot_name(layer_slot)
                    : "<unknown>");
        }
        if (!ast_zone_layer_slot_is_relation(layer_slot)
            && ast_zone_layer_slot_is_pool(layer_slot)
            && ast_zone_layer_slot_pool_capacity(layer_slot) <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, layer_slot,
                "Zone effect pool '%s' must declare a positive capacity.\n"
                "Reason:\n"
                "- effect pool capacity participates in lifecycle propagation bounds\n"
                "Contract source:\n"
                "- zone effect pool declaration\n"
                "Fix:\n"
                "- choose a positive pool capacity\n"
                "- or use a non-pool effect slot for the beta stable subset",
                ast_zone_layer_slot_name(layer_slot) != NULL
                    ? ast_zone_layer_slot_name(layer_slot)
                    : "<unknown>");
        }
    }
}
