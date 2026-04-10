static LLVMValueRef
llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx, LLVMTypeRef *out_field_type)
{
    ASTNode *obj_node;
    const char *field_name;
    const char *class_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr = NULL;
    int field_idx;

    if (out_field_type != NULL)
        *out_field_type = NULL;
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;

    obj_node = node->data.member.object;
    field_name = node->data.member.name;
    if (obj_node == NULL || field_name == NULL)
        return NULL;

    class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return NULL;

    cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return NULL;

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = obj_node->data.identifier.name;
        base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
        if (base_ptr == NULL)
            return NULL;
    } else if (obj_node->type == AST_MEMBER_ACCESS) {
        base_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
        if (base_ptr == NULL)
            return NULL;
    } else {
        return NULL;
    }

    field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return NULL;

    if (out_field_type != NULL)
        *out_field_type = cls->fields[field_idx].field_type;
    return LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.assignment.target == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (node->data.assignment.target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = node->data.assignment.target->data.array_access.array;
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *name = array_node->data.identifier.name;
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            LLVMValueRef idx = llvm_emit_expression(
                node->data.assignment.target->data.array_access.index, ctx);
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (arr_var != NULL && entry != NULL && idx != NULL && val != NULL) {
                LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type,
                    arr_var->alloca, llvm_tmp_name(ctx));
                LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type,
                    data_ptr, &idx, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (node->data.assignment.target->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef gep = llvm_emit_member_lvalue_ptr(
            node->data.assignment.target, ctx, &field_type);
        LLVMValueRef val;
        if (gep == NULL || field_type == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(val) != field_type) {
            if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_f32 || LLVMTypeOf(val) == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_f32 || field_type == ctx->type_f64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(field_type) > LLVMGetIntTypeWidth(LLVMTypeOf(val)))
                    ? LLVMBuildSExt(ctx->builder, val, field_type, llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            }
        }
        LLVMBuildStore(ctx->builder, val, gep);
        return val;
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL && ctx->current_class_name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (cls != NULL && self_var != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
                LLVMValueRef base_ptr;
                LLVMValueRef gep;
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                base_ptr = self_var->alloca;
                if (self_var->type == LLVMPointerType(cls->struct_type, 0))
                    base_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                        self_var->alloca, llvm_tmp_name(ctx));
                gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
    }
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    {
        const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
        if (slot_inner != NULL) {
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", slot_inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef args[] = { var->alloca, val };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            } else {
                if (llvm_lookup_slot_is_secure(ctx, name))
                    llvm_direct_secure_slot_write(ctx, var, val);
                else
                    llvm_direct_slot_write(ctx, var, val);
            }
            return val;
        }
    }

    {
        LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMBuildStore(ctx->builder, val, var->alloca);
        return val;
    }
}

static LLVMValueRef
llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.member.object;
    const char *field_name = node->data.member.name;

    if (obj_node == NULL || field_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (llvm_is_upper_ident(obj_node)) {
        LLVMEnumVariantEntry *variant =
            llvm_lookup_enum_variant_qualified(ctx,
                obj_node->data.identifier.name, field_name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32,
                (unsigned long long)variant->value, 0);
    }

    {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *cls;
        int field_idx;

        if (class_name == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        cls = llvm_lookup_class(ctx, class_name);
        if (cls == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        field_idx = llvm_class_field_index(cls, field_name);
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            LLVMProjectionBorrowEntry *projection_borrow =
                llvm_lookup_projection_borrow(ctx, var_name);
            if (projection_borrow != NULL) {
                LLVMClassTypeEntry *source_cls;
                ASTNode *source_decl;
                LLVMVarEntry *source_var;
                const char *source_class_name = llvm_lookup_var_class(ctx,
                    projection_borrow->source_name);
                if (source_class_name == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                source_cls = llvm_lookup_class(ctx, source_class_name);
                source_decl = llvm_find_projection_nominal_decl(ctx, source_class_name);
                source_var = llvm_scope_lookup(ctx, projection_borrow->source_name);
                if (source_cls == NULL || source_decl == NULL || source_var == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                {
                    LLVMValueRef source_base = source_var->alloca;
                    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
                        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
                            source_var->alloca, llvm_tmp_name(ctx));
                    }
                    return llvm_load_projection_path_value(ctx, source_decl, source_cls,
                        source_base, field_name);
                }
            }
            {
                LLVMValueRef base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
                if (base_ptr != NULL) {
                    LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                        cls->struct_type, base_ptr, (unsigned)field_idx,
                        llvm_tmp_name(ctx));
                    LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                    return LLVMBuildLoad2(ctx->builder, field_type, gep,
                        llvm_tmp_name(ctx));
                }
            }
        }

        {
            LLVMValueRef obj_val = llvm_emit_expression(obj_node, ctx);
            if (obj_val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);

            if (LLVMTypeOf(obj_val) == LLVMPointerType(cls->struct_type, 0)) {
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, obj_val, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }

            if (LLVMTypeOf(obj_val) == cls->struct_type) {
                return LLVMBuildExtractValue(ctx->builder, obj_val,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
            }
        }
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}
