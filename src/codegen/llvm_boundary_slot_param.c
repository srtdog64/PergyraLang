/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_boundary_slot_param.h"

#include <string.h>

#include "llvm_internal_api.h"

const char *
llvm_boundary_slot_inner_name(LLVMGenCtx *ctx, FuncParam *param,
                              bool *is_secure_out)
{
    const char *type_name;
    const char *inner_name;
    GenericParams *generic_args;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (ctx == NULL || param == NULL || param->type == NULL
        || param->type->type != AST_TYPE
        || param->type->data.type.name == NULL)
        return NULL;
    if (param->mode != PARAM_MODE_OWN && param->mode != PARAM_MODE_REF)
        return NULL;

    type_name = param->type->data.type.name;
    if (strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0)
        return NULL;

    generic_args = param->type->data.type.generic_args;
    if (generic_args == NULL || generic_args->count == 0
        || generic_args->params == NULL || generic_args->params[0] == NULL)
        return NULL;

    inner_name = generic_args->params[0]->name;
    if (inner_name == NULL && generic_args->params[0]->constraint != NULL
        && generic_args->params[0]->constraint->type == AST_TYPE) {
        inner_name = generic_args->params[0]->constraint->data.type.name;
    }
    if (inner_name == NULL)
        return NULL;

    (void)llvm_lookup_class(ctx, inner_name);

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}

#endif
