#include "type_checker_internal.h"
#include "diag_codes.h"

void
type_check_zone_projection_rules(ASTNode *node, SemanticContext *ctx)
{
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

}
