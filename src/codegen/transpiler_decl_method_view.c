/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Hosted-method MIR metadata view helpers for the C backend.
 */

#include "transpiler_decl_lookup.h"

#include <string.h>

#include "host_decl_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "transpiler_context.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_inventory_view.h"

TranspilerHostedMethodView
transpiler_hosted_method_view(const TranspilerCtx *ctx,
                              const char *host_name,
                              ASTNode **ast_compat_methods,
                              size_t ast_compat_count)
{
    TranspilerHostedMethodView view;
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_methods = ast_compat_methods;
    view.ast_compat_count = ast_compat_count;
    view.count = ast_compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && ast_compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && transpiler_is_host_decl_type(mir_decl_header_ast_type_or(
            header, AST_PROGRAM))) {
        view.decl_header = header;
        view.count = mir_decl_header_method_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_method_view_missing_mir_metadata(
    const TranspilerHostedMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclMethod *
transpiler_hosted_method_view_metadata(const TranspilerHostedMethodView *view,
                                       size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_method(view->decl_header, index);
}

ASTNode *
transpiler_hosted_method_view_compat_method(
    const TranspilerHostedMethodView *view,
    size_t index)
{
    if (view == NULL || view->uses_mir_metadata
        || view->ast_compat_methods == NULL
        || index >= view->ast_compat_count) {
        return NULL;
    }
    return view->ast_compat_methods[index];
}

const MIRDeclMethod *
transpiler_find_host_method_metadata_in_context(
    const TranspilerCtx *ctx,
    const char *host_type_name,
    const char *method_name)
{
    const MIRDeclHeader *header;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    header = transpiler_active_host_decl_header(ctx, host_type_name);
    if (header == NULL) {
        ASTNode *base_decl = transpiler_generic_class_spec_base_decl(
            ctx, host_type_name);
        const char *base_name = transpiler_decl_name_local(base_decl);
        if (base_name != NULL)
            header = transpiler_active_host_decl_header(ctx, base_name);
    }
    if (header == NULL) {
        const char *generic_start = strchr(host_type_name, '<');
        if (generic_start != NULL) {
            char base_name[128];
            size_t base_len = (size_t)(generic_start - host_type_name);
            if (base_len > 0 && base_len < sizeof(base_name)) {
                memcpy(base_name, host_type_name, base_len);
                base_name[base_len] = '\0';
                header = transpiler_active_host_decl_header(ctx, base_name);
            }
        }
    }
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_method_count(header); i++) {
        const MIRDeclMethod *method = mir_decl_header_method(header, i);
        const char *name = transpiler_mir_decl_method_name(method);
        if (name != NULL && strcmp(name, method_name) == 0)
            return method;
    }
    return NULL;
}

bool
transpiler_hosted_method_view_missing_mir_method_row(
    const TranspilerHostedMethodView *view,
    size_t index)
{
    return view != NULL
        && view->uses_mir_metadata
        && transpiler_hosted_method_view_metadata(view, index) == NULL;
}

bool
transpiler_require_hosted_method_view_rows(
    TranspilerCtx *ctx,
    const TranspilerHostedMethodView *view,
    const char *message_fmt,
    const char *host_name)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        if (transpiler_hosted_method_view_missing_mir_method_row(view, i)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                message_fmt != NULL
                    ? message_fmt
                    : "MIR-only C path has invalid method declaration metadata row for '%s'",
                host_name != NULL ? host_name : "(anonymous)");
            return false;
        }
    }
    return true;
}

