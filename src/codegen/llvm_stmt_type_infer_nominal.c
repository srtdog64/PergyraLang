#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_stmt_type_infer_helpers.h"
#include "parser/ast_api.h"

#include <string.h>

const char *
llvm_stmt_infer_nominal_name_from_init(LLVMGenCtx *ctx, ASTNode *init)
{
    const char *name;

    if (ctx == NULL || init == NULL)
        return NULL;

    if (init->type == AST_IDENTIFIER && init->data.identifier.name != NULL) {
        name = init->data.identifier.name;
        {
            LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
            if (var != NULL) {
                LLVMClassTypeEntry *var_cls =
                    llvm_stmt_lookup_class_by_type(ctx, var->type);
                if (var_cls != NULL)
                    return var_cls->class_name;
            }
        }
        {
            const char *tracked = llvm_lookup_var_class(ctx, name);
            if (tracked != NULL)
                return tracked;
        }
        if (strcmp(name, "self") != 0) {
            ASTNode *host_decl = llvm_current_host_decl(ctx);
            const char *host_name = llvm_decl_node_name(host_decl);
            LLVMClassTypeEntry *host_cls =
                host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                        ctx, host_cls->fields[field_idx].field_type);
                    if (field_cls != NULL)
                        return field_cls->class_name;
                }
            }
        }
        return NULL;
    }

    if (init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ast_call_callee(init)->data.identifier.name != NULL) {
        name = ast_call_callee(init)->data.identifier.name;
        if (llvm_stmt_call_returns_collection_value(name)
            && ast_call_arg_count(init) >= 1
            && ast_call_argument(init, 0) != NULL
            && ast_call_argument(init, 0)->type == AST_IDENTIFIER) {
            const char *collection =
                ast_call_argument(init, 0)->data.identifier.name;
            const char *inner = llvm_stmt_lookup_collection_get_inner(
                ctx, name, collection);
            if (inner != NULL && llvm_lookup_class(ctx, inner) != NULL)
                return inner;
        }
        if (llvm_lookup_class(ctx, name) != NULL)
            return name;
        {
            LLVMFuncEntry *callee_fn =
                llvm_stmt_lookup_visible_function(ctx, name);
            LLVMClassTypeEntry *ret_cls = callee_fn != NULL
                ? llvm_stmt_lookup_class_by_type(ctx, callee_fn->ret_type)
                : NULL;
            if (ret_cls != NULL)
                return ret_cls->class_name;
        }
    }

    if (init->type == AST_MEMBER_ACCESS
        && ast_member_object(init) != NULL
        && ast_member_name(init) != NULL) {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, ast_member_object(init));
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, ast_member_name(init));
            if (field_idx >= 0) {
                LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                    ctx, base_cls->fields[field_idx].field_type);
                if (field_cls != NULL)
                    return field_cls->class_name;
            }
        }
    }

    return NULL;
}

#endif
