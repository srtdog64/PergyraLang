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
    if (entry == NULL || !entry->is_subject)
        return NULL;

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}

static ASTNode *
llvm_find_function_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < ctx->hir->function_count; i++) {
        ASTNode *stmt = ctx->hir->functions[i];
        if (stmt != NULL && stmt->type == AST_FUNC_DECL
            && stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, name) == 0) {
            return stmt;
        }
    }
    return llvm_lookup_generic_template(ctx, name);
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

    args = calloc(emitted_count > 0 ? emitted_count : 1, sizeof(LLVMValueRef));
    if (args == NULL)
        return NULL;

    unsigned arg_idx = 0;
    unsigned emitted_idx = 0;
    for (size_t i = 0; i < decl->data.func_decl.param_count && arg_idx < argc; i++) {
        bool is_secure = false;
        FuncParam *p = decl->data.func_decl.params[i];
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        ASTNode *arg_node = arg_nodes[arg_idx++];

        if (inner != NULL && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *source_name = arg_node->data.identifier.name;
            LLVMVarEntry *slot_var = llvm_scope_lookup(ctx, source_name);
            args[emitted_idx++] = slot_var != NULL
                ? slot_var->alloca
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

static bool
llvm_is_upper_ident(ASTNode *node)
{
    if (node == NULL || node->type != AST_IDENTIFIER
        || node->data.identifier.name == NULL
        || node->data.identifier.name[0] == '\0')
        return false;

    return node->data.identifier.name[0] >= 'A'
        && node->data.identifier.name[0] <= 'Z';
}

static const char *
llvm_call_arg_device_inner(LLVMGenCtx *ctx, ASTNode *node)
{
    if (node != NULL && node->type == AST_IDENTIFIER)
        return llvm_lookup_device_slot_inner(ctx, node->data.identifier.name);
    return NULL;
}

static LLVMValueRef
llvm_array_data_ptr(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 0, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_array_length_i64(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 1, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_build_option_value(LLVMGenCtx *ctx, LLVMTypeRef inner_ty,
                        LLVMValueRef has_value, LLVMValueRef value)
{
    LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
        (LLVMTypeRef[]){ ctx->type_i32, inner_ty }, 2, 0);
    LLVMValueRef tag = LLVMBuildSelect(ctx->builder, has_value,
        LLVMConstInt(ctx->type_i32, 0, 0),
        LLVMConstInt(ctx->type_i32, 1, 0),
        llvm_tmp_name(ctx));
    LLVMValueRef option = LLVMGetUndef(option_ty);
    option = LLVMBuildInsertValue(ctx->builder, option, tag, 0, llvm_tmp_name(ctx));
    option = LLVMBuildInsertValue(ctx->builder, option, value, 1, llvm_tmp_name(ctx));
    return option;
}

static ASTNode *
llvm_find_projection_nominal_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, name) == 0) {
            return stmt;
        }
        if (stmt->type == AST_ACTOR_DECL
            && stmt->data.actor_decl.name != NULL
            && strcmp(stmt->data.actor_decl.name, name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static size_t
llvm_projection_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    if (decl->type == AST_ACTOR_DECL)
        return decl->data.actor_decl.field_count;
    return 0;
}

static ClassField *
llvm_projection_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    if (decl->type == AST_ACTOR_DECL) {
        if (index < decl->data.actor_decl.field_count)
            return decl->data.actor_decl.fields[index];
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

    field_count = llvm_projection_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at(source_decl, i);
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

        vessel_decl = llvm_find_projection_nominal_decl(ctx, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefix_len = strlen(field->name) + strlen(nested_path) + 2;
        prefixed_path = malloc(prefix_len);
        if (prefixed_path != NULL)
            snprintf(prefixed_path, prefix_len, "%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
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
        free(path);
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
            free(path);
            return field_value;
        }

        for (size_t i = 0; i < llvm_projection_field_count(current_decl); i++) {
            ClassField *field = llvm_projection_field_at(current_decl, i);
            if (field == NULL || field->name == NULL
                || strcmp(field->name, segment) != 0
                || field->type == NULL || field->type->type != AST_TYPE
                || field->type->data.type.name == NULL) {
                continue;
            }
            current_decl = llvm_find_projection_nominal_decl(ctx, field->type->data.type.name);
            current_cls = llvm_lookup_class(ctx, field->type->data.type.name);
            current_ptr = field_ptr;
            break;
        }
        cursor = dot + 1;
    }

    free(path);
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_subject_projection(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    ASTNode *source_decl;
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (node == NULL || node->data.call.arg_count != 2)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    target_arg = node->data.call.arguments[0];
    source_arg = node->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL
        || source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_arg->data.identifier.name == NULL) {
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    target_cls = llvm_lookup_class(ctx, target_arg->data.identifier.name);
    source_var = llvm_scope_lookup(ctx, source_arg->data.identifier.name);
    source_class_name = llvm_lookup_var_class(ctx, source_arg->data.identifier.name);
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

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        field_value = llvm_load_projection_path_value(ctx, source_decl, source_cls,
            source_base, target_field->field_name);
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
}

static ASTNode *
llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type, const char *name)
{
    if (ctx == NULL || ctx->hir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL || stmt->type != decl_type)
            continue;
        if (decl_type == AST_RELATION_DECL
            && stmt->data.relation_decl.name != NULL
            && strcmp(stmt->data.relation_decl.name, name) == 0)
            return stmt;
        if (decl_type == AST_EFFECT_DECL
            && stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, name) == 0)
            return stmt;
        if (decl_type == AST_ZONE_DECL
            && stmt->data.zone_decl.name != NULL
            && strcmp(stmt->data.zone_decl.name, name) == 0)
            return stmt;
        if (decl_type == AST_WORLD_DECL
            && stmt->data.world_decl.name != NULL
            && strcmp(stmt->data.world_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
llvm_find_nominal_host_method_decl(LLVMGenCtx *ctx, const char *host_type_name,
                                   const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL, host_type_name);
    if (decl == NULL)
        decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, host_type_name);
    if (decl == NULL)
        decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL, host_type_name);
    if (decl == NULL)
        decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, host_type_name);
    if (decl == NULL)
        decl = llvm_find_projection_nominal_decl(ctx, host_type_name);

    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        methods = decl->data.class_decl.methods;
        method_count = decl->data.class_decl.method_count;
        break;
    case AST_ACTOR_DECL:
        methods = decl->data.actor_decl.methods;
        method_count = decl->data.actor_decl.method_count;
        break;
    case AST_RELATION_DECL:
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
        break;
    case AST_EFFECT_DECL:
        methods = decl->data.effect_decl.methods;
        method_count = decl->data.effect_decl.method_count;
        break;
    case AST_ZONE_DECL:
        methods = decl->data.zone_decl.methods;
        method_count = decl->data.zone_decl.method_count;
        break;
    case AST_WORLD_DECL:
        methods = decl->data.world_decl.methods;
        method_count = decl->data.world_decl.method_count;
        break;
    default:
        return NULL;
    }

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static ASTNode *
llvm_find_zone_state_decl(LLVMGenCtx *ctx, ASTNode *zone_decl, const char *state_name)
{
    (void)ctx;
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_world_state_decl(LLVMGenCtx *ctx, ASTNode *world_decl, const char *state_name)
{
    (void)ctx;
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;
    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }
    return NULL;
}

