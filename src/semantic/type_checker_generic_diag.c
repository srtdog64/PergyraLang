/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker generic contract diagnostics.
 */

#include "diag_codes.h"
#include "type_checker_generic_diag_internal.h"

void
semantic_report_ability_generic_bound_failure(SemanticContext *ctx,
                                              const ASTNode *site,
                                              const char *owner_label,
                                              const char *owner_name,
                                              const char *ability_name,
                                              const char *param_name,
                                              const char *bound_name,
                                              const char *bounds_text,
                                              const char *required_text,
                                              const char *concrete_name)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
        PGY_CAUSE_GENERIC_BOUND_VALIDATION_FAILED,
        PGY_FIX_ALIGN_GENERIC_BOUND_OR_ANNOTATE,
        site,
        "%s '%s' uses ability '%s' with generic argument '%s' that does not satisfy bound '%s'.\n"
        "Reason:\n"
        "- consumer path is %s '%s'\n"
        "- generic subject is ability '%s'\n"
        "- ability declaration '%s' requires '%s: %s'\n"
        "- full bound set is '%s: %s'\n"
        "- expected type args are '%s'\n"
        "- actual type args are '%s'\n"
        "- broken bound is '%s'\n"
        "Fix:\n"
        "- pass a type argument that satisfies '%s'\n"
        "- or relax the bound on ability '%s'",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        ability_name != NULL ? ability_name : "<ability>",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        ability_name != NULL ? ability_name : "<ability>",
        ability_name != NULL ? ability_name : "<ability>",
        param_name != NULL ? param_name : "<type-param>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        required_text != NULL ? required_text : "<ability>",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        bound_name != NULL ? bound_name : "<constraint>",
        ability_name != NULL ? ability_name : "<ability>");
}

void
semantic_report_function_generic_bound_failure(SemanticContext *ctx,
                                               ASTNode *site,
                                               const char *function_name,
                                               const char *param_name,
                                               const char *bound_name,
                                               const char *bounds_text,
                                               const char *expected_sig,
                                               const char *actual_sig,
                                               const char *concrete_name)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
        PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
        "Type '%s' does not satisfy constraint '%s' for generic parameter '%s'.\n"
        "Reason:\n"
        "- generic subject is function '%s'\n"
        "- function '%s' requires '%s: %s'\n"
        "- full bound set is '%s: %s'\n"
        "- expected type args are '%s'\n"
        "- actual type args are '%s'\n"
        "- consumer path is call site of '%s'\n"
        "- broken bound is '%s'\n"
        "Fix:\n"
        "- pass an argument whose type satisfies '%s'\n"
        "- or relax the function where-clause",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        function_name != NULL ? function_name : "<function>",
        function_name != NULL ? function_name : "<function>",
        param_name != NULL ? param_name : "<type-param>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        expected_sig != NULL ? expected_sig : "<function>",
        actual_sig != NULL ? actual_sig : "<actual>",
        function_name != NULL ? function_name : "<function>",
        bound_name != NULL ? bound_name : "<constraint>",
        bound_name != NULL ? bound_name : "<constraint>");
}

void
semantic_report_class_generic_bound_failure(SemanticContext *ctx,
                                            ASTNode *site,
                                            const char *class_name,
                                            const char *param_name,
                                            const char *bound_name,
                                            const char *bounds_text,
                                            const char *expected_text,
                                            const char *actual_text,
                                            const char *concrete_name,
                                            const char *site_label)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
        PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
        "Type '%s' does not satisfy constraint '%s' for generic parameter '%s' in class '%s'.\n"
        "Contract source:\n"
        "- class declaration contract is '%s'\n"
        "- where clause requires '%s: %s'\n"
        "- validation path is '%s'\n"
        "Reason:\n"
        "- class '%s' requires '%s: %s'\n"
        "- full bound set is '%s: %s'\n"
        "- expected type args are '%s'\n"
        "- actual type args are '%s'\n"
        "- %s type argument is '%s'\n"
        "Fix:\n"
        "- pass/specialize with a type that satisfies '%s'\n"
        "- or relax the class where-clause",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        class_name != NULL ? class_name : "<class>",
        expected_text != NULL ? expected_text : "<class>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        actual_text != NULL ? actual_text : "<actual>",
        class_name != NULL ? class_name : "<class>",
        param_name != NULL ? param_name : "<type-param>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        expected_text != NULL ? expected_text : "<class>",
        actual_text != NULL ? actual_text : "<actual>",
        site_label != NULL ? site_label : "effective",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>");
}
