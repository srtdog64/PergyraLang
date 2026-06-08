#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "codegen_slot_type_policy.h"
#include "llvm_mir_signature.h"
#include "parser/ast_api.h"

const char *
llvm_stmt_render_type_annotation_copy(LLVMGenCtx *ctx, ASTNode *type_ann)
{
    char buf[256];
    size_t offset;
    GenericParams *generic_args;

    if (ast_type_name(type_ann) == NULL)
        return NULL;
    generic_args = ast_type_generic_args(type_ann);
    size_t generic_count = ast_generic_param_count(generic_args);
    if (generic_count == 0) {
        if (ctx == NULL)
            return ast_type_name(type_ann);
        return pgy_arena_strdup(&ctx->scratch, ast_type_name(type_ann));
    }

    {
        int written = snprintf(buf, sizeof(buf), "%s<",
                               ast_type_name(type_ann));
        if (written < 0 || (size_t)written >= sizeof(buf))
            return NULL;
        offset = (size_t)written;
    }
    for (size_t i = 0; i < generic_count; i++) {
        char *arg = llvm_stmt_render_type_arg(
            ast_generic_param_at(generic_args, i));
        if (arg == NULL || arg[0] == '\0') {
            free(arg);
            return NULL;
        }
        int written = snprintf(buf + offset, sizeof(buf) - offset,
                               "%s%s", i == 0 ? "" : ", ",
                               arg);
        free(arg);
        if (written < 0)
            return NULL;
        if ((size_t)written >= sizeof(buf) - offset)
            return NULL;
        offset += (size_t)written;
    }
    if (offset + 1 < sizeof(buf)) {
        buf[offset++] = '>';
        buf[offset] = '\0';
    } else {
        return NULL;
    }
    if (ctx == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->scratch, buf);
}

static const char *
llvm_stmt_declared_return_type_name(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;
    ASTNode *return_type;
    bool generic_func;
    bool extern_func;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl = llvm_stmt_find_function_decl_by_name(ctx, name);
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    generic_func =
        ast_generic_param_count(ast_declaration_generic_params(decl)) > 0;
    extern_func = llvm_decl_is_extern_function(ctx, decl);
    if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
        const MIRRoutine *routine =
            llvm_active_function_routine_for_source_ast(ctx, decl);
        const char *return_type_name = NULL;
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing declared return inference routine for '%s'",
                name);
            return NULL;
        }
        if (!llvm_mir_routine_signature_metadata_complete_for(ctx,
                routine, decl,
                LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                "MIR-only LLVM path missing declared return inference signature metadata for '%s'",
                "MIR-only LLVM path missing declared return inference return type-name metadata for '%s'",
                NULL)) {
            return NULL;
        }
        return_type_name = llvm_mir_routine_return_type_name(routine);
        if (return_type_name != NULL)
            return return_type_name;
        return NULL;
    }

    return_type = ast_func_return_type(decl);
    if (ast_type_name(return_type) == NULL) {
        return NULL;
    }

    return ast_type_name(return_type);
}

LLVMValueRef
llvm_stmt_create_slot_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    return llvm_create_entry_alloca(ctx, type, name);
}