static ASTNode *
llvm_resolve_world_zone_decl(LLVMGenCtx *ctx, ASTNode *world_decl, const char *slot_name)
{
    ASTNode *zone_slot = llvm_find_world_zone_slot_decl(world_decl, slot_name);
    if (ctx == NULL || zone_slot == NULL || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;
    return llvm_find_named_domain_decl(ctx, AST_ZONE_DECL, zone_slot->data.world_zone.zone_type);
}

static ASTNode *
llvm_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_zone_layer_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static bool
llvm_world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return false;
    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return true;
        }
    }
    return false;
}

static LLVMClassTypeEntry *
llvm_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef ty);

static bool
llvm_nominal_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    if (ctx->hir == NULL)
        return false;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_CLASS_DECL:
            if (stmt->data.class_decl.name != NULL
                && strcmp(stmt->data.class_decl.name, type_name) == 0
                && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
                return true;
            break;
        case AST_PARTY_DECL:
            if (stmt->data.party_decl.name != NULL
                && strcmp(stmt->data.party_decl.name, type_name) == 0)
                return true;
            break;
        case AST_SYSTEMIC_DECL:
            if (stmt->data.systemic_decl.name != NULL
                && strcmp(stmt->data.systemic_decl.name, type_name) == 0)
                return true;
            break;
        case AST_WORLD_DECL:
            if (stmt->data.world_decl.name != NULL
                && strcmp(stmt->data.world_decl.name, type_name) == 0)
                return true;
            break;
        case AST_RELATION_DECL:
            if (stmt->data.relation_decl.name != NULL
                && strcmp(stmt->data.relation_decl.name, type_name) == 0)
                return true;
            break;
        case AST_EFFECT_DECL:
            if (stmt->data.effect_decl.name != NULL
                && strcmp(stmt->data.effect_decl.name, type_name) == 0)
                return true;
            break;
        case AST_ZONE_DECL:
            if (stmt->data.zone_decl.name != NULL
                && strcmp(stmt->data.zone_decl.name, type_name) == 0)
                return true;
            break;
        default:
            break;
        }
    }

    return false;
}

static const char *
llvm_current_host_class_name(LLVMGenCtx *ctx)
{
    return ctx != NULL ? ctx->current_class_name : NULL;
}

static const char *
llvm_current_field_class_name(LLVMGenCtx *ctx, const char *field_name)
{
    LLVMClassTypeEntry *parent_cls;
    LLVMClassTypeEntry *field_cls;
    int field_idx;
    const char *host_name;

    host_name = llvm_current_host_class_name(ctx);
    if (ctx == NULL || host_name == NULL || field_name == NULL)
        return NULL;

    parent_cls = llvm_lookup_class(ctx, host_name);
    if (parent_cls == NULL)
        return NULL;

    field_idx = llvm_class_field_index(parent_cls, field_name);
    if (field_idx < 0)
        return NULL;

    field_cls = llvm_lookup_class_by_type(ctx, parent_cls->fields[field_idx].field_type);
    return field_cls != NULL ? field_cls->class_name : NULL;
}

static LLVMValueRef
llvm_current_self_base_ptr(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL || cls == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    if (llvm_nominal_uses_pointer_self(ctx, cls->class_name))
        return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
            llvm_tmp_name(ctx));
    return self_var->alloca;
}

static LLVMValueRef
llvm_identifier_base_ptr(LLVMGenCtx *ctx, const char *name, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *var;

    if (ctx == NULL || name == NULL || cls == NULL)
        return NULL;

    var = llvm_scope_lookup(ctx, name);
    if (var != NULL) {
        LLVMValueRef base_ptr = var->alloca;
        if (var->type == LLVMPointerType(cls->struct_type, 0)) {
            base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                llvm_tmp_name(ctx));
        }
        return base_ptr;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        int parent_field_idx;
        LLVMValueRef self_ptr;
        if (parent_cls == NULL)
            return NULL;
        parent_field_idx = llvm_class_field_index(parent_cls, name);
        if (parent_field_idx < 0)
            return NULL;
        self_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
        if (self_ptr == NULL)
            return NULL;
        return LLVMBuildStructGEP2(ctx->builder, parent_cls->struct_type, self_ptr,
            (unsigned)parent_field_idx, llvm_tmp_name(ctx));
    }

    return NULL;
}

