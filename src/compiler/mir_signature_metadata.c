#include "mir_signature_metadata.h"

#include <stdint.h>
#include <stdlib.h>

#include "mir_type_helpers.h"
#include "../parser/ast_api.h"

void
mir_routine_signature_type_names_clear(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    if (routine->param_type_names != NULL) {
        for (size_t i = 0; i < routine->param_count; i++)
            free(routine->param_type_names[i]);
    }
    free(routine->param_type_names);
    routine->param_type_names = NULL;
    free(routine->return_type_name);
    routine->return_type_name = NULL;
}

bool
mir_routine_signature_type_names_capture(MIRRoutine *routine)
{
    if (routine == NULL || !routine->has_signature)
        return true;

    if (routine->param_count > 0) {
        if (routine->param_count > SIZE_MAX / sizeof(char *))
            return false;
        routine->param_type_names = calloc(routine->param_count,
            sizeof(char *));
        if (routine->param_type_names == NULL)
            return false;
        for (size_t i = 0; i < routine->param_count; i++) {
            FuncParam *param =
                routine->params != NULL ? routine->params[i] : NULL;
            if (param != NULL && param->type != NULL)
                routine->param_type_names[i] =
                    mir_capture_type_name(param->type, NULL);
        }
    }
    if (routine->return_type != NULL) {
        routine->return_type_name =
            mir_capture_type_name(routine->return_type, NULL);
    } else if (routine->ast != NULL && routine->ast->type == AST_FUNC_DECL
               && ast_func_semantic_return_type_name(routine->ast) != NULL) {
        routine->return_type_name =
            mir_capture_type_name(NULL,
                ast_func_semantic_return_type_name(routine->ast));
    }
    return true;
}
