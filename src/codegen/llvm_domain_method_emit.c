/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_method_emit.h"

#include "llvm_inventory_host_methods.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_projection_sync_helpers.h"

bool
llvm_emit_domain_sync_and_method_bodies(LLVMGenCtx *ctx,
    ASTNode ***domain_groups,
    const size_t *domain_group_counts,
    size_t domain_group_count)
{
    if (ctx == NULL || domain_groups == NULL || domain_group_counts == NULL)
        return true;

    for (size_t group = 0; group < domain_group_count; group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
            if (stmt == NULL)
                continue;

            const char *decl_name = NULL;
            decl_name = llvm_decl_node_name(stmt);
            if (decl_name == NULL)
                continue;

            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);
            if (cls != NULL && cls->domain_kind != LLVM_DOMAIN_NONE
                && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC
                && cls->sync_function_name != NULL) {
                LLVMFuncEntry *sync_entry;
                sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
                if (sync_entry != NULL) {
                    if (cls->domain_kind == LLVM_DOMAIN_ZONE)
                        llvm_emit_zone_sync(stmt, decl_name, cls, sync_entry->fn,
                            ctx);
                    else if (cls->domain_kind == LLVM_DOMAIN_WORLD)
                        llvm_emit_world_sync(stmt, decl_name, cls, sync_entry->fn,
                            ctx);
                    else
                        llvm_emit_domain_projection_sync(stmt, decl_name, cls,
                            sync_entry->fn, ctx);
                    if (ctx->has_error)
                        return false;
                }
            }

            LLVMHostedMethodView method_view =
                llvm_hosted_method_view_from_decl(ctx, decl_name, stmt);
            if (llvm_hosted_method_view_missing_mir_metadata(&method_view)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing method declaration metadata for domain '%s'",
                    decl_name != NULL ? decl_name : "(anonymous-domain)");
                return false;
            }
            for (size_t j = 0; j < method_view.count; j++) {
                const MIRDeclMethod *method_meta =
                    llvm_hosted_method_view_metadata(&method_view, j);
                ASTNode *method =
                    llvm_hosted_method_view_source_ast(&method_view, j);
                const char *method_name = llvm_mir_decl_method_name(method_meta);
                const MIRRoutine *mir_method = NULL;
                if (llvm_hosted_method_view_missing_mir_method_row(&method_view, j)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path has invalid method declaration metadata row for domain '%s'",
                        decl_name != NULL ? decl_name : "(anonymous-domain)");
                    return false;
                }
                if (method_name == NULL && method != NULL
                    && method->type == AST_FUNC_DECL)
                    method_name = ast_declaration_name(method);
                if (method_meta == NULL
                    && (method == NULL || method->type != AST_FUNC_DECL)) {
                    continue;
                }

                mir_method = llvm_mir_decl_method_routine(ctx, method_meta);
                if (method == NULL && mir_method != NULL)
                    method = llvm_mir_routine_source_ast_of_type(
                        mir_method, MIR_SCOPE_METHOD, AST_FUNC_DECL);
                if (method_name == NULL && method != NULL)
                    method_name = ast_declaration_name(method);
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    if (ctx->has_error)
                        return false;
                    continue;
                }
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing routine for domain method '%s.%s'",
                    decl_name != NULL ? decl_name : "(anonymous-domain)",
                    method_name != NULL ? method_name : "(anonymous)");
                return false;
            }
        }
    }

    return true;
}

bool
llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx)
{
    ASTNode **nominal_nodes = NULL;
    size_t nominal_count = 0;

    if (ctx == NULL)
        return true;

    llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count);
    for (size_t i = 0; i < nominal_count; i++) {
        ASTNode *decl = nominal_nodes != NULL ? nominal_nodes[i] : NULL;
        const char *cls_name;

        if (decl == NULL || decl->type != AST_CLASS_DECL)
            continue;

        cls_name = llvm_decl_node_name(decl);
        LLVMHostedMethodView method_view =
            llvm_hosted_method_view_from_decl(ctx, cls_name, decl);
        if (llvm_hosted_method_view_missing_mir_metadata(&method_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing declaration metadata for class method '%s.%s'",
                cls_name != NULL ? cls_name : "(anonymous-class)",
                "(metadata)");
            return false;
        }
        if (!method_view.uses_mir_metadata)
            continue;
        for (size_t j = 0; j < method_view.count; j++) {
            const MIRDeclMethod *method_meta =
                llvm_hosted_method_view_metadata(&method_view, j);
            const char *method_name;
            const MIRRoutine *mir_method;

            if (llvm_hosted_method_view_missing_mir_method_row(&method_view, j)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has invalid method declaration metadata row for class '%s'",
                    cls_name != NULL ? cls_name : "(anonymous-class)");
                return false;
            }
            method_name = llvm_mir_decl_method_name(method_meta);
            mir_method = llvm_mir_decl_method_routine(ctx, method_meta);
            if (mir_method != NULL) {
                llvm_emit_func_from_mir(mir_method, ctx);
                if (ctx->has_error)
                    return false;
                continue;
            }
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing routine for class method '%s.%s'",
                cls_name != NULL ? cls_name : "(anonymous-class)",
                method_name != NULL ? method_name : "(anonymous)");
            return false;
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */
