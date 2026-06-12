/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend nominal/extern registration helpers split from llvm_pipeline.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

static bool
llvm_register_join_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                        const char *left, const char *sep,
                        const char *right, const char *surface)
{
    int written;

    if (out == NULL || out_size == 0 || left == NULL || sep == NULL
        || right == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s%s%s", left, sep, right);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM registration generated name is too long for %s",
        surface != NULL ? surface : "declaration");
    return false;
}

static bool
llvm_register_payload_field_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                                 size_t index)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    written = snprintf(out, out_size, "_%zu", index);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM enum payload field name is too long");
    return false;
}

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
    return NULL;
}

void
llvm_register_enum_decl(LLVMGenCtx *ctx, ASTNode *stmt)
{
    if (ctx == NULL || stmt == NULL || stmt->type != AST_ENUM_DECL)
        return;

    const char *enum_name = llvm_decl_node_name(stmt);
    size_t variant_count = 0;
    char **variants = ast_enum_variants(stmt, &variant_count);
    if (enum_name == NULL)
        return;

    bool has_data = false;
    for (size_t j = 0; j < variant_count; j++) {
        const char *variant_name = variants != NULL ? variants[j] : NULL;
        if (ast_enum_variant_param_count(stmt, j) > 0) {
            has_data = true;
        }
        if (llvm_lookup_enum_variant_qualified(ctx, enum_name,
                variant_name) == NULL) {
            llvm_register_enum_variant(ctx, enum_name,
                variant_name, (int)j);
        }
    }

    if (has_data && llvm_lookup_class(ctx, enum_name) == NULL) {
        /* Type-array buffers used to feed LLVMStructSetBody and
         * llvm_class_add_field.  LLVM copies the array contents into its
         * own type definitions, so the buffers never need to outlive this
         * function.  Allocated from the ctx-scope scratch arena, which
         * shares block reuse with other LLVM-lowering scratch sites. */
        LLVMTypeRef *enum_fields = pgy_arena_calloc(&ctx->scratch,
            (variant_count + 1) * sizeof(LLVMTypeRef));
        if (enum_fields == NULL) {
            llvm_set_error(ctx,
                "LLVM enum field allocation failed for '%s'",
                enum_name);
            return;
        }
        LLVMTypeRef enum_ty = LLVMStructCreateNamed(ctx->context, enum_name);
        LLVMClassTypeEntry *enum_entry =
            llvm_register_class(ctx, enum_name, enum_ty, false, false);

        enum_fields[0] = ctx->type_i32;
        if (enum_entry != NULL)
            llvm_class_add_field(enum_entry, "tag", ctx->type_i32, 0);

        for (size_t j = 0; j < variant_count; j++) {
            const char *variant_name = variants != NULL ? variants[j] : NULL;
            size_t param_count = ast_enum_variant_param_count(stmt, j);

            if (param_count == 0) {
                enum_fields[j + 1] = LLVMStructTypeInContext(ctx->context, NULL, 0, 0);
                continue;
            }

            char payload_name[256];
            if (!llvm_register_join_name(ctx, payload_name,
                    sizeof(payload_name), enum_name, "$", variant_name,
                    "enum variant payload")) {
                return;
            }

            LLVMTypeRef *payload_fields = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            if (payload_fields == NULL) {
                llvm_set_error(ctx,
                    "LLVM enum payload field allocation failed for '%s.%s'",
                    enum_name,
                    variant_name != NULL ? variant_name : "<anonymous>");
                return;
            }
            for (size_t p = 0; p < param_count; p++) {
                ASTNode *pt = ast_enum_variant_param(stmt, j, p);
                payload_fields[p] = llvm_register_required_ast_type(
                    ctx, stmt, pt, "enum variant payload");
                if (ctx->has_error || payload_fields[p] == NULL)
                    return;
            }

            LLVMTypeRef payload_ty = LLVMStructCreateNamed(ctx->context, payload_name);
            LLVMStructSetBody(payload_ty, payload_fields, (unsigned)param_count, 0);

            LLVMClassTypeEntry *payload_entry =
                llvm_register_class(ctx, pergyra_strdup(payload_name), payload_ty, false, false);
            if (payload_entry != NULL) {
                for (size_t p = 0; p < param_count; p++) {
                    char field_name[32];
                    if (!llvm_register_payload_field_name(ctx, field_name,
                            sizeof(field_name), p)) {
                        return;
                    }
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

    LLVMHostedMethodView enum_method_view =
        llvm_hosted_method_view_from_decl(ctx, enum_name, stmt);
    if (llvm_hosted_method_view_missing_mir_metadata(&enum_method_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing enum method declaration metadata");
        return;
    }
    if (!llvm_require_hosted_method_view_rows(
            ctx,
            &enum_method_view,
            "MIR-only LLVM path has invalid method declaration metadata row for enum '%s'",
            enum_name != NULL ? enum_name : "(anonymous-enum)")) {
        return;
    }

    if (!enum_method_view.uses_mir_metadata)
        return;
    for (size_t j = 0; j < enum_method_view.count; j++) {
        const MIRDeclMethod *method_meta =
            llvm_hosted_method_view_metadata(&enum_method_view, j);
        ASTNode *method = llvm_mir_decl_method_source_ast(method_meta);
        const char *method_name = NULL;
        size_t pc = 0;
        const char *return_type_name = NULL;
        ASTNode *return_type = NULL;

        method_name = llvm_mir_decl_method_name(method_meta);
        pc = llvm_mir_decl_method_param_count(method_meta);
        return_type_name = llvm_mir_decl_method_return_type_name(method_meta);
        return_type = llvm_mir_decl_method_return_type(method_meta);
        if (method_name == NULL)
            continue;
        if (!llvm_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                enum_name,
                method_name,
                LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only LLVM path missing enum method registry return type-name metadata for '%s.%s'",
                "MIR-only LLVM path missing enum method registry parameter type-name metadata for '%s.%s'")) {
            return;
        }

        LLVMTypeRef ret_type = ctx->type_void;
        if (return_type_name != NULL)
            ret_type = pergyra_type_to_llvm(ctx, return_type_name);
        else if (return_type != NULL) {
            ret_type = ast_type_to_llvm(ctx, return_type);
        }
        if (ctx->has_error || ret_type == NULL)
            return;

        size_t user_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            user_pc++;
        }

        LLVMTypeRef self_type = NULL;
        LLVMClassTypeEntry *enum_entry = llvm_lookup_class(ctx, enum_name);
        if (has_data) {
            if (enum_entry == NULL || enum_entry->struct_type == NULL) {
                llvm_set_error(ctx,
                    "LLVM payload enum method self type requires registered enum metadata for '%s.%s'",
                    enum_name,
                    method_name != NULL ? method_name : "<anonymous>");
                return;
            }
            self_type = enum_entry->struct_type;
        } else if (llvm_enum_type_exists(ctx, enum_name)) {
            self_type = ctx->type_i32;
        } else {
            llvm_set_error(ctx,
                "LLVM enum method self type requires enum metadata for '%s.%s'",
                enum_name,
                method_name != NULL ? method_name : "<anonymous>");
            return;
        }
        /* Param-type buffer is consumed by LLVMFunctionType (copies). */
        LLVMTypeRef *param_types = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error(ctx,
                "LLVM enum method parameter allocation failed for '%s.%s'",
                enum_name,
                method_name != NULL ? method_name : "<anonymous>");
            return;
        }
        param_types[0] = self_type;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            const char *param_type_name =
                llvm_mir_decl_method_param_type_name(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            if (param_type_name != NULL) {
                param_types[pidx++] =
                    pergyra_type_to_llvm(ctx, param_type_name);
            } else {
                param_types[pidx++] = llvm_register_required_ast_type(
                    ctx, method, p != NULL ? p->type : NULL,
                    "enum method parameter");
            }
            if (ctx->has_error || param_types[pidx - 1] == NULL)
                return;
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        if (!llvm_register_join_name(ctx, full_name, sizeof(full_name),
                enum_name, "_", method_name, "enum method")) {
            return;
        }
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

    const char *cls_name = llvm_decl_node_name(stmt);
    if (cls_name == NULL || llvm_lookup_class(ctx, cls_name) != NULL)
        return;
    LLVMHostedFieldView field_view =
        llvm_hosted_class_field_view_from_decl(ctx, cls_name, stmt);
    if (llvm_hosted_field_view_missing_mir_metadata(&field_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field declaration metadata for '%s'",
            cls_name);
        return;
    }
    size_t fc = field_view.count;
    /* Field-type buffer: consumed by LLVMStructSetBody (copies) and read
     * once for llvm_class_add_field below; never retained. */
    LLVMTypeRef *field_types = pgy_arena_calloc(&ctx->scratch,
        (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
    if (field_types == NULL) {
        llvm_set_error(ctx,
            "LLVM class field allocation failed for '%s'",
            cls_name);
        return;
    }
    for (size_t j = 0; j < fc; j++) {
        ASTNode *field_type = llvm_hosted_field_view_type(&field_view, j);
        field_types[j] = llvm_register_required_ast_type(
            ctx, stmt, field_type, "class field");
        if (ctx->has_error || field_types[j] == NULL)
            return;
    }

    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context, cls_name);
    LLVMStructSetBody(struct_ty, field_types, (unsigned)fc, 0);

    NominalDeclKind nominal_kind = ast_class_nominal_kind(stmt);
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
            const char *field_name =
                llvm_hosted_field_view_name(&field_view, j);
            if (field_name == NULL)
                continue;
            llvm_class_add_field(entry, field_name, field_types[j], (int)j);
        }
    }

    /* field_types is ctx->scratch-owned. */

    LLVMHostedMethodView class_method_view =
        llvm_hosted_method_view_from_decl(ctx, cls_name, stmt);
    if (llvm_hosted_method_view_missing_mir_metadata(&class_method_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class method declaration metadata");
        return;
    }
    if (!llvm_require_hosted_method_view_rows(
            ctx,
            &class_method_view,
            "MIR-only LLVM path has invalid method declaration metadata row for class '%s'",
            cls_name != NULL ? cls_name : "(anonymous-class)")) {
        return;
    }

    if (!class_method_view.uses_mir_metadata)
        return;
    for (size_t j = 0; j < class_method_view.count; j++) {
        const MIRDeclMethod *method_meta =
            llvm_hosted_method_view_metadata(&class_method_view, j);
        ASTNode *method = llvm_mir_decl_method_source_ast(method_meta);
        const char *method_name = NULL;
        size_t pc = 0;
        const char *return_type_name = NULL;
        ASTNode *return_type = NULL;
        bool method_is_action = false;

        method_name = llvm_mir_decl_method_name(method_meta);
        pc = llvm_mir_decl_method_param_count(method_meta);
        return_type_name = llvm_mir_decl_method_return_type_name(method_meta);
        return_type = llvm_mir_decl_method_return_type(method_meta);
        method_is_action = llvm_mir_decl_method_is_action_like(method_meta);
        if (method_name == NULL)
            continue;
        if (!llvm_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                cls_name,
                method_name,
                LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only LLVM path missing class method registry return type-name metadata for '%s.%s'",
                "MIR-only LLVM path missing class method registry parameter type-name metadata for '%s.%s'")) {
            return;
        }

        LLVMTypeRef ret_type = ctx->type_void;
        if (return_type_name != NULL)
            ret_type = pergyra_type_to_llvm(ctx, return_type_name);
        else if (return_type != NULL) {
            ret_type = ast_type_to_llvm(ctx, return_type);
        }
        if (ctx->has_error || ret_type == NULL)
            return;

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
        if (param_types == NULL) {
            llvm_set_error(ctx,
                "LLVM class method parameter allocation failed for '%s.%s'",
                cls_name,
                method_name != NULL ? method_name : "<anonymous>");
            return;
        }
        param_types[0] = is_pointer_self_host ? LLVMPointerType(struct_ty, 0) : struct_ty;
        size_t pidx = 1;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = llvm_mir_decl_method_param(method_meta, k);
            const char *param_type_name =
                llvm_mir_decl_method_param_type_name(method_meta, k);
            if (llvm_param_is_implicit_self(p))
                continue;
            if (param_type_name != NULL) {
                param_types[pidx] =
                    pergyra_type_to_llvm(ctx, param_type_name);
            } else {
                param_types[pidx] = llvm_register_required_ast_type(
                    ctx, method, p != NULL ? p->type : NULL,
                    "class method parameter");
            }
            if (ctx->has_error || param_types[pidx] == NULL)
                return;
            /* Match the body emit (llvm_emit_func_from_mir): a subject-typed
             * parameter is passed by pointer. Without this the registered
             * forward signature drifts from the emitted body signature. */
            if (param_type_name != NULL
                ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                : (p != NULL && p->type != NULL
                    && ast_type_name(p->type) != NULL
                    && llvm_type_name_uses_pointer_self(ctx,
                        ast_type_name(p->type)))) {
                param_types[pidx] = LLVMPointerType(param_types[pidx], 0);
            }
            pidx++;
        }

        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types, (unsigned)(user_pc + 1), 0);
        char full_name[256];
        if (!llvm_register_join_name(ctx, full_name, sizeof(full_name),
                cls_name, "_", method_name, "class method")) {
            return;
        }
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

        size_t extern_decl_count = 0;
        (void)ast_extern_block_declarations(stmt, &extern_decl_count);
        for (size_t j = 0; j < extern_decl_count; j++) {
            ASTNode *decl = ast_extern_block_declaration(stmt, j);
            if (decl == NULL || decl->type != AST_FUNC_DECL)
                continue;

            const char *fname = ast_declaration_name(decl);
            if (fname == NULL)
                continue;
            if (llvm_lookup_function(ctx, fname) != NULL)
                continue;

            LLVMTypeRef ret = ctx->type_void;
            if (ast_func_return_type(decl) != NULL)
                ret = ast_type_to_llvm(ctx, ast_func_return_type(decl));
            if (ctx->has_error || ret == NULL)
                return;

            size_t pc = ast_func_param_count(decl);
            /* Extern param-type buffer: consumed by LLVMFunctionType. */
            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (pc > 0 ? pc : 1) * sizeof(LLVMTypeRef));
            if (ptypes == NULL) {
                llvm_set_error(ctx,
                    "LLVM extern parameter allocation failed for '%s'",
                    fname);
                return;
            }
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = ast_func_param(decl, k);
                ptypes[k] = llvm_register_required_ast_type(
                    ctx, decl, p != NULL ? p->type : NULL, "extern parameter");
                if (ctx->has_error || ptypes[k] == NULL)
                    return;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)pc, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            /* ptypes is ctx->scratch-owned. */
        }
    }
}

#endif
