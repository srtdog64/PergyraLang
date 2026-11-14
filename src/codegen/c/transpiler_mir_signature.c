/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR signature eligibility policy.
 */

#include "transpiler_mir_signature.h"

#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_type_require.h"
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
transpiler_mir_routine_param_is_boundary_resource(
    const MIRRoutine *routine,
    size_t param_index)
{
    MIRParamCarriage carriage =
        transpiler_mir_routine_param_carriage(routine, param_index);
    return transpiler_mir_routine_param_resource_kind(routine, param_index)
            != MIR_PARAM_RESOURCE_NONE
        && (carriage == MIR_PARAM_CARRIAGE_OWNER_HANDLE
            || carriage == MIR_PARAM_CARRIAGE_READONLY_REF);
}

bool
transpiler_mir_routine_param_is_slot_family(const MIRRoutine *routine,
                                             size_t param_index)
{
    MIRParamResourceKind kind =
        transpiler_mir_routine_param_resource_kind(routine, param_index);
    return kind == MIR_PARAM_RESOURCE_SLOT
        || kind == MIR_PARAM_RESOURCE_SECURE_SLOT;
}

bool
transpiler_mir_routine_param_is_secure_slot(const MIRRoutine *routine,
                                             size_t param_index)
{
    return transpiler_mir_routine_param_resource_kind(routine, param_index)
        == MIR_PARAM_RESOURCE_SECURE_SLOT;
}

bool
transpiler_mir_routine_param_is_device_slot(const MIRRoutine *routine,
                                             size_t param_index)
{
    return transpiler_mir_routine_param_resource_kind(routine, param_index)
        == MIR_PARAM_RESOURCE_DEVICE_SLOT;
}

bool
transpiler_mir_type_name_supported(TranspilerCtx *ctx, const char *type_name)
{
    char c_type[256];

    if (type_name == NULL || type_name[0] == '\0')
        return false;
    if (strcmp(type_name, "Unknown") == 0)
        return false;
    if (transpiler_mir_type_supported(type_name))
        return true;

    if (!transpiler_copy_c_type_or_user_type_name(type_name,
            c_type,
            sizeof(c_type))
        || c_type[0] == '\0'
        || strcmp(c_type, "Unknown") == 0) {
        (void)ctx;
        return false;
    }
    return true;
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
                ctx, ast_event_handler_return_type(type_node))) {
            return false;
        }
        for (size_t i = 0; i < ast_event_handler_param_count(type_node); i++) {
            if (!transpiler_mir_ast_type_supported(
                    ctx, ast_event_handler_param_type(type_node, i))) {
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

    if (!pergyra_ast_type_to_c_copy_in_ctx(ctx, (ASTNode *)type_node,
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
transpiler_mir_routine_signature_metadata_complete_for(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const ASTNode *func_decl,
    unsigned requirements,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt)
{
    const char *func_name = func_decl != NULL
        ? ast_declaration_name((ASTNode *)func_decl)
        : NULL;

    if (func_name == NULL)
        func_name = mir_routine_name(routine);
    if (func_name == NULL)
        func_name = "(anonymous)";

    if (routine == NULL)
        return true;

    if (!mir_routine_has_signature(routine)) {
        transpiler_set_mir_inventory_missing(ctx,
            missing_signature_fmt != NULL
                ? missing_signature_fmt
                : "MIR-only C path missing function signature metadata for '%s'",
            func_name);
        return false;
    }

    if ((requirements & TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME) != 0
        && mir_routine_return_type_name(routine) == NULL) {
        ASTNode *return_type = mir_routine_return_type(routine);
        if (return_type != NULL
            && return_type->type != AST_EVENT_HANDLER_TYPE) {
            transpiler_set_mir_inventory_missing(ctx,
                missing_return_type_fmt != NULL
                    ? missing_return_type_fmt
                    : "MIR-only C path missing function return type-name metadata for '%s'",
                func_name);
            return false;
        }
    }

    if ((requirements & TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES) != 0) {
        for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
            FuncParam *param = mir_routine_param(routine, i);
            if (param == NULL || mir_routine_param_type_name(routine, i) != NULL)
                continue;
            if (param->type != NULL
                && param->type->type != AST_EVENT_HANDLER_TYPE) {
                transpiler_set_mir_inventory_missing(ctx,
                    missing_param_type_fmt != NULL
                        ? missing_param_type_fmt
                        : "MIR-only C path missing function parameter type-name metadata for '%s'",
                    func_name);
                return false;
            }
        }
    }

    return true;
}

/*
 * Row 607: validate a lossless callable (EventHandler) signature descriptor by
 * its rendered MIR type names, so the supportedness check consumes the MIR
 * carrier instead of re-reading the EventHandler AST node.
 */
static bool
transpiler_mir_callable_sig_supported(TranspilerCtx *ctx,
                                      const MIRCallableSig *sig)
{
    if (sig == NULL)
        return true;
    if (!sig->is_callable)
        return false;
    if (sig->return_type_name != NULL
        && !transpiler_mir_type_name_supported(ctx, sig->return_type_name))
        return false;
    if (sig->param_count > 0 && sig->param_type_names == NULL)
        return false;
    for (size_t i = 0; i < sig->param_count; i++) {
        if (sig->param_type_names[i] == NULL
            || !transpiler_mir_type_name_supported(ctx,
                   sig->param_type_names[i]))
            return false;
    }
    return true;
}

bool
transpiler_mir_routine_signature_supported_strict(
    TranspilerCtx *ctx,
    const MIRRoutine *routine)
{
    const char *name;

    if (routine == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "%s",
            "MIR-only C path missing routine signature metadata");
        return false;
    }
    name = mir_routine_name(routine);
    if (!mir_routine_has_signature(routine)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing function signature metadata for '%s'",
            name != NULL ? name : "(anonymous)");
        return false;
    }

    /* A null return type is the MIR representation of Void.  Any non-null
     * source return shape must have a concrete MIR name or callable row. */
    if (mir_routine_return_type_name(routine) != NULL) {
        if (!transpiler_mir_type_name_supported(
                ctx, mir_routine_return_type_name(routine))) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has unsupported function return type-name metadata for '%s'",
                name != NULL ? name : "(anonymous)");
            return false;
        }
    } else if (mir_routine_return_callable_sig(routine) != NULL) {
        if (!transpiler_mir_callable_sig_supported(
                ctx, mir_routine_return_callable_sig(routine))) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has incomplete function return callable metadata for '%s'",
                name != NULL ? name : "(anonymous)");
            return false;
        }
    } else if (mir_routine_return_type(routine) != NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing function return type-name metadata for '%s'",
            name != NULL ? name : "(anonymous)");
        return false;
    }

    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        const char *type_name = mir_routine_param_type_name(routine, i);
        const MIRCallableSig *callable =
            mir_routine_param_callable_sig(routine, i);

        /* Method self may be represented by owner metadata rather than a
         * duplicate source type slot.  It is not a recoverable user type. */
        if (param == NULL || param->name == NULL
            || (type_name == NULL && callable == NULL
                && strcmp(param->name, "self") != 0)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing function parameter type-name metadata for '%s' at argument %llu",
                name != NULL ? name : "(anonymous)",
                (unsigned long long)i);
            return false;
        }
        if (type_name != NULL) {
            if (!transpiler_mir_type_name_supported(ctx, type_name)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has unsupported function parameter type-name metadata for '%s' at argument %llu",
                    name != NULL ? name : "(anonymous)",
                    (unsigned long long)i);
                return false;
            }
        } else if (callable != NULL
                   && !transpiler_mir_callable_sig_supported(ctx, callable)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has incomplete function parameter callable metadata for '%s' at argument %llu",
                name != NULL ? name : "(anonymous)",
                (unsigned long long)i);
            return false;
        }
    }
    return true;
}

