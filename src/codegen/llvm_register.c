/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend nominal/extern registration helpers split from llvm_pipeline.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void
llvm_register_enum_decl(LLVMGenCtx *ctx, ASTNode *stmt)
{
    if (ctx == NULL || stmt == NULL || stmt->type != AST_ENUM_DECL)
        return;

    const char *enum_name = stmt->data.enum_decl.name;
    if (enum_name == NULL)
        return;

    bool has_data = false;
    for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
        if (stmt->data.enum_decl.variant_param_counts != NULL
            && stmt->data.enum_decl.variant_param_counts[j] > 0) {
            has_data = true;
        }
        if (llvm_lookup_enum_variant_qualified(ctx, enum_name,
                stmt->data.enum_decl.variants[j]) == NULL) {
            llvm_register_enum_variant(ctx, enum_name,
                stmt->data.enum_decl.variants[j], (int)j);
        }
    }

    if (!has_data || llvm_lookup_class(ctx, enum_name) != NULL)
        goto register_methods;

    size_t variant_count = stmt->data.enum_decl.variant_count;
    LLVMTypeRef *enum_fields = calloc((variant_count + 1), sizeof(LLVMTypeRef));
    LLVMTypeRef enum_ty = LLVMStructCreateNamed(ctx->context, enum_name);
    LLVMClassTypeEntry *enum_entry =
        llvm_register_class(ctx, enum_name, enum_ty, false, false);

    enum_fields[0] = ctx->type_i32;
    if (enum_entry != NULL)
        llvm_class_add_field(enum_entry, "tag", ctx->type_i32, 0);

    for (size_t j = 0; j < variant_count; j++) {
        const char *variant_name = stmt->data.enum_decl.variants[j];
        size_t param_count = (stmt->data.enum_decl.variant_param_counts != NULL)
            ? stmt->data.enum_decl.variant_param_counts[j] : 0;

        if (param_count == 0) {
            enum_fields[j + 1] = LLVMStructTypeInContext(ctx->context, NULL, 0, 0);
            continue;
        }

        char payload_name[256];
        snprintf(payload_name, sizeof(payload_name), "%s$%s", enum_name, variant_name);

        LLVMTypeRef *payload_fields = calloc(param_count, sizeof(LLVMTypeRef));
        for (size_t p = 0; p < param_count; p++) {
            ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
            payload_fields[p] = (pt != NULL) ? ast_type_to_llvm(ctx, pt) : ctx->type_i32;
        }

        LLVMTypeRef payload_ty = LLVMStructCreateNamed(ctx->context, payload_name);
        LLVMStructSetBody(payload_ty, payload_fields, (unsigned)param_count, 0);

        LLVMClassTypeEntry *payload_entry =
            llvm_register_class(ctx, pergyra_strdup(payload_name), payload_ty, false, false);
        if (payload_entry != NULL) {
            for (size_t p = 0; p < param_count; p++) {
                char field_name[32];
                snprintf(field_name, sizeof(field_name), "_%zu", p);
                llvm_class_add_field(payload_entry,
                    pergyra_strdup(field_name),
                    payload_fields[p], (int)p);
            }
        }

        enum_fields[j + 1] = payload_ty;
        if (enum_entry != NULL)
            llvm_class_add_field(enum_entry, variant_name, payload_ty, (int)(j + 1));
        free(payload_fields);
    }

    LLVMStructSetBody(enum_ty, enum_fields, (unsigned)(variant_count + 1), 0);
    free(enum_fields);

register_methods:
    for (size_t j = 0; j < stmt->data.enum_decl.method_count; j++) {
        ASTNode *method = stmt->data.enum_decl.methods[j];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        size_t pc = method->data.func_decl.param_count;
        LLVMTypeRef ret_type = ctx->type_void;
        if (method->data.func_decl.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, method->data.func_decl.return_type);

        size_t user_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = method->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            user_pc++;
        }

        LLVMTypeRef self_type = ctx->type_i32;
        LLVMClassTypeEntry *enum_entry = llvm_lookup_class(ctx, enum_name);
        if (enum_entry != NULL)
            self_type = enum_entry->struct_type;
        LLVMTypeRef *param_types = calloc(user_pc + 1, sizeof(LLVMTypeRef));
        param_types[0] = self_type;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = method->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            param_types[pidx++] = (p->type != NULL) ? ast_type_to_llvm(ctx, p->type)
                                                    : ctx->type_i32;
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s_%s", enum_name, method_name);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, full_name, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret_type);
        free(param_types);
    }
}

