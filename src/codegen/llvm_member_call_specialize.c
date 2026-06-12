#ifdef PGY_LLVM_ENABLED
/*
 * On-demand emission of a specialized generic-class method on the LLVM backend.
 *
 * A method call p.GetFirst() on a variable of a specialized generic class
 * (Pair<Int>) resolves, in llvm_member_call_emit, to the function name
 * "Pair<Int>_GetFirst". The C backend monomorphizes such methods ahead of time;
 * the LLVM backend discovers instantiations lazily, so the specialized method
 * body is emitted here the first time the call is lowered, with the class type
 * parameters bound to the instantiation arguments through the type-substitution
 * stack. The function is registered under the same name the call site looks up,
 * so a retry of that lookup finds it.
 */
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_limits_internal.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../common/string_compat.h"

#include <llvm-c/Core.h>
#include <string.h>

/*
 * Copy the index-th comma-separated argument of a generic instantiation name
 * (the text between the outermost angle brackets) into out. Returns false if
 * the index is out of range or the buffer is too small.
 */
static bool
llvm_specialize_extract_arg(const char *type_name, size_t index,
                            char *out, size_t out_size)
{
    const char *lt = type_name != NULL ? strchr(type_name, '<') : NULL;
    const char *cursor;
    int depth;
    size_t current;
    const char *arg_start;

    if (lt == NULL || out == NULL || out_size == 0)
        return false;
    cursor = lt + 1;
    depth = 1;
    current = 0;
    arg_start = cursor;
    for (; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            depth++;
        } else if (*cursor == '>') {
            depth--;
            if (depth == 0)
                break;
        } else if (*cursor == ',' && depth == 1) {
            if (current == index) {
                break;
            }
            current++;
            arg_start = cursor + 1;
        }
    }
    if (current != index)
        return false;
    {
        size_t len = (size_t)(cursor - arg_start);
        while (len > 0 && arg_start[0] == ' ') {
            arg_start++;
            len--;
        }
        while (len > 0 && arg_start[len - 1] == ' ')
            len--;
        if (len == 0 || len >= out_size)
            return false;
        memcpy(out, arg_start, len);
        out[len] = '\0';
    }
    return true;
}