static ASTNode *
llvm_current_host_method_decl(LLVMGenCtx *ctx, const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || method_name == NULL || ctx->current_class_name == NULL
        || ctx->hir == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_CLASS_DECL:
            if (stmt->data.class_decl.name != NULL
                && strcmp(stmt->data.class_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.class_decl.methods;
                method_count = stmt->data.class_decl.method_count;
            }
            break;
        case AST_ACTOR_DECL:
            if (stmt->data.actor_decl.name != NULL
                && strcmp(stmt->data.actor_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.actor_decl.methods;
                method_count = stmt->data.actor_decl.method_count;
            }
            break;
        case AST_RELATION_DECL:
            if (stmt->data.relation_decl.name != NULL
                && strcmp(stmt->data.relation_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.relation_decl.methods;
                method_count = stmt->data.relation_decl.method_count;
            }
            break;
        case AST_EFFECT_DECL:
            if (stmt->data.effect_decl.name != NULL
                && strcmp(stmt->data.effect_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.effect_decl.methods;
                method_count = stmt->data.effect_decl.method_count;
            }
            break;
        case AST_ZONE_DECL:
            if (stmt->data.zone_decl.name != NULL
                && strcmp(stmt->data.zone_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.zone_decl.methods;
                method_count = stmt->data.zone_decl.method_count;
            }
            break;
        case AST_WORLD_DECL:
            if (stmt->data.world_decl.name != NULL
                && strcmp(stmt->data.world_decl.name, ctx->current_class_name) == 0) {
                decl = stmt;
                methods = stmt->data.world_decl.methods;
                method_count = stmt->data.world_decl.method_count;
            }
            break;
        default:
            break;
        }
        if (decl != NULL)
            break;
    }

    for (size_t i = 0; methods != NULL && i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static LLVMValueRef
llvm_current_self_call_arg(LLVMGenCtx *ctx)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
        llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_await_task_handle(LLVMGenCtx *ctx, LLVMValueRef task, const char *inner,
                       bool is_remote)
{
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    if (await_fn == NULL || task == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMValueRef args[] = { task };
    LLVMValueRef raw = LLVMBuildCall2(ctx->builder, await_fn->fn_type,
        await_fn->fn, args, 1, llvm_tmp_name(ctx));

    if (inner == NULL || strcmp(inner, "Void") == 0) {
        if (!is_remote)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
    LLVMValueRef value;
    if (strcmp(inner, "String") == 0) {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(ctx->type_i8ptr, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            typed_ptr, llvm_tmp_name(ctx));
    } else {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(inner_ty, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, inner_ty, typed_ptr, llvm_tmp_name(ctx));
    }

    if (!is_remote)
        return value;

    LLVMTypeRef result_fields[] = { ctx->type_i32, inner_ty, ctx->type_i8ptr };
    LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context, result_fields, 3, 0);
    LLVMValueRef result = LLVMGetUndef(result_ty);
    result = LLVMBuildInsertValue(ctx->builder, result,
        LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
    result = LLVMBuildInsertValue(ctx->builder, result,
        value, 1, llvm_tmp_name(ctx));
    result = LLVMBuildInsertValue(ctx->builder, result,
        LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
    return result;
}

static LLVMValueRef
llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                             ASTNode **arg_nodes, size_t argc)
{
    LLVMValueRef *args = NULL;

    if (argc > 0) {
        args = calloc(argc, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(arg_nodes[i], ctx);
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, (unsigned)argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, (unsigned)argc, llvm_tmp_name(ctx));
    }

    free(args);
    return result;
}

static LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, const char *callee_name,
                          LLVMValueRef *args, size_t argc)
{
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    if (generic_ast == NULL)
        return llvm_lookup_function(ctx, callee_name);

    char mangled[256];
    snprintf(mangled, sizeof(mangled), "%s", callee_name);
    for (size_t i = 0; i < argc; i++) {
        LLVMTypeRef at = (args != NULL && args[i] != NULL)
            ? LLVMTypeOf(args[i]) : ctx->type_i32;
        const char *suf = llvm_type_to_suffix(ctx, at);
        llvm_append_mangled_suffix(mangled, sizeof(mangled), suf);
    }

    if (!llvm_mono_already_emitted(ctx, mangled)) {
        llvm_register_mono(ctx, mangled);

        GenericParams *gp = generic_ast->data.func_decl.generic_params;
        int saved_subst = ctx->type_subst_count;
        ctx->type_subst_count = 0;
        for (size_t gi = 0; gi < gp->count && gi < 8; gi++) {
            LLVMTypeRef concrete = (gi < argc && args != NULL && args[gi] != NULL)
                ? LLVMTypeOf(args[gi]) : ctx->type_i32;
            ctx->type_subst[ctx->type_subst_count].param_name = gp->params[gi]->name;
            ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
            ctx->type_subst[ctx->type_subst_count].type_name = llvm_type_to_suffix(ctx, concrete);
            ctx->type_subst_count++;
        }

        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        LLVMTypeRef ret = ctx->type_void;
        if (generic_ast->data.func_decl.return_type != NULL)
            ret = ast_type_to_llvm(ctx, generic_ast->data.func_decl.return_type);

        size_t pc = generic_ast->data.func_decl.param_count;
        LLVMTypeRef *ptypes = calloc((pc * 2) > 0 ? (pc * 2) : 1, sizeof(LLVMTypeRef));
        size_t real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
                if (inner != NULL) {
                    LLVMTypeRef slot_ty = ast_type_to_llvm(ctx, p->type);
                    ptypes[real_pc++] = LLVMPointerType(slot_ty, 0);
                    if (is_secure)
                        ptypes[real_pc++] = llvm_secure_token_type(ctx, inner);
                } else {
                    ptypes[real_pc++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
                }
            }
        }
        LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
        LLVMValueRef mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        free(ptypes);

        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
                if (inner != NULL) {
                    LLVMValueRef slot_ptr = LLVMGetParam(mono_fn, (unsigned)real_pc++);
                    llvm_scope_declare(ctx, p->name, slot_ptr, pt);
                    llvm_register_slot_var(ctx, p->name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                        char token_name[256];
                        LLVMValueRef token_alloca;
                        snprintf(token_name, sizeof(token_name), "%s_token", p->name);
                        token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
                        LLVMBuildStore(ctx->builder, LLVMGetParam(mono_fn, (unsigned)real_pc++), token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
                    }
                } else {
                    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder, LLVMGetParam(mono_fn, (unsigned)real_pc), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    if (p->type != NULL && p->type->type == AST_TYPE
                        && p->type->data.type.name != NULL
                        && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
                        llvm_register_var_class(ctx, p->name, p->type->data.type.name);
                    }
                    real_pc++;
                }
            }
        }

        if (generic_ast->data.func_decl.body != NULL)
            llvm_emit_block(generic_ast->data.func_decl.body, ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0));
        }

        llvm_scope_pop(ctx);
        ctx->type_subst_count = saved_subst;
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    }

    return llvm_lookup_function(ctx, mangled);
}

static LLVMValueRef
llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *callee_name = NULL;
    size_t argc = 0;
    LLVMValueRef *args = NULL;
    LLVMFuncEntry *callee_entry = NULL;
    LLVMFuncEntry *spawn_fn = NULL;
    LLVMFuncEntry *malloc_fn = NULL;
    LLVMFuncEntry *free_fn = NULL;
    int wrapper_id = ++ctx->tmp_counter;

    if (target == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        argc = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (argc > 0) {
        args = calloc(argc, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(call->data.call.arguments[i], ctx);
    }

    callee_entry = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    spawn_fn = llvm_lookup_function(ctx,
        node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking_export"
            : "pgy_async_spawn_export");
    malloc_fn = llvm_lookup_function(ctx, "malloc");
    free_fn = llvm_lookup_function(ctx, "free");
    if (spawn_fn == NULL || malloc_fn == NULL || free_fn == NULL) {
        free(args);
        return LLVMConstNull(ctx->type_task_handle);
    }
    if (callee_entry == NULL) {
        free(args);
        return LLVMConstNull(ctx->type_task_handle);
    }

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    char wrapper_name[96];
    snprintf(wrapper_name, sizeof(wrapper_name), "_pgy_spawn_expr_%d", wrapper_id);
    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    LLVMValueRef wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);

    LLVMTypeRef arg_struct_type = NULL;
    if (argc > 0) {
        LLVMTypeRef *field_types = calloc(argc, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < argc; i++)
            field_types[i] = LLVMTypeOf(args[i]);
        arg_struct_type = LLVMStructTypeInContext(ctx->context, field_types, (unsigned)argc, 0);
        free(field_types);
    }

    ctx->current_function = wrapper_fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    llvm_scope_push(ctx);

    LLVMValueRef loaded_args_storage[16];
    LLVMValueRef *loaded_args = loaded_args_storage;
    if (argc > 16)
        loaded_args = calloc(argc, sizeof(LLVMValueRef));

    LLVMValueRef raw_arg = LLVMGetParam(wrapper_fn, 0);
    if (argc > 0) {
        LLVMValueRef typed_ctx = LLVMBuildBitCast(ctx->builder, raw_arg,
            LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
        for (size_t i = 0; i < argc; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, arg_struct_type,
                typed_ctx, (unsigned)i, llvm_tmp_name(ctx));
            loaded_args[i] = LLVMBuildLoad2(ctx->builder, LLVMTypeOf(args[i]), gep, llvm_tmp_name(ctx));
        }
        LLVMValueRef free_args[] = { raw_arg };
        LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn, free_args, 1, "");
    }

    LLVMValueRef call_result = NULL;
    if (callee_entry->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, callee_entry->fn_type, callee_entry->fn,
            loaded_args, (unsigned)argc, "");
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    } else {
        call_result = LLVMBuildCall2(ctx->builder, callee_entry->fn_type, callee_entry->fn,
            loaded_args, (unsigned)argc, llvm_tmp_name(ctx));
        LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
        uint64_t size = LLVMABISizeOfType(td, callee_entry->ret_type);
        LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
        LLVMValueRef raw_result = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
            malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef typed_result = LLVMBuildBitCast(ctx->builder, raw_result,
            LLVMPointerType(callee_entry->ret_type, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, call_result, typed_result);
        LLVMBuildRet(ctx->builder, raw_result);
    }

    llvm_scope_pop(ctx);
    if (loaded_args != loaded_args_storage)
        free(loaded_args);

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMValueRef raw_spawn_arg = LLVMConstNull(ctx->type_i8ptr);
    if (argc > 0) {
        LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
        uint64_t size = LLVMABISizeOfType(td, arg_struct_type);
        LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
        raw_spawn_arg = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
            malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef typed_arg = LLVMBuildBitCast(ctx->builder, raw_spawn_arg,
            LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
        for (size_t i = 0; i < argc; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, arg_struct_type,
                typed_arg, (unsigned)i, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, args[i], gep);
        }
    }

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, wrapper_fn, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = { fn_ptr, raw_spawn_arg };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 2, llvm_tmp_name(ctx));

    free(args);
    return handle;
}