static const char *
llvm_simple_expr_type_name(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *callee;
    ASTNode *receiver;
    const char *method_name;

    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return "Long";
        return ast_number_is_float(expr) ? "Float" : "Int";
    case AST_STRING: return "String";
    case AST_BOOLEAN: return "Bool";
    case AST_IDENTIFIER: {
        LLVMVarEntry entry;
        if (llvm_scope_lookup_snapshot(ctx, ast_identifier_name(expr), &entry))
            return llvm_type_to_suffix(ctx, entry.type);
        return NULL;
    }
    case AST_CALL:
        callee = ast_call_callee(expr);
        if (callee != NULL
            && callee->type == AST_MEMBER_ACCESS
            && ast_member_name(callee) != NULL) {
            receiver = ast_member_object(callee);
            method_name = ast_member_name(callee);
            if (receiver != NULL && receiver->type == AST_IDENTIFIER) {
                const char *name = ast_identifier_name(receiver);
                const char *inner = llvm_lookup_slot_inner(ctx, name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, name);
                if (inner != NULL
                    && pgy_codegen_call_name_is_read(method_name))
                    return inner;
                if (inner != NULL
                    && (pgy_codegen_call_name_is_write(method_name)
                        || pgy_codegen_call_name_is_release(method_name))) {
                    return "Void";
                }
                const char *receiver_type = NULL;
                ASTNode *method_decl = NULL;
                if (strcmp(name, "self") == 0) {
                    ASTNode *host_decl = llvm_current_host_decl(ctx);
                    receiver_type = llvm_decl_node_name(host_decl);
                }
                if (receiver_type == NULL)
                    receiver_type = llvm_lookup_var_class(ctx, name);
                if (receiver_type != NULL) {
                    const MIRDeclMethod *method_meta =
                        llvm_find_host_method_metadata_in_context(
                            ctx, receiver_type, method_name);
                    if (!llvm_mir_decl_method_metadata_complete_for(ctx,
                            method_meta,
                            receiver_type,
                            method_name,
                            LLVM_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
                            "MIR-only LLVM path missing let method return type-name metadata for '%s.%s'",
                            NULL)) {
                        return NULL;
                    }
                    const char *method_return_type_name =
                        llvm_mir_decl_method_return_type_name(method_meta);
                    ASTNode *method_return_type =
                        llvm_mir_decl_method_return_type(method_meta);
                    if (method_return_type_name != NULL)
                        return method_return_type_name;
                    if (method_return_type == NULL && method_meta == NULL) {
                        if (llvm_active_has_mir(ctx)) {
                            llvm_set_mir_inventory_missing(ctx,
                                "MIR-only LLVM path missing let method return metadata for '%s.%s'",
                                receiver_type != NULL ? receiver_type : "(anonymous)",
                                method_name != NULL ? method_name : "(anonymous)");
                            return NULL;
                        }
                        method_decl = llvm_find_host_method_decl_in_context(
                            ctx, receiver_type, method_name);
                        method_return_type = ast_func_return_type(method_decl);
                    }
                    if (method_return_type != NULL
                        && ast_type_name(method_return_type) != NULL) {
                        return ast_type_name(method_return_type);
                    }
                }
            }
        }
        if (callee != NULL
            && callee->type == AST_IDENTIFIER
            && ast_identifier_name(callee) != NULL) {
            const char *callee_name = ast_identifier_name(callee);
            if (ast_call_arg_count(expr) >= 1
                && ast_call_argument(expr, 0) != NULL
                && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
                const char *name =
                    ast_identifier_name(ast_call_argument(expr, 0));
                const char *inner = NULL;
                if (pgy_codegen_call_name_is_slot_operation(callee_name)) {
                    inner = llvm_lookup_slot_inner(ctx, name);
                    if (inner == NULL) {
                        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                        if (view != NULL)
                            inner = view->inner_type;
                    }
                    if (inner == NULL)
                        inner = llvm_lookup_device_slot_inner(ctx, name);
                    if (inner != NULL
                        && pgy_codegen_call_name_is_read(callee_name))
                        return inner;
                    if (inner != NULL)
                        return "Void";
                }
            }
            const char *declared_ret = llvm_stmt_declared_return_type_name(ctx,
                callee_name);
            if (declared_ret != NULL)
                return declared_ret;
        }
        return NULL;
    default:
        return NULL;
    }
}

const char *
llvm_infer_spawn_future_inner(LLVMGenCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = ast_spawn_function(spawn_expr);
    ASTNode *call = NULL;
    ASTNode *callee = target;
    const char *callee_name = NULL;
    ASTNode *decl;
    ASTNode *return_type;
    const char *ret_name;
    bool generic_func;
    bool extern_func;
    const MIRRoutine *routine = NULL;
    bool use_mir_signature = false;

    if (target != NULL && target->type == AST_CALL) {
        call = target;
        callee = ast_call_callee(target);
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);
    if (callee_name == NULL)
        return NULL;

    decl = llvm_stmt_find_function_decl_by_name(ctx, callee_name);
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    generic_func =
        ast_generic_param_count(ast_declaration_generic_params(decl)) > 0;
    extern_func = llvm_decl_is_extern_function(ctx, decl);
    if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
        routine = llvm_active_function_routine_for_source_ast(ctx, decl);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing spawn future inference routine for '%s'",
                callee_name);
            return NULL;
        }
        if (!llvm_mir_routine_signature_metadata_complete_for(ctx,
                routine, decl,
                LLVM_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only LLVM path missing spawn future inference signature metadata for '%s'",
                "MIR-only LLVM path missing spawn future inference return type-name metadata for '%s'",
                "MIR-only LLVM path missing spawn future inference parameter type-name metadata for '%s'")) {
            return NULL;
        }
        ret_name = llvm_mir_routine_return_type_name(routine);
        return_type = llvm_mir_routine_return_type(routine);
        use_mir_signature = true;
    } else {
        return_type = ast_func_return_type(decl);
        ret_name = ast_type_name(return_type);
    }

    if (ret_name == NULL)
        return NULL;
    if (!(ret_name[0] >= 'A' && ret_name[0] <= 'Z' && ret_name[1] == '\0'))
        return ret_name;

    if (call == NULL)
        return NULL;

    size_t param_count = use_mir_signature
        ? llvm_mir_routine_param_count(routine)
        : ast_func_param_count(decl);
    for (size_t i = 0; i < param_count && i < ast_call_arg_count(call); i++) {
        FuncParam *param = use_mir_signature
            ? llvm_mir_routine_param(routine, i)
            : ast_func_param(decl, i);
        const char *param_type_name = use_mir_signature
            ? llvm_mir_routine_param_type_name(routine, i)
            : (param != NULL ? ast_type_name(param->type) : NULL);
        if (param == NULL)
            continue;
        if (param_type_name == NULL)
            continue;
        if (strcmp(param_type_name, ret_name) == 0) {
            const char *actual_type = llvm_simple_expr_type_name(ctx,
                ast_call_argument(call, i));
            if (actual_type == NULL || actual_type[0] == '\0')
                continue;
            return ctx != NULL
                ? pgy_arena_strdup(&ctx->scratch, actual_type)
                : actual_type;
        }
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
