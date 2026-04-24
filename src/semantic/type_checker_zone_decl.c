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
    Type *resolved;
    ASTNode *type_ref;
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return NULL;
    type_ref = slot->data.domain_slot.type;
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_ref);
    if (resolved != NULL)
        return resolved;
    return resolve_type_node(type_ref, ctx);
}

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    const char *prev_module_path = ctx->current_module_path;
    size_t subject_count;
    size_t object_count;
    size_t mutation_rule_count;
    ctx->current_zone = node;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.zone_decl.name,
        SYMBOL_ZONE,
        node->data.zone_decl.shared_fields,
        node->data.zone_decl.shared_count,
        node->data.zone_decl.methods,
        node->data.zone_decl.method_count,
        "zone");

    ok = type_check_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "zone") && ok;
    subject_count = count_subject_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    object_count = count_object_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);

    if (subject_count > 4) {
        semantic_warning(ctx, node,
            "Zone '%s' declares %llu subject slots; prefer keeping active subjects to 4 or fewer and model supporting state as objects",
            node->data.zone_decl.name,
            (unsigned long long) subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has multiple subject slots but no object slots; consider modeling passive support state as objects",
            node->data.zone_decl.name);
    }

    mutation_rule_count = node->data.zone_decl.apply_count
        + node->data.zone_decl.link_count
        + node->data.zone_decl.detach_count
        + node->data.zone_decl.unlink_count
        + node->data.zone_decl.refresh_count
        + node->data.zone_decl.maintained_effect_count
        + node->data.zone_decl.maintained_relation_count
        + node->data.zone_decl.maintained_state_count;

    if (subject_count == 0
        && (mutation_rule_count > 0 || node->data.zone_decl.authority_count > 0)) {
        semantic_warning(ctx, node,
            "Zone '%s' mutates state or declares authority but has no subject slot",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
        ASTNode *authority = node->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(node, authority->data.zone_authority.subject_slot_name);
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
        for (size_t j = i + 1; j < node->data.zone_decl.authority_count; j++) {
            ASTNode *other = node->data.zone_decl.authorities[j];
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
                    node->data.zone_decl.name,
                    authority->data.zone_authority.subject_slot_name,
                    authority->data.zone_authority.subject_slot_name);
            }
        }
    }

    if (mutation_rule_count > 0 && node->data.zone_decl.authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = node->data.zone_decl.layer_slots[i];
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
                 && explicit_type_reference_allowed(decl, node, ctx))) {
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
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

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        const char *effect_slot_name = apply->data.zone_apply.effect_slot_name;
        const char *target_slot_name = apply->data.zone_apply.target_slot_name;
        const char *state_name = apply->data.zone_apply.state_name;
        const char *participant_slot_name = apply->data.zone_apply.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx, "apply",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply,
                "Zone apply references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- apply mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply,
                "Zone apply references unknown target slot '%s'.\n"
                "Reason:\n"
                "- apply must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, apply, participant_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        const char *relation_slot_name = link->data.zone_link.relation_slot_name;
        const char *left_slot_name = link->data.zone_link.left_slot_name;
        const char *right_slot_name = link->data.zone_link.right_slot_name;
        const char *state_name = link->data.zone_link.state_name;
        const char *participant_slot_name = link->data.zone_link.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx, "link",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- link mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown left slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown right slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_participant_authority(node, link, participant_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        const char *effect_slot_name = detach->data.zone_detach.effect_slot_name;
        const char *target_slot_name = detach->data.zone_detach.target_slot_name;
        const char *state_name = detach->data.zone_detach.state_name;
        const char *participant_slot_name = detach->data.zone_detach.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx, "detach",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, detach,
                "Zone detach references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- detach mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, detach,
                "Zone detach references unknown target slot '%s'.\n"
                "Reason:\n"
                "- detach must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, detach,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, detach, participant_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        const char *relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
        const char *left_slot_name = unlink->data.zone_unlink.left_slot_name;
        const char *right_slot_name = unlink->data.zone_unlink.right_slot_name;
        const char *state_name = unlink->data.zone_unlink.state_name;
        const char *participant_slot_name = unlink->data.zone_unlink.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx, "unlink",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- unlink mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown left slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown right slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_participant_authority(node, unlink, participant_slot_name, ctx, "unlink");
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        const char *object_slot_name = refresh->data.zone_refresh.object_slot_name;
        const char *source_slot_name = refresh->data.zone_refresh.source_slot_name;
        const char *participant_slot_name = refresh->data.zone_refresh.participant_slot_name;
        ASTNode *target_slot = NULL;
        bool boundary_projection = false;
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (find_zone_domain_slot(node, object_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, refresh,
                "Zone %s references unknown target slot '%s'.\n"
                "Reason:\n"
                "- the projection target must already be declared in the zone\n"
                "- '%s' is not a known object/tobject slot in zone '%s'\n"
                "Fix:\n"
                "- declare object/tobject slot '%s' in the zone first\n"
                "- or change %s to an existing projection target slot",
                action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                action_name);
        }
        if (find_zone_domain_slot(node, source_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, refresh,
                "Zone %s references unknown source slot '%s'.\n"
                "Reason:\n"
                "- projection sync must read from a declared zone slot\n"
                "- '%s' is not a known source slot in zone '%s'\n"
                "Fix:\n"
                "- declare source slot '%s' in the zone first\n"
                "- or change %s to an existing source slot",
                action_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<slot>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                source_slot_name != NULL ? source_slot_name : "<slot>",
                action_name);
        }
        target_slot = find_zone_domain_slot(node, object_slot_name);
        boundary_projection = target_slot != NULL
            && target_slot->type == AST_DOMAIN_SLOT
            && target_slot->data.domain_slot.is_tobject;
        type_check_zone_projection_contract(node, refresh,
            object_slot_name, source_slot_name, ctx, action_name);
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            if (boundary_projection) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, refresh,
                    "Zone %s to boundary target '%s' must specify 'by <subjectSlot>' when authority is declared.\n"
                    "Reason:\n"
                    "- boundary projection publishes authority-bearing state across the zone edge\n"
                    "- zone '%s' declares authority, so provenance must name the approving subject slot\n"
                    "Contract source:\n"
                    "- zone authority declaration on this zone\n"
                    "- boundary projection publish/bind/refresh requires an approving subject slot\n"
                    "Fix:\n"
                    "- add 'by <subjectSlot>' to this %s clause\n"
                    "- or publish into a non-authority zone",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    action_name);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, refresh,
                    "Zone %s must specify 'by <subjectSlot>' when authority is declared.\n"
                    "Reason:\n"
                    "- the zone declares authority and this projection mutates local derived state\n"
                    "- without an explicit participant, contract provenance becomes harder to explain at diagnostics/runtime\n"
                    "Contract source:\n"
                    "- zone authority declaration on this zone\n"
                    "- projection sync mutates derived zone state and requires an approving subject slot\n"
                    "Fix:\n"
                    "- add 'by <subjectSlot>' to this %s clause\n"
                    "- or remove the authority declaration if this projection is intentionally authority-free",
                    action_name,
                    action_name);
            }
        }
        type_check_zone_participant_authority(node, refresh, participant_slot_name, ctx, action_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        const char *effect_slot_name = maintain->data.zone_maintain_effect.effect_slot_name;
        const char *target_slot_name = maintain->data.zone_maintain_effect.target_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_effect.participant_slot_name;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps an effect lifecycle active and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown target slot '%s'.\n"
                "Reason:\n"
                "- maintain must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<effect>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_effect_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_effects[j];
            if (strcmp(effect_slot_name, other->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(target_slot_name, other->data.zone_maintain_effect.target_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for effect '%s' on '%s'\n"
                    "- or split the contract if the lifecycle targets are actually different",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
            ASTNode *detach = node->data.zone_decl.detaches[j];
            const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
            const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
            if (detach->data.zone_detach.state_name != NULL) {
                resolve_zone_effect_state(node, detach,
                    detach->data.zone_detach.state_name, ctx, "detach",
                    &detach_effect_slot_name, &detach_target_slot_name);
            }
            if (detach_effect_slot_name != NULL
                && detach_target_slot_name != NULL
                && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while detach removes the same lifecycle path\n"
                    "- the current zone contract points in two different directions for one effect target pair\n"
                    "Fix:\n"
                    "- keep either maintain or detach for effect '%s' on '%s'\n"
                    "- or split the rules so they target different slots/states",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        const char *relation_slot_name = maintain->data.zone_maintain_relation.relation_slot_name;
        const char *left_slot_name = maintain->data.zone_maintain_relation.left_slot_name;
        const char *right_slot_name = maintain->data.zone_maintain_relation.right_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_relation.participant_slot_name;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps a relation lifecycle active and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown left slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown right slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
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
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<relation>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_relation_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_relations[j];
            if (strcmp(relation_slot_name, other->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(left_slot_name, other->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(right_slot_name, other->data.zone_maintain_relation.right_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same relation lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for relation '%s' between '%s' and '%s'\n"
                    "- or split the contract if the relation endpoints are actually different",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
            ASTNode *unlink = node->data.zone_decl.unlinks[j];
            const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
            const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
            const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
            if (unlink->data.zone_unlink.state_name != NULL) {
                resolve_zone_relation_state(node, unlink,
                    unlink->data.zone_unlink.state_name, ctx, "unlink",
                    &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
            }
            if (unlink_relation_slot_name != NULL
                && unlink_left_slot_name != NULL
                && unlink_right_slot_name != NULL
                && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                && strcmp(left_slot_name, unlink_left_slot_name) == 0
                && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while unlink removes the same relation path\n"
                    "- the current zone contract points in two different directions for one relation edge\n"
                    "Fix:\n"
                    "- keep either maintain or unlink for relation '%s' between '%s' and '%s'\n"
                    "- or split the rules so they target different relation states",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        ASTNode *state;
        const char *state_name = maintain->data.zone_maintain_state.state_name;
        const char *participant_slot_name = maintain->data.zone_maintain_state.participant_slot_name;
        state = find_zone_state(node, state_name);
        if (state == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown state '%s'.\n"
                "Reason:\n"
                "- maintain state aliases must reference a declared zone state\n"
                "- zone '%s' does not declare state '%s'\n"
                "Fix:\n"
                "- declare state '%s' before this maintain clause\n"
                "- or change maintain to an existing state alias",
                state_name != NULL ? state_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<unknown>",
                state_name != NULL ? state_name : "<unknown>");
        } else if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
                ASTNode *unlink = node->data.zone_decl.unlinks[j];
                const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
                const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
                const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
                if (unlink->data.zone_unlink.state_name != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        unlink->data.zone_unlink.state_name, ctx, "unlink",
                        &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
                }
                if (unlink_relation_slot_name != NULL
                    && unlink_left_slot_name != NULL
                    && unlink_right_slot_name != NULL
                    && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                    && strcmp(left_slot_name, unlink_left_slot_name) == 0
                    && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and unlinks state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named relation-state active, while unlink removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or unlink for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
                ASTNode *detach = node->data.zone_decl.detaches[j];
                const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
                const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
                if (detach->data.zone_detach.state_name != NULL) {
                    resolve_zone_effect_state(node, detach,
                        detach->data.zone_detach.state_name, ctx, "detach",
                        &detach_effect_slot_name, &detach_target_slot_name);
                }
                if (detach_effect_slot_name != NULL
                    && detach_target_slot_name != NULL
                    && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                    && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and detaches state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named effect-state active, while detach removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or detach for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        }
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps named state '%s' active\n"
                "- persistent named-state lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<state>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *state_name = state->data.zone_state.state_name;
        if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown relation slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as a relation-backed lifecycle alias\n"
                    "- zone '%s' does not declare relation slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing relation slot in state '%s'\n"
                    "- or declare relation slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, left_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown left slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared left endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the left endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, right_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown right slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared right endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the right endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown effect slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as an effect-backed lifecycle alias\n"
                    "- zone '%s' does not declare effect slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing effect slot in state '%s'\n"
                    "- or declare effect slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, target_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown target slot '%s'.\n"
                    "Reason:\n"
                    "- effect state '%s' needs a declared target slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the target\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < node->data.zone_decl.state_count; j++) {
            ASTNode *other = node->data.zone_decl.states[j];
            if (state_name != NULL
                && other != NULL
                && other->data.zone_state.state_name != NULL
                && strcmp(state_name, other->data.zone_state.state_name) == 0) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_REDECLARATION,
                    PGY_CAUSE_ZONE_STATE_DUPLICATE_NAME,
                    PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                    other,
                    "Redeclaration of zone state '%s'",
                    state_name);
            }
        }
    }

    ctx->current_zone = saved_zone;
    ctx->current_module_path = prev_module_path;
    return ok && !ctx->has_error;
}
