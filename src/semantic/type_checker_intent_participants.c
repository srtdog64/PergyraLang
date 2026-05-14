#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"

#include <stdbool.h>

void
type_check_intent_step_participant_contract(ASTNode *intent_decl,
                                            ASTNode *step,
                                            ASTNode *zone_decl,
                                            bool *matched_action,
                                            SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP
        || ctx == NULL) {
        return;
    }

    for (size_t j = 0; j < ast_intent_step_who_count(step); j++) {
        const char *alias = ast_intent_step_who_names(step, NULL)[j];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *participant_type_name = NULL;
        if (involves == NULL) {
            const char *source = ast_intent_step_inherited_who_from_intent(step)
                ? " inherited from the intent-level who default"
                : (ast_intent_step_derived_who_from_single_participant(step)
                    ? " derived from the single subject participant"
                    : "");
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' refers to unknown participant '%s'%s.\n"
                "Reason:\n"
                "- who must name an intent participant declared in the intent parameter list, who <alias>: <Subject>;, or involves <alias>: <Subject>;\n"
                "- this step cannot be checked until the participant alias is bound to a subject type\n"
                "Fix:\n"
                "- declare the participant with 'who %s: <Subject>;' or add it to the intent parameter list\n"
                "- or change the intent-level who default to an existing participant alias",
                ast_intent_step_name(step) != NULL ? ast_intent_step_name(step) : "<step>",
                alias != NULL ? alias : "<participant>",
                source,
                alias != NULL ? alias : "<participant>");
            continue;
        }

        participant_type_name = intent_involves_type_name(involves);
        if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' who participant '%s' must bind to a subject type",
                ast_intent_step_name(step) != NULL ? ast_intent_step_name(step) : "<step>",
                alias != NULL ? alias : "<participant>");
            continue;
        }
        if (zone_decl != NULL && participant_type_name != NULL) {
            ASTNode **zone_slots;
            size_t zone_slot_count;
            zone_slots = ast_zone_slots(zone_decl, &zone_slot_count);
            if (!domain_has_subject_slot_type(
                    zone_slots, zone_slot_count, ctx, participant_type_name)) {
            const char *zone_name = ast_zone_name(zone_decl);
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' binds participant '%s' of type '%s', but zone '%s' has no matching subject slot",
                ast_intent_step_name(step) != NULL ? ast_intent_step_name(step) : "<step>",
                alias != NULL ? alias : "<participant>",
                participant_type_name,
                zone_name != NULL ? zone_name : "<zone>");
            }
        }
        if (ast_intent_step_transfer_from_alias(step) != NULL && participant_type_name != NULL) {
            ASTNode *from_involves = find_intent_involves_local(intent_decl,
                ast_intent_step_transfer_from_alias(step));
            Type *from_type = from_involves != NULL
                ? intent_resolve_involves_type(from_involves, ctx) : NULL;
            ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                from_type != NULL ? from_type->name : NULL);
            if (from_zone_decl != NULL) {
                ASTNode **from_zone_slots;
                size_t from_zone_slot_count;
                from_zone_slots = ast_zone_slots(from_zone_decl,
                                                 &from_zone_slot_count);
                if (!domain_has_subject_slot_type(
                        from_zone_slots, from_zone_slot_count, ctx,
                        participant_type_name)) {
                const char *from_zone_name = ast_zone_name(from_zone_decl);
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer source zone '%s' has no matching subject slot for participant '%s' of type '%s'.\n"
                    "Reason:\n"
                    "- contract source is transfer handoff edge '%s' -> '%s'\n"
                    "- handoff source zone '%s' must be able to host participant '%s' before transfer can materialize\n"
                    "- participant '%s' has type '%s', but zone '%s' declares no matching subject slot for that type\n"
                    "Fix:\n"
                    "- add a subject slot of type '%s' to zone '%s'\n"
                    "- or use a participant whose type matches one of zone '%s' subject slots\n"
                    "- or change the transfer source zone binding",
                    ast_intent_step_name(step) != NULL ? ast_intent_step_name(step) : "<step>",
                    from_zone_name != NULL ? from_zone_name : "<zone>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    ast_intent_step_transfer_from_alias(step) != NULL
                        ? ast_intent_step_transfer_from_alias(step) : "<sourceZone>",
                    ast_intent_step_transfer_to_alias(step) != NULL
                        ? ast_intent_step_transfer_to_alias(step) : "<targetZone>",
                    from_zone_name != NULL ? from_zone_name : "<zone>",
                    alias != NULL ? alias : "<participant>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    from_zone_name != NULL ? from_zone_name : "<zone>",
                    participant_type_name,
                    from_zone_name != NULL ? from_zone_name : "<zone>",
                    from_zone_name != NULL ? from_zone_name : "<zone>");
                }
            }
        }

        ASTNode *subject_type = ast_intent_involves_subject_type(involves);
        if (subject_type != NULL
            && subject_type->type == AST_TYPE
            && subject_type->data.type.name != NULL) {
            ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root,
                subject_type->data.type.name);
            if (subject_decl_has_action_named(subject_decl, ast_intent_step_name(step))
                && matched_action != NULL) {
                *matched_action = true;
            }
        }
    }
}