static const char *
llvm_operator_overload_suffix(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "mod";
    case TOKEN_EQUAL: return "eq";
    case TOKEN_NOT_EQUAL: return "ne";
    case TOKEN_LESS: return "lt";
    case TOKEN_LESS_EQUAL: return "le";
    case TOKEN_GREATER: return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default: return NULL;
    }
}

static const char *
llvm_expr_custom_type_name(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_IDENTIFIER: {
        const char *name = node->data.identifier.name;
        const char *class_name = llvm_lookup_var_class(ctx, name);
        if (class_name != NULL)
            return class_name;
        if (strcmp(name, "self") != 0) {
            const char *host_name = llvm_current_host_class_name(ctx);
            LLVMClassTypeEntry *host_cls = host_name != NULL
                ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = host_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        {
            LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
            if (variant != NULL)
                return variant->enum_name;
        }
        return NULL;
    }
    case AST_MEMBER_ACCESS:
        if (llvm_is_upper_ident(node->data.member.object)) {
            LLVMEnumVariantEntry *variant =
                llvm_lookup_enum_variant_qualified(ctx,
                    node->data.member.object->data.identifier.name,
                    node->data.member.name);
            if (variant != NULL)
                return variant->enum_name;
        }
        {
            const char *base_name =
                llvm_expr_custom_type_name(node->data.member.object, ctx);
            LLVMClassTypeEntry *base_cls = NULL;
            if (base_name != NULL)
                base_cls = llvm_lookup_class(ctx, base_name);
            if (base_cls != NULL) {
                int field_idx = llvm_class_field_index(base_cls,
                    node->data.member.name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = base_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        return NULL;
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee = node->data.call.callee->data.identifier.name;
            if (llvm_lookup_class(ctx, callee) != NULL)
                return callee;
            {
                LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee);
                if (variant != NULL)
                    return variant->enum_name;
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

static LLVMClassTypeEntry *
llvm_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == ty)
            return &ctx->class_types[i];
    }
    return NULL;
}

static ASTNode *
llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name)
{
    if (ctx == NULL || ctx->hir == NULL || enum_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && stmt->data.enum_decl.name != NULL
            && strcmp(stmt->data.enum_decl.name, enum_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx)
{
    double val = node->data.number.value;

    /* Check if integer fits in i32 */
    if (val == (int64_t)val && val >= -2147483648.0 && val <= 2147483647.0)
        return LLVMConstInt(ctx->type_i32, (unsigned long long)(int32_t)val, 1);

    /* Check if integer fits in i64 (beyond i32 range) */
    if (val == (double)(int64_t)val
        && val >= -9.2233720368547758e+18
        && val <=  9.2233720368547758e+18)
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);

    return LLVMConstReal(ctx->type_f64, val);
}

static LLVMValueRef
llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *str = node->data.string.value;
    LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str,
                                                    llvm_tmp_name(ctx));
    return global;
}

