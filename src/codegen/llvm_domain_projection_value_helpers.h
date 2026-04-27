static LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   ASTNode *source_decl,
                                   ASTNode *refresh,
                                   LLVMValueRef source_ptr);

static ASTNode *
llvm_find_projection_nominal_decl_local(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

static size_t
llvm_projection_field_count_local(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
llvm_projection_field_at_local(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static int
llvm_resolve_projection_source_path_rec(LLVMGenCtx *ctx, ASTNode *source_decl,
                                        const char *field_name, unsigned depth,
                                        char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = llvm_projection_field_count_local(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at_local(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at_local(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        size_t prefix_len;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = llvm_find_projection_nominal_decl_local(
            ctx, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefix_len = strlen(field->name) + strlen(nested_path) + 2;
        prefixed_path = pgy_arena_alloc(&ctx->scratch, prefix_len);
        if (prefixed_path != NULL)
            snprintf(prefixed_path, prefix_len, "%s.%s", field->name, nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

static LLVMValueRef
llvm_load_projection_path_value(LLVMGenCtx *ctx,
                                ASTNode *source_decl,
                                LLVMClassTypeEntry *source_cls,
                                LLVMValueRef source_ptr,
                                const char *field_name)
{
    char *path = NULL;
    char *cursor;
    ASTNode *current_decl;
    LLVMClassTypeEntry *current_cls;
    LLVMValueRef current_ptr;

    if (llvm_resolve_projection_source_path_rec(ctx, source_decl, field_name, 0, &path) != 1
        || path == NULL || source_cls == NULL || source_ptr == NULL) {
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    current_decl = source_decl;
    current_cls = source_cls;
    current_ptr = source_ptr;
    cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        char *dot = strchr(cursor, '.');
        char *segment = cursor;
        int field_index;
        LLVMClassFieldInfo *field_info = NULL;
        LLVMValueRef field_ptr;

        if (dot != NULL)
            *dot = '\0';

        field_index = llvm_class_field_index(current_cls, segment);
        if (field_index < 0)
            break;
        for (int i = 0; i < current_cls->field_count; i++) {
            if (current_cls->fields[i].index == field_index) {
                field_info = &current_cls->fields[i];
                break;
            }
        }
        if (field_info == NULL || field_info->field_type == NULL)
            break;

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            LLVMValueRef field_value = LLVMBuildLoad2(ctx->builder, field_info->field_type,
                field_ptr, llvm_tmp_name(ctx));
            return field_value;
        }

        for (size_t i = 0; i < llvm_projection_field_count_local(current_decl); i++) {
            ClassField *field = llvm_projection_field_at_local(current_decl, i);
            if (field == NULL || field->name == NULL
                || strcmp(field->name, segment) != 0
                || field->type == NULL || field->type->type != AST_TYPE
                || field->type->data.type.name == NULL) {
                continue;
            }
            current_decl = llvm_find_projection_nominal_decl_local(
                ctx, field->type->data.type.name);
            current_cls = llvm_lookup_class(ctx, field->type->data.type.name);
            current_ptr = field_ptr;
            break;
        }
        cursor = dot + 1;
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   ASTNode *source_decl,
                                   ASTNode *refresh,
