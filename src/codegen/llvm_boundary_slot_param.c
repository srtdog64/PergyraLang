/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_boundary_slot_param.h"

#include <string.h>

#include "codegen_slot_type_policy.h"
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
        || ast_type_name(param->type) == NULL)
        return NULL;
    if (param->mode != PARAM_MODE_OWN && param->mode != PARAM_MODE_REF)
        return NULL;

    type_name = ast_type_name(param->type);
    if (!pgy_codegen_type_name_is_slot(type_name)
        && !pgy_codegen_type_name_is_secure_slot(type_name))
        return NULL;

    generic_args = ast_type_generic_args(param->type);
    GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
    if (inner_param == NULL)
        return NULL;

    inner_name = llvm_keep_rendered_persistent(ctx,
        llvm_stmt_render_type_arg(inner_param),
        "out of memory copying LLVM boundary slot type");
    if (inner_name == NULL)
        return NULL;

    (void)llvm_lookup_class(ctx, inner_name);

    if (is_secure_out != NULL)
        *is_secure_out = pgy_codegen_type_name_is_secure_slot(type_name);
    return inner_name;
}

const char *
llvm_boundary_slot_inner_name_from_type_name(LLVMGenCtx *ctx,
                                            FuncParam *param,
                                            const char *type_name,
                                            bool *is_secure_out)
{
    char inner_name[256];
    const char *inner;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (ctx == NULL || param == NULL || type_name == NULL
        || type_name[0] == '\0') {
        return NULL;
    }
    if (param->mode != PARAM_MODE_OWN && param->mode != PARAM_MODE_REF)
        return NULL;
    if (!pgy_codegen_type_name_is_slot(type_name)
        && !pgy_codegen_type_name_is_secure_slot(type_name)) {
        return NULL;
    }

    if (!llvm_constructed_arg_name_copy(type_name, 0,
            inner_name, sizeof(inner_name))
        || inner_name[0] == '\0'
        || strcmp(inner_name, "Unknown") == 0) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM boundary slot parameter '%s' requires concrete inner type metadata",
            param->name != NULL ? param->name : "<param>");
        return NULL;
    }

    inner = pgy_arena_strdup(&ctx->persistent, inner_name);
    if (inner == NULL) {
        llvm_set_error(ctx, "%s",
            "out of memory copying LLVM boundary slot type");
        return NULL;
    }

    (void)llvm_lookup_class(ctx, inner);

    if (is_secure_out != NULL)
        *is_secure_out = pgy_codegen_type_name_is_secure_slot(type_name);
    return inner;
}

#endif