static LLVMValueRef
llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, node->data.boolean.value ? 1 : 0, 0);
}

static LLVMValueRef
llvm_direct_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                      const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr, llvm_tmp_name(ctx));
}

static void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value);

static void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var);

static void
llvm_direct_secure_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                              LLVMValueRef value)
{
    llvm_direct_slot_write(ctx, slot_var, value);
}

static LLVMValueRef
llvm_direct_secure_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                             const char *inner)
{
    return llvm_direct_slot_read(ctx, slot_var, inner);
}

static void
llvm_direct_secure_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef token_ptr;

    llvm_direct_slot_release(ctx, slot_var);
    if (slot_var == NULL)
        return;

    token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
}

static void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value)
{
    LLVMValueRef value_ptr;
    LLVMValueRef occupied_ptr;

    if (slot_var == NULL || value == NULL)
        return;

    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 0, llvm_tmp_name(ctx));
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, value_ptr);
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        occupied_ptr);
}

static void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef occupied_ptr;

    if (slot_var == NULL)
        return;

    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
        occupied_ptr);
}

static LLVMVarEntry *
llvm_resolve_slot_target(LLVMGenCtx *ctx, ASTNode *slot_arg,
                         const char **inner_out,
                         const char **source_name_out,
                         bool *secure_out)
{
    const char *inner = "Int";
    const char *source_name = NULL;
    bool is_secure = false;

    if (slot_arg == NULL)
        return NULL;

    if (slot_arg->type == AST_IDENTIFIER) {
        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, slot_arg->data.identifier.name);
        if (view != NULL) {
            source_name = view->source_slot;
            inner = view->inner_type;
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        } else {
            source_name = slot_arg->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    } else if (slot_arg->type == AST_CALL
               && slot_arg->data.call.callee != NULL
               && slot_arg->data.call.callee->type == AST_IDENTIFIER
               && slot_arg->data.call.arg_count >= 1
               && slot_arg->data.call.arguments[0] != NULL
               && slot_arg->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee = slot_arg->data.call.callee->data.identifier.name;
        if (callee != NULL
            && (strcmp(callee, "ViewRead") == 0
                || strcmp(callee, "ViewWrite") == 0
                || strcmp(callee, "Move") == 0)) {
            source_name = slot_arg->data.call.arguments[0]->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    }

    if (inner == NULL)
        inner = "Int";
    if (inner_out != NULL)
        *inner_out = inner;
    if (source_name_out != NULL)
        *source_name_out = source_name;
    if (secure_out != NULL)
        *secure_out = is_secure;
    return source_name != NULL ? llvm_scope_lookup(ctx, source_name) : NULL;
}

static LLVMValueRef
llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.identifier.name;

    /* Slot sugar: auto-Read — call pgy_read_T(&slot) instead of loading struct */
    if (!ctx->suppress_slot_auto_read) {
        const char *inner = llvm_lookup_slot_inner(ctx, name);
        if (inner != NULL) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
            if (var != NULL) {
                bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
                char fn_name[64];
                snprintf(fn_name, sizeof(fn_name),
                    is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    if (is_secure) {
                        LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, name);
                        if (token_var != NULL) {
                            LLVMValueRef args[] = { var->alloca, token_var->alloca };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                                 args, 2, llvm_tmp_name(ctx));
                        }
                    }
                    LLVMValueRef args[] = { var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                         args, 1, llvm_tmp_name(ctx));
                }
                if (is_secure)
                    return llvm_direct_secure_slot_read(ctx, var, inner);
                return llvm_direct_slot_read(ctx, var, inner);
            }
        }
    }

    /* Look up in scope */
    LLVMVarEntry *entry = llvm_scope_lookup(ctx, name);
    if (entry != NULL)
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));

    if (ctx->current_class_name != NULL && strcmp(name, "self") != 0) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        if (cls != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, cls);
                if (base_ptr == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }
        }
    }

    /* Look up as function (for passing as value) */
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, name);
    if (fn != NULL)
        return fn->fn;

    /* Bare enum variant identifier */
    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0);
    }

    /* Unknown — default to 0 */
    return LLVMConstInt(ctx->type_i32, 0, 0);
}
