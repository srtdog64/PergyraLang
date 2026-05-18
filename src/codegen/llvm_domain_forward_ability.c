/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM ability vtable forward declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_forward_internal.h"

void
llvm_emit_domain_ability_vtables(LLVMGenCtx *ctx,
                                 ASTNode **abilities,
                                 size_t ability_count)
{
    if (ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        const char *ab_name;
        size_t mc;
        LLVMTypeRef *vt_fields;
        char vt_name[256];
        LLVMTypeRef vt_struct;
        LLVMClassTypeEntry *entry;

        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        ab_name = ast_ability_name(stmt);
        mc = ast_ability_method_count(stmt);
        vt_fields = pgy_arena_calloc(&ctx->scratch,
            (mc > 0 ? mc : 1) * sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = ast_ability_method(stmt, j);
            LLVMTypeRef ret;
            size_t pc;
            size_t user_pc = 0;
            LLVMTypeRef *ptypes;
            size_t pidx = 1;
            LLVMTypeRef fn_type;

            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            {
                const char *mname =
                    llvm_domain_method_name_metadata_first(NULL, method);
                ASTNode *return_type =
                    llvm_domain_method_return_type_metadata_first(NULL, method);

                ret = ctx->type_void;
                if (return_type != NULL) {
                    ret = ast_type_to_llvm(ctx, return_type);
                    if (ctx->has_error || ret == NULL)
                        return;
                }

                pc = llvm_domain_method_param_count_metadata_first(NULL, method);
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p =
                        llvm_domain_method_param_metadata_first(NULL, method, k);
                    if (!llvm_param_is_implicit_self_local(p))
                        user_pc++;
                }
                ptypes = pgy_arena_calloc(&ctx->scratch,
                    (user_pc + 1) * sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p =
                        llvm_domain_method_param_metadata_first(NULL, method, k);
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    LLVMTypeRef pt = llvm_domain_forward_required_param_type(
                        ctx, method, p, "ability method", mname);
                    if (ctx->has_error || pt == NULL)
                        return;
                    ptypes[pidx++] = pt;
                }
            }

            fn_type = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
        }

        if (!llvm_domain_forward_suffix_name(vt_name, sizeof(vt_name),
                ab_name, "vtable")) {
            llvm_set_error(ctx,
                "LLVM ability vtable name is too long for '%s'", ab_name);
            return;
        }
        vt_struct = LLVMStructCreateNamed(ctx->context, vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        entry = llvm_register_class(ctx, pergyra_strdup(vt_name),
                                    vt_struct, false, false);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = ast_ability_method(stmt, j);
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        llvm_domain_method_name_metadata_first(NULL, method),
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }
}

#endif
