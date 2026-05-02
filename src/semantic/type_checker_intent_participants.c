#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"

#include <stdbool.h>

static Type *
intent_participant_resolve_involves_type(ASTNode *involves, SemanticContext *ctx)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL) {
        return TYPE_UNKNOWN;
    }
    return semantic_type_resolution_lookup_annotation_or_unknown(
        ctx, involves->data.intent_involves.subject_type);
}

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

    for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
        const char *alias = step->data.intent_step.who_names[j];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *participant_type_name = NULL;
        if (involves == NULL) {
            const char *source = step->data.intent_step.inherited_who_from_intent
                ? " inherited from the intent-level who default"
                : "";
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' refers to unknown participant '%s'%s.\n"
                "Reason:\n"
                "- who must name an intent participant declared in the intent parameter list, who <alias>: <Subject>;, or involves <alias>: <Subject>;\n"
                "- this step cannot be checked until the participant alias is bound to a subject type\n"
                "Fix:\n"
                "- declare the participant with 'who %s: <Subject>;' or add it to the intent parameter list\n"
                "- or change the intent-level who default to an existing participant alias",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                alias != NULL ? alias : "<participant>",
                source,
                alias != NULL ? alias : "<participant>");
            continue;
        }

        participant_type_name = intent_involves_type_name(involves);
        if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' who participant '%s' must bind to a subject type",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                alias != NULL ? alias : "<participant>");
            continue;
        }
        if (zone_decl != NULL && participant_type_name != NULL
            && !domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' binds participant '%s' of type '%s', but zone '%s' has no matching subject slot",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                alias != NULL ? alias : "<participant>",
                participant_type_name,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
        }
        if (step->data.intent_step.transfer_from_alias != NULL && participant_type_name != NULL) {
            ASTNode *from_involves = find_intent_involves_local(intent_decl,
                step->data.intent_step.transfer_from_alias);
            Type *from_type = from_involves != NULL
                ? intent_participant_resolve_involves_type(from_involves, ctx) : NULL;
            ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                from_type != NULL ? from_type->name : NULL);
            if (from_zone_decl != NULL
                && !domain_has_subject_slot_type(from_zone_decl->data.zone_decl.slots,
                    from_zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
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
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_zone_decl->data.zone_decl.name != NULL
                        ? from_zone_decl->data.zone_decl.name : "<zone>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    step->data.intent_step.transfer_from_alias != NULL
                        ? step->data.intent_step.transfer_from_alias : "<sourceZone>",
                    step->data.intent_step.transfer_to_alias != NULL
                        ? step->data.intent_step.transfer_to_alias : "<targetZone>",
                    from_zone_decl->data.zone_decl.name != NULL
                        ? from_zone_decl->data.zone_decl.name : "<zone>",
                    alias != NULL ? alias : "<participant>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    from_zone_decl->data.zone_decl.name != NULL
                        ? from_zone_decl->data.zone_decl.name : "<zone>",
                    participant_type_name,
                    from_zone_decl->data.zone_decl.name != NULL
                        ? from_zone_decl->data.zone_decl.name : "<zone>",
                    from_zone_decl->data.zone_decl.name != NULL
                        ? from_zone_decl->data.zone_decl.name : "<zone>");
            }
        }

        if (involves->data.intent_involves.subject_type != NULL
            && involves->data.intent_involves.subject_type->type == AST_TYPE
            && involves->data.intent_involves.subject_type->data.type.name != NULL) {
            ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root,
                involves->data.intent_involves.subject_type->data.type.name);
            if (subject_decl_has_action_named(subject_decl, step->data.intent_step.name)
                && matched_action != NULL) {
                *matched_action = true;
            }
        }
    }
}