static void
llvm_register_hir_class_decl(LLVMGenCtx *ctx, ASTNode *stmt)
{
    const char *cls_name = stmt->data.class_decl.name;
    size_t fc = stmt->data.class_decl.field_count;
    LLVMTypeRef *field_types = calloc(fc > 0 ? fc : 1, sizeof(LLVMTypeRef));
    for (size_t j = 0; j < fc; j++) {
        ClassField *f = stmt->data.class_decl.fields[j];
        field_types[j] = (f->type != NULL) ? ast_type_to_llvm(ctx, f->type) : ctx->type_i32;
    }

    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context, cls_name);
    LLVMStructSetBody(struct_ty, field_types, (unsigned)fc, 0);

    NominalDeclKind nominal_kind = stmt->data.class_decl.nominal_kind;
    bool is_subject = nominal_kind == NOMINAL_DECL_SUBJECT;
    bool is_pointer_self_host = is_subject || nominal_kind == NOMINAL_DECL_VESSEL;
    bool is_immutable = llvm_nominal_uses_immutable_projection_storage(nominal_kind);
    bool is_boundary_transfer = llvm_nominal_is_boundary_transfer_contract(nominal_kind);
    LLVMClassTypeEntry *entry = llvm_register_class(ctx, cls_name, struct_ty,
                                                    is_subject, is_pointer_self_host);
    if (entry != NULL) {
        entry->is_immutable = is_immutable;
        entry->is_boundary_transfer_contract = is_boundary_transfer;
        for (size_t j = 0; j < fc; j++) {
            ClassField *f = stmt->data.class_decl.fields[j];
            llvm_class_add_field(entry, f->name, field_types[j], (int)j);
        }
    }

    free(field_types);

    for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
        ASTNode *method = stmt->data.class_decl.methods[j];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        size_t pc = method->data.func_decl.param_count;
        LLVMTypeRef ret_type = ctx->type_void;
        if (method->data.func_decl.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, method->data.func_decl.return_type);

        size_t user_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = method->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            user_pc++;
        }

        LLVMTypeRef *param_types = calloc(user_pc + 1, sizeof(LLVMTypeRef));
        param_types[0] = is_pointer_self_host ? LLVMPointerType(struct_ty, 0) : struct_ty;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = method->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            param_types[pidx++] = (p->type != NULL) ? ast_type_to_llvm(ctx, p->type)
                                                    : ctx->type_i32;
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s_%s", cls_name, method_name);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, full_name, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret_type);
        llvm_set_function_flags(ctx, LLVMGetValueName(fn),
                                method->data.func_decl.is_action,
                                method->data.func_decl.is_action && user_pc == 0);
        free(param_types);
    }
}

void
llvm_register_hir_nominal_types(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < hir->type_count; i++) {
        ASTNode *stmt = hir->types[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL) {
            llvm_register_enum_decl(ctx, stmt);
            continue;
        }
        if (stmt != NULL && stmt->type == AST_CLASS_DECL)
            llvm_register_hir_class_decl(ctx, stmt);
    }

}

void
llvm_register_hir_extern_prototypes(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL || stmt->type != AST_EXTERN_BLOCK)
            continue;

        for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
            ASTNode *decl = stmt->data.extern_block.declarations[j];
            if (decl == NULL || decl->type != AST_FUNC_DECL)
                continue;

            const char *fname = decl->data.func_decl.name;
            if (llvm_lookup_function(ctx, fname) != NULL)
                continue;

            LLVMTypeRef ret = ctx->type_void;
            if (decl->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx, decl->data.func_decl.return_type);

            size_t pc = decl->data.func_decl.param_count;
            LLVMTypeRef *ptypes = calloc(pc > 0 ? pc : 1, sizeof(LLVMTypeRef));
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = decl->data.func_decl.params[k];
                ptypes[k] = (p->type != NULL) ? ast_type_to_llvm(ctx, p->type)
                                              : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)pc, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            free(ptypes);
        }
    }
}

#endif
