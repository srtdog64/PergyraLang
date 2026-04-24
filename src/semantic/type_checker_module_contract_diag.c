/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Diagnostics shared by action/zone/intent module contract consumers.
 */

#include "diag_codes.h"
#include "type_checker_module_contract_diag_internal.h"

void
report_subject_ability_requirement_mismatch(SemanticContext *ctx,
                                            const ASTNode *site,
                                            const char *owner_label,
                                            const char *owner_name,
                                            const char *subject_label,
                                            const char *subject_name,
                                            const char *required_text,
                                            const char *actual_text,
                                            const char *fix_tail)
{
    if (ctx == NULL || site == NULL)
        return;

    if (actual_text != NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
            PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
            site,
            "%s '%s' requires ability '%s', but %s '%s' implements '%s' instead.\n"
            "Reason:\n"
            "- consumer path is %s '%s'\n"
            "- %s '%s' does not satisfy required ability '%s'\n"
            "- expected type args are '%s'\n"
            "- actual implementation is '%s'\n"
            "- actual type args are '%s'\n"
            "Fix:\n"
            "- implement '%s' for %s '%s'\n"
            "- %s",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            required_text != NULL ? required_text : "<ability>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            actual_text,
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            required_text != NULL ? required_text : "<ability>",
            required_text != NULL ? required_text : "<ability>",
            actual_text,
            actual_text,
            required_text != NULL ? required_text : "<ability>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            fix_tail != NULL ? fix_tail : "change/remove the ability requirement");
        return;
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID,
        PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS,
        site,
        "%s '%s' requires ability '%s', but %s '%s' has no matching role impl.\n"
        "Reason:\n"
        "- consumer path is %s '%s'\n"
        "- %s '%s' does not satisfy required ability '%s'\n"
        "- expected type args are '%s'\n"
        "- no matching implementation was found for '%s'\n"
        "Fix:\n"
        "- implement '%s' for %s '%s'\n"
        "- %s",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        required_text != NULL ? required_text : "<ability>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        fix_tail != NULL ? fix_tail : "change/remove the ability requirement");
}
