/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend nominal/extern registration helpers split from llvm_pipeline.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static LLVMTypeRef
llvm_register_required_ast_type(LLVMGenCtx *ctx,
                                ASTNode *owner,
                                ASTNode *type_node,
                                const char *surface)
{
    if (ctx == NULL)
        return NULL;
    if (type_node != NULL)
        return ast_type_to_llvm(ctx, type_node);

    llvm_set_error_at_with_hints(ctx, owner,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM registration for %s requires explicit type metadata; silent i32 fallback is not allowed",
        surface != NULL ? surface : "declaration");
    return ctx->type_i32;
}

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

    if (has_data && llvm_lookup_class(ctx, enum_name) == NULL) {
        size_t variant_count = stmt->data.enum_decl.variant_count;
        /* Type-array buffers used to feed LLVMStructSetBody and
         * llvm_class_add_field.  LLVM copies the array contents into its
         * own type definitions, so the buffers never need to outlive this
         * function.  Allocated from the ctx-scope scratch arena, which
         * shares block reuse with other LLVM-lowering scratch sites. */
        LLVMTypeRef *enum_fields = pgy_arena_calloc(&ctx->scratch,
            (variant_count + 1) * sizeof(LLVMTypeRef));
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

            LLVMTypeRef *payload_fields = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            for (size_t p = 0; p < param_count; p++) {
                ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                payload_fields[p] = llvm_register_required_ast_type(
                    ctx, stmt, pt, "enum variant payload");
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
            /* payload_fields is arena-owned. */
        }

        LLVMStructSetBody(enum_ty, enum_fields, (unsigned)(variant_count + 1), 0);
        /* enum_fields / payload_fields are ctx->scratch-owned;
         * destroyed in llvm_ctx_destroy(). */
    }

    for (size_t j = 0; j < stmt->data.enum_decl.method_count; j++) {
        ASTNode *method = stmt->data.enum_decl.methods[j];
        const MIRDeclMethod *method_meta = NULL;
        const char *method_name = NULL;
        size_t pc = 0;
        ASTNode *return_type = NULL;
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        method_meta = llvm_find_host_method_metadata_in_context(
            ctx, enum_name, method->data.func_decl.name);
        if (method_meta == NULL) {
            llvm_set_error(ctx,
                "MIR-only LLVM path missing enum method declaration metadata");
            return;
        }
        method_name = llvm_mir_decl_method_name(method_meta);
        pc = llvm_mir_decl_method_param_count(method_meta);
        return_type = llvm_mir_decl_method_return_type(method_meta);
        if (method_name == NULL)
            continue;

        LLVMTypeRef ret_type = ctx->type_void;
        if (return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, return_type);

        size_t user_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            user_pc++;
        }

        LLVMTypeRef self_type = ctx->type_i32;
        LLVMClassTypeEntry *enum_entry = llvm_lookup_class(ctx, enum_name);
        if (enum_entry != NULL)
            self_type = enum_entry->struct_type;
        /* Param-type buffer is consumed by LLVMFunctionType (copies). */
        LLVMTypeRef *param_types = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        param_types[0] = self_type;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            param_types[pidx++] = llvm_register_required_ast_type(
                ctx, method, p != NULL ? p->type : NULL, "enum method parameter");
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s_%s", enum_name, method_name);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, full_name, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret_type);
        /* param_types is ctx->scratch-owned. */
    }
}

void
llvm_register_nominal_decl(LLVMGenCtx *ctx, ASTNode *stmt)
{
    if (ctx == NULL || stmt == NULL)
        return;
    if (stmt->type == AST_ENUM_DECL) {
        llvm_register_enum_decl(ctx, stmt);
        return;
    }
    if (stmt->type != AST_CLASS_DECL)
        return;

    const char *cls_name = stmt->data.class_decl.name;
    if (cls_name == NULL || llvm_lookup_class(ctx, cls_name) != NULL)
        return;
    size_t fc = stmt->data.class_decl.field_count;
    /* Field-type buffer: consumed by LLVMStructSetBody (copies) and read
     * once for llvm_class_add_field below; never retained. */
    LLVMTypeRef *field_types = pgy_arena_calloc(&ctx->scratch,
        (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
    for (size_t j = 0; j < fc; j++) {
        ClassField *f = stmt->data.class_decl.fields[j];
        field_types[j] = llvm_register_required_ast_type(
            ctx, stmt, f != NULL ? f->type : NULL, "class field");
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

    /* field_types is ctx->scratch-owned. */

    for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
        ASTNode *method = stmt->data.class_decl.methods[j];
        const MIRDeclMethod *method_meta = NULL;
        const char *method_name = NULL;
        size_t pc = 0;
        ASTNode *return_type = NULL;
        bool method_is_action = false;
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        method_meta = llvm_find_host_method_metadata_in_context(
            ctx, cls_name, method->data.func_decl.name);
        if (method_meta == NULL) {
            llvm_set_error(ctx,
                "MIR-only LLVM path missing class method declaration metadata");
            return;
        }
        method_name = llvm_mir_decl_method_name(method_meta);
        pc = llvm_mir_decl_method_param_count(method_meta);
        return_type = llvm_mir_decl_method_return_type(method_meta);
        method_is_action = llvm_mir_decl_method_is_action_like(method_meta);
        if (method_name == NULL)
            continue;

        LLVMTypeRef ret_type = ctx->type_void;
        if (return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, return_type);

        size_t user_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            user_pc++;
        }

        /* Per-method param-type buffer: consumed by LLVMFunctionType. */
        LLVMTypeRef *param_types = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        param_types[0] = is_pointer_self_host ? LLVMPointerType(struct_ty, 0) : struct_ty;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            param_types[pidx++] = llvm_register_required_ast_type(
                ctx, method, p != NULL ? p->type : NULL, "class method parameter");
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s_%s", cls_name, method_name);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, full_name, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret_type);
        llvm_set_function_flags(ctx, LLVMGetValueName(fn),
                                method_is_action,
                                method_is_action && user_pc == 0);
        /* param_types is ctx->scratch-owned. */
    }
}

void
llvm_register_active_nominal_types(LLVMGenCtx *ctx)
{
    ASTNode **nominal_nodes = NULL;
    size_t nominal_count = 0;

    if (ctx == NULL)
        return;

    llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count);
    for (size_t i = 0; i < nominal_count; i++) {
        ASTNode *stmt = nominal_nodes != NULL ? nominal_nodes[i] : NULL;
        if (stmt == NULL)
            continue;
        if (stmt->type != AST_CLASS_DECL
            && stmt->type != AST_ENUM_DECL) {
            continue;
        }
        llvm_register_nominal_decl(ctx, stmt);
    }
}

void
llvm_register_active_extern_prototypes(LLVMGenCtx *ctx)
{
    ASTNode **externs = NULL;
    size_t extern_count = 0;

    if (ctx == NULL)
        return;

    llvm_active_externs(ctx, &externs, &extern_count);
    if (externs == NULL)
        return;

    for (size_t i = 0; i < extern_count; i++) {
        ASTNode *stmt = externs[i];
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
            /* Extern param-type buffer: consumed by LLVMFunctionType. */
            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (pc > 0 ? pc : 1) * sizeof(LLVMTypeRef));
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = decl->data.func_decl.params[k];
                ptypes[k] = llvm_register_required_ast_type(
                    ctx, decl, p != NULL ? p->type : NULL, "extern parameter");
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)pc, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            /* ptypes is ctx->scratch-owned. */
        }
    }
}

#endif
