#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"

#include <stdlib.h>

void
type_check_intent_step_ability_contract(ASTNode *intent_decl,
                                        ASTNode *step,
                                        SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || ctx == NULL)
        return;

    for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
        ASTNode *ability_ref = step->data.intent_step.required_abilities[j];
        const char *ability = ability_ref_name(ability_ref);
        char *required_text = ability_ref_display(ability_ref);

        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            step,
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>",
            ability_ref,
            "intent step ability consumer lookup");

        if (resolve_required_ability_decl(
                ability_ref, step, ctx, "Intent step",
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>") == NULL) {
            free(required_text);
            continue;
        }

        for (size_t k = 0; k < step->data.intent_step.who_count; k++) {
            const char *alias = step->data.intent_step.who_names[k];
            ASTNode *involves = find_intent_involves_local(intent_decl, alias);
            const char *participant_type_name =
                intent_involves_type_name(involves);

            if (!intent_involves_is_subject_host(ctx->program_root, involves))
                continue;
            if (participant_type_name == NULL
                || subject_type_has_ability(ctx->program_root,
                    participant_type_name, ability_ref)) {
                continue;
            }

            ASTNode *actual_impl = subject_type_find_base_ability_impl(
                ctx->program_root, participant_type_name, ability);
            char *actual_text = actual_impl != NULL
                ? ability_ref_display(actual_impl) : NULL;
            char contract_summary[512];
            const char *required_label =
                required_text != NULL ? required_text
                                      : (ability != NULL ? ability : "<ability>");

            intent_step_format_contract_source_summary(
                intent_decl, step, ctx, contract_summary,
                sizeof(contract_summary));

            if (actual_impl != NULL) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_INTENT_STEP_INVALID,
                    PGY_CAUSE_INTENT_STEP,
                    PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS,
                    step,
                    "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' implements '%s' instead.\n"
                    "Reason:\n"
                    "- participant type '%s' does not satisfy required ability '%s'\n"
                    "- actual implementation is '%s'\n"
                    "%s%s"
                    "Fix:\n"
                    "- implement '%s' for subject type '%s'\n"
                    "- or choose a participant whose subject type satisfies the contract\n"
                    "- or override/remove the inherited step requirement",
                    step->data.intent_step.name != NULL
                        ? step->data.intent_step.name : "<step>",
                    required_label,
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    actual_text,
                    participant_type_name,
                    required_label,
                    actual_text,
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    required_label,
                    participant_type_name);
            } else {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_INTENT_STEP_INVALID,
                    PGY_CAUSE_INTENT_STEP,
                    PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS,
                    step,
                    "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' does not implement it.\n"
                    "Reason:\n"
                    "- participant type '%s' does not satisfy required ability '%s'\n"
                    "- no matching implementation was found for '%s'\n"
                    "%s%s"
                    "Fix:\n"
                    "- implement '%s' for subject type '%s'\n"
                    "- or choose a participant whose subject type satisfies the contract\n"
                    "- or override/remove the inherited step requirement",
                    step->data.intent_step.name != NULL
                        ? step->data.intent_step.name : "<step>",
                    required_label,
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    participant_type_name,
                    required_label,
                    required_label,
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    required_label,
                    participant_type_name);
            }
            free(actual_text);
        }
        free(required_text);
    }
}
