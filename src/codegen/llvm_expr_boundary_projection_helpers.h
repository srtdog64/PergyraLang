static const char *
llvm_boundary_slot_inner_name(LLVMGenCtx *ctx, FuncParam *param, bool *is_secure_out)
{
    const char *type_name;
    const char *inner_name;
    GenericParams *generic_args;
    LLVMClassTypeEntry *entry;

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

    entry = llvm_lookup_class(ctx, inner_name);
    (void)entry;

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}

static LLVMValueRef
llvm_boundary_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    if (ctx == NULL || slot_var == NULL)
        return NULL;
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, slot_var->type, slot_var->alloca,
                              llvm_tmp_name(ctx));
    }
    return slot_var->alloca;
}

static ASTNode *
llvm_find_function_decl(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    return llvm_lookup_generic_template(ctx, name);
}

static ASTNode *
llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_INTENT_DECL, name);
}

static bool
llvm_boundary_param_uses_pointer_self(LLVMGenCtx *ctx, FuncParam *param)
{
    return param != NULL && llvm_ast_type_uses_pointer_self(ctx, param->type);
}

static LLVMValueRef *
llvm_build_boundary_call_args(LLVMGenCtx *ctx, ASTNode *decl,
                              ASTNode **arg_nodes, size_t argc,
                              unsigned *out_count)
{
    LLVMValueRef *args;
    unsigned emitted_count = 0;

    if (out_count != NULL)
        *out_count = 0;
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;

    for (size_t i = 0; i < decl->data.func_decl.param_count; i++) {
        bool is_secure = false;
        FuncParam *p = decl->data.func_decl.params[i];
        emitted_count++;
        if (llvm_boundary_slot_inner_name(ctx, p, &is_secure) != NULL && is_secure)
            emitted_count++;
    }

    args = pgy_arena_calloc(&ctx->scratch,
                            (emitted_count > 0 ? emitted_count : 1) * sizeof(LLVMValueRef));
    if (args == NULL)
        return NULL;

    unsigned arg_idx = 0;
    unsigned emitted_idx = 0;
    for (size_t i = 0; i < decl->data.func_decl.param_count && arg_idx < argc; i++) {
        bool is_secure = false;
        FuncParam *p = decl->data.func_decl.params[i];
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        ASTNode *arg_node = arg_nodes[arg_idx++];
        bool pointer_self = llvm_boundary_param_uses_pointer_self(ctx, p);

        if (inner != NULL && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *source_name = arg_node->data.identifier.name;
            LLVMVarEntry *slot_var = llvm_scope_lookup(ctx, source_name);
            args[emitted_idx++] = slot_var != NULL
                ? llvm_boundary_slot_runtime_arg(ctx, slot_var)
                : llvm_emit_expression(arg_node, ctx);
            if (is_secure) {
                LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
                if (token_var != NULL) {
                    LLVMTypeRef token_ty = token_var->type;
                    args[emitted_idx++] = LLVMBuildLoad2(ctx->builder, token_ty,
                        token_var->alloca, llvm_tmp_name(ctx));
                } else {
                    args[emitted_idx++] = LLVMConstNull(llvm_secure_token_type(ctx, inner));
                }
            }
            continue;
        }

        if (pointer_self && arg_node != NULL) {
            if (arg_node->type == AST_IDENTIFIER) {
                const char *source_name = arg_node->data.identifier.name;
                LLVMVarEntry *var = llvm_scope_lookup(ctx, source_name);
                if (var != NULL) {
                    args[emitted_idx++] = LLVMGetTypeKind(var->type) == LLVMPointerTypeKind
                        ? LLVMBuildLoad2(ctx->builder, var->type, var->alloca, llvm_tmp_name(ctx))
                        : var->alloca;
                    continue;
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                LLVMValueRef ptr = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                if (ptr != NULL) {
                    args[emitted_idx++] = ptr;
                    continue;
                }
            }
        }

        args[emitted_idx++] = llvm_emit_expression(arg_node, ctx);
    }

    if (out_count != NULL)
        *out_count = emitted_idx;
    return args;
}

static void
llvm_append_mangled_suffix(char *buf, size_t buf_size, const char *suffix)
{
    if (buf == NULL || buf_size == 0 || suffix == NULL)
        return;

    size_t len = strlen(buf);
    if (len >= buf_size - 1)
        return;

    buf[len++] = '_';

    size_t remaining = buf_size - len - 1;
    size_t suffix_len = strlen(suffix);
    if (suffix_len > remaining)
        suffix_len = remaining;

    memcpy(buf + len, suffix, suffix_len);
    buf[len + suffix_len] = '\0';
}

#include "llvm_expr_projection_path_helpers.h"
