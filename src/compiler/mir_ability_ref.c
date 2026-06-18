#include "mir_ability_ref.h"

#include "mir_type_helpers.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>

void
mir_ability_ref_clear(MIRAbilityRef *ref)
{
    if (ref == NULL)
        return;
    free(ref->base_name);
    if (ref->actual_arg_type_names != NULL) {
        for (size_t i = 0; i < ref->actual_arg_count; i++)
            free(ref->actual_arg_type_names[i]);
        free(ref->actual_arg_type_names);
    }
    ref->base_name = NULL;
    ref->actual_arg_count = 0;
    ref->actual_arg_type_names = NULL;
}

static char *
mir_ability_ref_capture_generic_actual_type_name(GenericParam *param)
{
    ASTNode *constraint;
    const char *name;

    if (param == NULL)
        return NULL;
    constraint = ast_generic_param_constraint(param);
    if (constraint != NULL)
        return mir_capture_type_name(constraint, NULL);
    name = ast_generic_param_name(param);
    return mir_capture_type_name(NULL, name);
}

bool
mir_ability_ref_capture(MIRAbilityRef *ref, ASTNode *ability)
{
    GenericParams *actuals;
    size_t actual_count;

    if (ref == NULL)
        return false;
    ref->base_name = NULL;
    ref->actual_arg_count = 0;
    ref->actual_arg_type_names = NULL;

    if (ability == NULL || ability->type != AST_TYPE
        || ast_type_name(ability) == NULL) {
        return false;
    }

    ref->base_name = mir_capture_type_name(NULL, ast_type_name(ability));
    if (ref->base_name == NULL)
        return false;

    actuals = ast_type_generic_args(ability);
    actual_count = ast_generic_param_count(actuals);
    if (actual_count == 0)
        return true;
    if (actual_count > SIZE_MAX / sizeof(char *))
        return false;

    ref->actual_arg_type_names = calloc(actual_count, sizeof(char *));
    if (ref->actual_arg_type_names == NULL)
        return false;
    ref->actual_arg_count = actual_count;
    for (size_t i = 0; i < actual_count; i++) {
        ref->actual_arg_type_names[i] =
            mir_ability_ref_capture_generic_actual_type_name(
                ast_generic_param_at(actuals, i));
        if (ref->actual_arg_type_names[i] == NULL)
            return false;
    }
    return true;
}
