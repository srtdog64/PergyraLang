#ifndef PGY_LLVM_EXPR_HOST_SPAWN_LITERAL_HELPERS_H
#define PGY_LLVM_EXPR_HOST_SPAWN_LITERAL_HELPERS_H

static LLVMValueRef
llvm_emit_projection_from_binding(LLVMGenCtx *ctx,
                                  const char *target_class_name,
                                  const char *source_name)
{
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    ASTNode *source_decl;
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (ctx == NULL || target_class_name == NULL || source_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    target_cls = llvm_lookup_class(ctx, target_class_name);
    source_var = llvm_scope_lookup(ctx, source_name);
    source_class_name = llvm_lookup_var_class(ctx, source_name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL || source_decl == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMGetUndef(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *field = &target_cls->fields[i];
        LLVMValueRef field_val = llvm_load_projection_path_value(ctx, source_decl,
            source_cls, source_base, field->field_name);
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_val,
            (unsigned)field->index, llvm_tmp_name(ctx));
    }
    return projected;
}

#endif /* PGY_LLVM_EXPR_HOST_SPAWN_LITERAL_HELPERS_H */