bool
transpiler_mir_routine_signature_supported(TranspilerCtx *ctx,
                                           const MIRRoutine *routine,
                                           const ASTNode *func_decl)
{
    const bool has_signature = mir_routine_has_signature(routine);

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
            routine,
            func_decl,
            TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing function signature eligibility metadata for '%s'",
            "MIR-only C path missing function signature return type-name metadata for '%s'",
            "MIR-only C path missing function signature parameter type-name metadata for '%s'")) {
        return false;
    }

    const char *return_type_name = has_signature
        ? mir_routine_return_type_name(routine)
        : NULL;
    if (return_type_name != NULL) {
        if (!transpiler_mir_type_name_supported(ctx, return_type_name))
            return false;
    } else if (has_signature
               && mir_routine_return_callable_sig(routine) != NULL) {
        /* Row 607: callable return shape is MIR-owned. */
        if (!transpiler_mir_callable_sig_supported(ctx,
                mir_routine_return_callable_sig(routine)))
            return false;
    } else if (!transpiler_mir_ast_type_supported(
                   ctx, ast_func_return_type(func_decl))) {
        return false;
    }

    size_t param_count = has_signature
        ? mir_routine_param_count(routine)
        : ast_func_param_count(func_decl);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = has_signature
            ? mir_routine_param(routine, i)
            : ast_func_param(func_decl, i);
        const char *param_type_name = has_signature
            ? mir_routine_param_type_name(routine, i)
            : NULL;
        if (param == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!transpiler_mir_type_name_supported(ctx, param_type_name))
                return false;
            continue;
        }
        if (has_signature
            && mir_routine_param_callable_sig(routine, i) != NULL) {
            /* Row 607: callable param shape is MIR-owned. */
            if (!transpiler_mir_callable_sig_supported(ctx,
                    mir_routine_param_callable_sig(routine, i)))
                return false;
            continue;
        }
        if (param->type == NULL)
            continue;
        if (!transpiler_mir_ast_type_supported(ctx, param->type))
            return false;
    }

    return true;
}

bool
transpiler_mir_or_ast_function_is_generic(const MIRRoutine *routine,
                                          const ASTNode *func_decl)
{
    if (mir_routine_has_signature(routine))
        return transpiler_mir_routine_generic_param_count(routine) > 0;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;
    return ast_generic_param_count(
               ast_declaration_generic_params((ASTNode *)func_decl)) > 0;
}

bool
transpiler_mir_function_signature_supported(TranspilerCtx *ctx,
                                            const ASTNode *func_decl)
{
    return transpiler_mir_routine_signature_supported(ctx, NULL, func_decl);
}