bool
transpiler_mir_decl_method_metadata_complete_for(
    TranspilerCtx *ctx,
    const MIRDeclMethod *method,
    const char *host_name,
    const char *method_name,
    unsigned requirements,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt)
{
    const char *host_display =
        host_name != NULL ? host_name : "(anonymous-host)";
    const char *resolved_method_name = method_name != NULL
        ? method_name
        : transpiler_mir_decl_method_name(method);
    const char *method_display = resolved_method_name != NULL
        ? resolved_method_name
        : "(anonymous)";

    if (method == NULL || !transpiler_active_has_mir(ctx))
        return true;

    if ((requirements
            & TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME) != 0
        && transpiler_mir_decl_method_return_type_name(method) == NULL) {
        ASTNode *return_type = transpiler_mir_decl_method_return_type(method);
        if (return_type != NULL
            && return_type->type != AST_EVENT_HANDLER_TYPE) {
            transpiler_set_mir_inventory_missing(ctx,
                missing_return_type_fmt != NULL
                    ? missing_return_type_fmt
                    : "MIR-only C path missing hosted method return type-name metadata for '%s.%s'",
                host_display,
                method_display);
            return false;
        }
    }

    if ((requirements
            & TRANSPILER_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES) != 0) {
        for (size_t i = 0; i < transpiler_mir_decl_method_param_count(method);
             i++) {
            FuncParam *param = transpiler_mir_decl_method_param(method, i);
            if (param == NULL || param->name == NULL
                || strcmp(param->name, "self") == 0
                || transpiler_mir_decl_method_param_type_name(method, i)
                    != NULL) {
                continue;
            }
            if (param->type != NULL
                && param->type->type != AST_EVENT_HANDLER_TYPE) {
                transpiler_set_mir_inventory_missing(ctx,
                    missing_param_type_fmt != NULL
                        ? missing_param_type_fmt
                        : "MIR-only C path missing hosted method parameter type-name metadata for '%s.%s'",
                    host_display,
                    method_display);
                return false;
            }
        }
    }

    return true;
}

const char *
transpiler_mir_decl_method_name(const MIRDeclMethod *method)
{
    return mir_decl_method_name(method);
}

size_t
transpiler_mir_decl_method_param_count(const MIRDeclMethod *method)
{
    return mir_decl_method_param_count(method);
}

FuncParam *
transpiler_mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    return mir_decl_method_param(method, index);
}

const char *
transpiler_mir_decl_method_param_type_name(const MIRDeclMethod *method,
                                           size_t index)
{
    return mir_decl_method_param_type_name(method, index);
}

ASTNode *
transpiler_mir_decl_method_return_type(const MIRDeclMethod *method)
{
    return mir_decl_method_return_type(method);
}

const char *
transpiler_mir_decl_method_return_type_name(const MIRDeclMethod *method)
{
    return mir_decl_method_return_type_name(method);
}

bool
transpiler_mir_decl_method_is_async(const MIRDeclMethod *method)
{
    return mir_decl_method_is_async(method);
}

bool
transpiler_mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return mir_decl_method_is_action_like(method);
}

const char *
transpiler_mir_decl_method_within_zone(const MIRDeclMethod *method)
{
    return mir_decl_method_within_zone(method);
}

const char *
transpiler_mir_decl_method_causes_effect(const MIRDeclMethod *method)
{
    return mir_decl_method_causes_effect(method);
}

const MIRRoutine *
transpiler_mir_decl_method_routine(const TranspilerCtx *ctx,
                                   const MIRDeclMethod *method)
{
    TranspilerMIRRoutineInventory inventory;
    size_t routine_index = 0;

    if (ctx == NULL || !transpiler_active_has_mir(ctx) || method == NULL)
        return NULL;
    if (!mir_decl_method_routine_index(method, &routine_index))
        return NULL;
    transpiler_active_routine_inventory(ctx, &inventory);
    return transpiler_routine_inventory_get(&inventory, routine_index);
}

size_t
transpiler_mir_decl_method_projection_write_count(
    const MIRDeclMethod *method)
{
    return mir_decl_method_projection_write_count(method);
}

const char *
transpiler_mir_decl_method_projection_write_root_name(
    const MIRDeclMethod *method,
    size_t index)
{
    return mir_decl_method_projection_write_root_name(method, index);
}

const char *
transpiler_mir_decl_method_projection_write_member_name(
    const MIRDeclMethod *method,
    size_t index)
{
    return mir_decl_method_projection_write_member_name(method, index);
}

size_t
transpiler_mir_decl_method_projection_call_count(
    const MIRDeclMethod *method)
{
    return mir_decl_method_projection_call_count(method);
}

const char *
transpiler_mir_decl_method_projection_call_receiver_name(
    const MIRDeclMethod *method,
    size_t index)
{
    return mir_decl_method_projection_call_receiver_name(method, index);
}

const char *
transpiler_mir_decl_method_projection_call_method_name(
    const MIRDeclMethod *method,
    size_t index)
{
    return mir_decl_method_projection_call_method_name(method, index);
}

TranspilerHostedMethodView
transpiler_hosted_method_view_from_decl(const TranspilerCtx *ctx,
                                        const char *host_name,
                                        ASTNode *decl)
{
    PgyHostMethodCompatView compat = pgy_host_method_compat_view_from_decl(
        decl, transpiler_active_has_mir(ctx));

    return transpiler_hosted_method_view(ctx, host_name,
        compat.methods, compat.count);
}
