#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_module_contract_diag_internal.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

static Type *
zone_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return NULL;
    type_ref = slot->data.domain_slot.type;
    return semantic_type_resolution_lookup_annotation_or_unknown(ctx, type_ref);
}

void
type_check_zone_authorities(ASTNode *zone, SemanticContext *ctx)
{
    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone,
            authority->data.zone_authority.subject_slot_name);
        if (slot == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, authority,
                "Zone authority references unknown subject slot '%s'",
                authority->data.zone_authority.subject_slot_name);
            continue;
        }
        if (!slot->data.domain_slot.is_subject) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, authority,
                "Zone authority '%s' must reference a subject slot",
                authority->data.zone_authority.subject_slot_name);
        }
        slot_type = zone_resolve_domain_slot_type(slot, ctx);
        for (size_t j = 0; j < authority->data.zone_authority.ability_count; j++) {
            ASTNode *ability_ref = authority->data.zone_authority.required_abilities[j];
            const char *ability_name = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                authority,
                authority->data.zone_authority.subject_slot_name != NULL
                    ? authority->data.zone_authority.subject_slot_name : "<authority>",
                ability_ref,
                "zone authority ability consumer lookup");
            if (resolve_required_ability_decl(
                    ability_ref, authority, ctx, "Zone authority",
                    authority->data.zone_authority.subject_slot_name) == NULL) {
                free(required_text);
                continue;
            }
            if (slot_type == NULL || slot_type == TYPE_UNKNOWN
                || slot_type->name == NULL) {
                free(required_text);
                continue;
            }
            if (!subject_type_has_ability(ctx->program_root, slot_type->name, ability_ref)) {
                ASTNode *actual_impl = subject_type_find_base_ability_impl(
                    ctx->program_root, slot_type->name, ability_name);
                char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                report_subject_ability_requirement_mismatch(
                    ctx, authority, "Zone authority",
                    authority->data.zone_authority.subject_slot_name,
                    "subject type", slot_type->name,
                    required_text != NULL ? required_text : ability_name,
                    actual_text,
                    "change/remove the zone authority requirement");
                free(actual_text);
            }
            free(required_text);
        }
        for (size_t j = i + 1; j < zone->data.zone_decl.authority_count; j++) {
            ASTNode *other = zone->data.zone_decl.authorities[j];
            if (other != NULL
                && other->data.zone_authority.subject_slot_name != NULL
                && strcmp(authority->data.zone_authority.subject_slot_name,
                          other->data.zone_authority.subject_slot_name) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, other,
                    "Zone '%s' declares authority '%s' more than once.\n"
                    "Reason:\n"
                    "- zone authority must be uniquely attributable per subject slot\n"
                    "- duplicate authority declarations make provenance and diagnostics ambiguous\n"
                    "Fix:\n"
                    "- keep one authority declaration for '%s'\n"
                    "- or merge the required ability list into a single authority clause",
                    zone->data.zone_decl.name,
                    authority->data.zone_authority.subject_slot_name,
                    authority->data.zone_authority.subject_slot_name);
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
    const char *zone_name = zone != NULL && zone->data.zone_decl.name != NULL
        ? zone->data.zone_decl.name : "<zone>";
    const char *action = action_name != NULL ? action_name : "<action>";
    const char *kind = lifecycle_kind != NULL ? lifecycle_kind : "effect";

    if (zone == NULL || site == NULL || ctx == NULL
        || zone->data.zone_decl.authority_count == 0
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
    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone->data.zone_decl.layer_slots[i];
        const char *type_name = layer_slot->data.zone_layer_slot.layer_type;
        Symbol *sym = type_name != NULL ? scope_lookup(ctx->scope, type_name) : NULL;
        ASTNodeType decl_type = layer_slot->data.zone_layer_slot.is_relation
            ? AST_RELATION_DECL
            : AST_EFFECT_DECL;
        ASTNode *decl = type_name != NULL
            ? find_domain_decl_by_name(ctx->program_root, decl_type, type_name)
            : NULL;
        SymbolKind expected = layer_slot->data.zone_layer_slot.is_relation
            ? SYMBOL_RELATION
            : SYMBOL_EFFECT;
        const char *kind_name = layer_slot->data.zone_layer_slot.is_relation
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
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<slot>",
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                kind_name,
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
                kind_name);
        }
        if (layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, layer_slot,
                "Zone relation pool '%s' is not supported yet.\n"
                "Reason:\n"
                "- relation pools need explicit propagation/ownership semantics before backend emission can be trusted\n"
                "Contract source:\n"
                "- zone relation pool declaration\n"
                "Fix:\n"
                "- declare individual relation slots for the beta stable subset\n"
                "- or move pooled relation semantics to beta-out-of-scope documentation",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
        if (!layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool
            && layer_slot->data.zone_layer_slot.pool_capacity <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, layer_slot,
                "Zone effect pool '%s' must declare a positive capacity.\n"
                "Reason:\n"
                "- effect pool capacity participates in lifecycle propagation bounds\n"
                "Contract source:\n"
                "- zone effect pool declaration\n"
                "Fix:\n"
                "- choose a positive pool capacity\n"
                "- or use a non-pool effect slot for the beta stable subset",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
    }
}
