/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR signature eligibility policy.
 */

#include "transpiler_mir_signature.h"

#include <stdlib.h>
#include <string.h>

#include "transpiler_type_render.h"

bool
transpiler_mir_type_supported(const char *type_name)
{
    if (type_name == NULL)
        return false;
    if (strcmp(type_name, "Void") == 0)
        return true;
    return strcmp(type_name, "Int") == 0
           || strcmp(type_name, "Long") == 0
           || strcmp(type_name, "Float") == 0
           || strcmp(type_name, "Bool") == 0
           || strcmp(type_name, "String") == 0
           || strncmp(type_name, "Slot<", 5) == 0
           || strncmp(type_name, "SecureSlot<", 11) == 0
           || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

bool
transpiler_mir_ast_type_supported(TranspilerCtx *ctx, const ASTNode *type_node)
{
    const char *type_name = NULL;
    char c_type[256];

    if (type_node == NULL)
        return true;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        if (!transpiler_mir_ast_type_supported(
                ctx, type_node->data.event_handler_type.return_type)) {
            return false;
        }
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            if (!transpiler_mir_ast_type_supported(
                    ctx, type_node->data.event_handler_type.param_types[i])) {
                return false;
            }
        }
        return true;
    }

    type_name = transpiler_render_type_name_local(ctx, (ASTNode *)type_node);
    if (type_name == NULL)
        return false;
    if (transpiler_mir_type_supported(type_name))
        return true;

    if (!pergyra_ast_type_to_c_copy((ASTNode *)type_node,
            c_type,
            sizeof(c_type))
        || c_type[0] == '\0') {
        return false;
    }
    if (strcmp(type_name, "Unknown") == 0 || strcmp(c_type, "Unknown") == 0)
        return false;

    return true;
}

bool
transpiler_mir_function_signature_supported(TranspilerCtx *ctx,
                                            const ASTNode *func_decl)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (!transpiler_mir_ast_type_supported(ctx, func_decl->data.func_decl.return_type))
        return false;

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL)
            continue;
        if (!transpiler_mir_ast_type_supported(ctx, param->type))
            return false;
    }

    return true;
}