bool
llvm_emit_specialized_method_ondemand(LLVMGenCtx *ctx, const char *class_name,
                                      const char *method_name)
{
    const char *lt;
    char base[128];
    char full_name[256];
    size_t base_len;
    ASTNode *tmpl;
    const MIRDeclHeader *generic_header;
    ASTNode *method_decl;
    GenericParams *gp;
    size_t gpc;
    LLVMClassTypeEntry *spec_cls;
    LLVMTypeRef self_ty;
    LLVMTypeRef ret_ty;
    LLVMTypeRef *ptypes;
    size_t pc;
    size_t real_pc;
    int saved_subst;
    LLVMTypeRef ft;
    LLVMValueRef fn;
    LLVMBasicBlockRef entry;
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    LLVMTypeRef saved_function_ret;
    ASTNode *saved_func_decl;
    LLVMBasicBlockRef saved_bb;
    unsigned pidx;

    if (ctx == NULL || class_name == NULL || method_name == NULL)
        return false;
    lt = strchr(class_name, '<');
    if (lt == NULL)
        return false;

    snprintf(full_name, sizeof(full_name), "%s_%s", class_name, method_name);
    if (llvm_lookup_function(ctx, full_name) != NULL)
        return true;

    spec_cls = llvm_lookup_class(ctx, class_name);
    if (spec_cls == NULL)
        return false;
    self_ty = spec_cls->struct_type;

    base_len = (size_t)(lt - class_name);
    if (base_len == 0 || base_len >= sizeof(base))
        return false;
    memcpy(base, class_name, base_len);
    base[base_len] = '\0';

    tmpl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, base);
    method_decl = llvm_find_nominal_host_method_decl(ctx, base, method_name);
    if (tmpl == NULL || method_decl == NULL)
        return false;
    generic_header = llvm_find_decl_header_in_context_of_type(ctx,
        AST_CLASS_DECL, base);
    gp = generic_header == NULL ? ast_declaration_generic_params(tmpl) : NULL;
    gpc = generic_header != NULL
        ? mir_decl_header_generic_param_count(generic_header)
        : ast_generic_param_count(gp);
    if (gpc == 0 || gpc > MAX_TYPE_SUBST)
        return false;

    saved_subst = ctx->type_subst_count;
    for (size_t gi = 0; gi < gpc; gi++) {
        const MIRDeclGenericParam *meta = generic_header != NULL
            ? mir_decl_header_generic_param(generic_header, gi)
            : NULL;
        GenericParam *p = generic_header == NULL
            ? ast_generic_param_at(gp, gi)
            : NULL;
        const char *pname = meta != NULL
            ? mir_decl_generic_param_name(meta)
            : ast_generic_param_name(p);
        char arg_buf[256];
        LLVMTypeRef arg_ty;
        if (pname == NULL
            || !llvm_specialize_extract_arg(class_name, gi, arg_buf,
                   sizeof(arg_buf))) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        arg_ty = pergyra_type_to_llvm(ctx, arg_buf);
        if (arg_ty == NULL || ctx->type_subst_count >= MAX_TYPE_SUBST) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        ctx->type_subst[ctx->type_subst_count].param_name = pname;
        ctx->type_subst[ctx->type_subst_count].llvm_type = arg_ty;
        ctx->type_subst[ctx->type_subst_count].type_name =
            pergyra_strdup(arg_buf);
        ctx->type_subst_count++;
    }

    {
        ASTNode *rt = ast_func_return_type(method_decl);
        ret_ty = rt != NULL ? ast_type_to_llvm(ctx, rt) : ctx->type_void;
        if (ret_ty == NULL) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
    }

    pc = ast_func_param_count(method_decl);
    ptypes = pgy_arena_calloc(&ctx->scratch, (pc + 1) * sizeof(LLVMTypeRef));
    if (ptypes == NULL) {
        llvm_type_subst_restore_owned(ctx, saved_subst);
        return false;
    }
    real_pc = 0;
    ptypes[real_pc++] = self_ty;
    for (size_t k = 0; k < pc; k++) {
        FuncParam *p = ast_func_param(method_decl, k);
        LLVMTypeRef pt;
        if (p == NULL || llvm_param_is_implicit_self(p) || p->type == NULL)
            continue;
        pt = ast_type_to_llvm(ctx, p->type);
        if (pt == NULL) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        ptypes[real_pc++] = pt;
    }

    ft = LLVMFunctionType(ret_ty, ptypes, (unsigned)real_pc, 0);
    fn = LLVMAddFunction(ctx->module, full_name, ft);
    llvm_register_function(ctx, full_name, fn, ft, ret_ty);

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_function_ret = ctx->current_function_ret_type;
    saved_func_decl = ctx->current_func_decl;
    saved_bb = LLVMGetInsertBlock(ctx->builder);

    ctx->current_function = fn;
    ctx->current_ret_type = ret_ty;
    ctx->current_function_ret_type = ret_ty;
    ctx->current_func_decl = method_decl;
    entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    llvm_scope_push(ctx);

    {
        LLVMValueRef self_alloca =
            llvm_create_entry_alloca(ctx, self_ty, "self");
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), self_alloca);
        llvm_scope_declare(ctx, "self", self_alloca, self_ty);
        llvm_register_var_class(ctx, "self", class_name);
    }
    pidx = 1;
    for (size_t k = 0; k < pc; k++) {
        FuncParam *p = ast_func_param(method_decl, k);
        LLVMTypeRef pt;
        LLVMValueRef a;
        if (p == NULL || llvm_param_is_implicit_self(p)
            || p->name == NULL || p->type == NULL)
            continue;
        pt = ast_type_to_llvm(ctx, p->type);
        if (pt == NULL)
            continue;
        a = llvm_create_entry_alloca(ctx, pt, p->name);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, pidx++), a);
        llvm_scope_declare(ctx, p->name, a, pt);
    }

    {
        ASTNode *body = ast_func_body(method_decl);
        if (body != NULL)
            llvm_emit_block(body, ctx);
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (ret_ty == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder, LLVMConstNull(ret_ty));
    }

    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_func_decl = saved_func_decl;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    llvm_type_subst_restore_owned(ctx, saved_subst);
    return !ctx->has_error;
}
#endif /* PGY_LLVM_ENABLED */
