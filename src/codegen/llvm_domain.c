/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — domain-specific passes (Party/Systemic/World, Ability,
 * Role, Event).  Extracted from llvm_backend.c to keep file sizes
 * manageable.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static const char *
llvm_operator_suffix(TokenType op)
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

static bool
llvm_operator_method_name_matches(TokenType op, const char *name)
{
    static const struct {
        TokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }
    return false;
}

static ASTNode *
llvm_find_role_decl(const HIRProgram *hir, const char *role_name)
{
    if (hir == NULL || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_role_operator_method(const HIRProgram *hir, ASTNode *role,
                               TokenType op, int depth)
{
    if (hir == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && llvm_operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = llvm_find_role_decl(hir, inc->data.include_stmt.role_name);
        ASTNode *method = llvm_find_role_operator_method(hir, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static ASTNode *
llvm_find_world_state_decl(ASTNode *world_decl, const char *state_name)
{
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

static void
llvm_domain_decl_parts(ASTNode *stmt,
                       const char **decl_name,
                       ASTNode ***slots,
                       size_t *slot_count,
                       ASTNode ***shared_fields,
                       size_t *shared_count,
                       ASTNode ***methods,
                       size_t *method_count,
                       ASTNode ***refreshes,
                       size_t *refresh_count)
{
    *decl_name = NULL;
    *slots = NULL;
    *slot_count = 0;
    *shared_fields = NULL;
    *shared_count = 0;
    *methods = NULL;
    *method_count = 0;
    *refreshes = NULL;
    *refresh_count = 0;

    if (stmt == NULL)
        return;

    switch (stmt->type) {
    case AST_PARTY_DECL:
        *decl_name = stmt->data.party_decl.name;
        *shared_fields = stmt->data.party_decl.shared_fields;
        *shared_count = stmt->data.party_decl.shared_count;
        *methods = stmt->data.party_decl.methods;
        *method_count = stmt->data.party_decl.method_count;
        break;
    case AST_SYSTEMIC_DECL:
        *decl_name = stmt->data.systemic_decl.name;
        *shared_fields = stmt->data.systemic_decl.shared_fields;
        *shared_count = stmt->data.systemic_decl.shared_count;
        *methods = stmt->data.systemic_decl.methods;
        *method_count = stmt->data.systemic_decl.method_count;
        break;
    case AST_WORLD_DECL:
        *decl_name = stmt->data.world_decl.name;
        *shared_fields = stmt->data.world_decl.shared_fields;
        *shared_count = stmt->data.world_decl.shared_count;
        *methods = stmt->data.world_decl.methods;
        *method_count = stmt->data.world_decl.method_count;
        break;
    case AST_RELATION_DECL:
        *decl_name = stmt->data.relation_decl.name;
        *slots = stmt->data.relation_decl.slots;
        *slot_count = stmt->data.relation_decl.slot_count;
        *shared_fields = stmt->data.relation_decl.shared_fields;
        *shared_count = stmt->data.relation_decl.shared_count;
        *methods = stmt->data.relation_decl.methods;
        *method_count = stmt->data.relation_decl.method_count;
        *refreshes = stmt->data.relation_decl.refreshes;
        *refresh_count = stmt->data.relation_decl.refresh_count;
        break;
    case AST_EFFECT_DECL:
        *decl_name = stmt->data.effect_decl.name;
        *slots = stmt->data.effect_decl.slots;
        *slot_count = stmt->data.effect_decl.slot_count;
        *shared_fields = stmt->data.effect_decl.shared_fields;
        *shared_count = stmt->data.effect_decl.shared_count;
        *methods = stmt->data.effect_decl.methods;
        *method_count = stmt->data.effect_decl.method_count;
        *refreshes = stmt->data.effect_decl.refreshes;
        *refresh_count = stmt->data.effect_decl.refresh_count;
        break;
    case AST_ZONE_DECL:
        *decl_name = stmt->data.zone_decl.name;
        *slots = stmt->data.zone_decl.slots;
        *slot_count = stmt->data.zone_decl.slot_count;
        *shared_fields = stmt->data.zone_decl.shared_fields;
        *shared_count = stmt->data.zone_decl.shared_count;
        *methods = stmt->data.zone_decl.methods;
        *method_count = stmt->data.zone_decl.method_count;
        *refreshes = stmt->data.zone_decl.refreshes;
        *refresh_count = stmt->data.zone_decl.refresh_count;
        break;
    default:
        break;
    }
}

static LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   LLVMValueRef source_ptr)
{
    LLVMValueRef projected;

    if (ctx == NULL || target_cls == NULL || source_cls == NULL || source_ptr == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        LLVMClassFieldInfo *source_field = NULL;
        int source_index;
        LLVMValueRef field_ptr;
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        source_index = llvm_class_field_index(source_cls, target_field->field_name);
        if (source_index < 0)
            continue;

        for (int j = 0; j < source_cls->field_count; j++) {
            if (source_cls->fields[j].index == source_index) {
                source_field = &source_cls->fields[j];
                break;
            }
        }
        if (source_field == NULL || source_field->field_type == NULL)
            continue;

        field_ptr = LLVMBuildStructGEP2(ctx->builder, source_cls->struct_type,
            source_ptr, (unsigned)source_index, llvm_tmp_name(ctx));
        field_value = LLVMBuildLoad2(ctx->builder, source_field->field_type,
            field_ptr, llvm_tmp_name(ctx));
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
}

static void
llvm_emit_domain_projection_sync_body(ASTNode *stmt,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx)
{
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    const char *unused_name = NULL;
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    ASTNode **unused_shared = NULL;
    size_t unused_shared_count = 0;
    ASTNode **unused_methods = NULL;
    size_t unused_method_count = 0;

    llvm_domain_decl_parts(stmt, &unused_name, &slots, &slot_count,
        &unused_shared, &unused_shared_count, &unused_methods, &unused_method_count,
        &refreshes, &refresh_count);

    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef flag_ptr;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL)
            continue;
        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            slot->data.domain_slot.slot_name);
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        flag_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), flag_ptr);
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        LLVMClassTypeEntry *target_cls;
        LLVMClassTypeEntry *source_cls;
        int target_index;
        int source_index;
        LLVMValueRef self_ptr;
        LLVMValueRef target_ptr;
        LLVMValueRef source_ptr;
        LLVMValueRef projected;
        const char *target_slot_name;
        const char *source_slot_name;
        const char *target_type_name;
        const char *source_type_name;
        ASTNode *target_slot_decl = NULL;
        ASTNode *source_slot_decl = NULL;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_slot_name = refresh->data.zone_refresh.object_slot_name;
        source_slot_name = refresh->data.zone_refresh.source_slot_name;
        if (target_slot_name == NULL || source_slot_name == NULL)
            continue;

        for (size_t j = 0; j < slot_count; j++) {
            ASTNode *slot = slots[j];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            if (slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, target_slot_name) == 0) {
                target_slot_decl = slot;
            }
            if (slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, source_slot_name) == 0) {
                source_slot_decl = slot;
            }
        }
        if (target_slot_decl == NULL || source_slot_decl == NULL
            || target_slot_decl->data.domain_slot.type == NULL
            || source_slot_decl->data.domain_slot.type == NULL
            || target_slot_decl->data.domain_slot.type->type != AST_TYPE
            || source_slot_decl->data.domain_slot.type->type != AST_TYPE)
            continue;

        target_type_name = target_slot_decl->data.domain_slot.type->data.type.name;
        source_type_name = source_slot_decl->data.domain_slot.type->data.type.name;
        if (target_type_name == NULL || source_type_name == NULL)
            continue;

        target_cls = llvm_lookup_class(ctx, target_type_name);
        source_cls = llvm_lookup_class(ctx, source_type_name);
        target_index = llvm_class_field_index(decl_cls, target_slot_name);
        source_index = llvm_class_field_index(decl_cls, source_slot_name);
        if (target_cls == NULL || source_cls == NULL
            || target_index < 0 || source_index < 0)
            continue;

        self_ptr = LLVMGetParam(sync_fn, 0);
        target_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)target_index, llvm_tmp_name(ctx));
        source_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)source_index, llvm_tmp_name(ctx));
        projected = llvm_build_domain_projection_value(ctx, target_cls,
            source_cls, source_ptr);
        LLVMBuildStore(ctx->builder, projected, target_ptr);
        {
            char field_name[256];
            int field_idx;
            LLVMValueRef flag_ptr;
            snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                target_slot_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx >= 0) {
                flag_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), flag_ptr);
            }
        }
    }
}

static void
llvm_emit_domain_projection_sync(ASTNode *stmt,
                                 const char *decl_name,
                                 LLVMClassTypeEntry *decl_cls,
                                 LLVMValueRef sync_fn,
                                 LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    const char *saved_class_name;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || decl_name == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_class_name = ctx->current_class_name;
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;
    ctx->current_class_name = decl_name;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }

    llvm_emit_domain_projection_sync_body(stmt, decl_cls, sync_fn, ctx);

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_class_name = saved_class_name;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

static void
llvm_emit_zone_sync(ASTNode *stmt, const char *decl_name,
                    LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                    LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    const char *saved_class_name;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_ZONE_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_class_name = ctx->current_class_name;
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;
    ctx->current_class_name = decl_name;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }

    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        if (state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        state_name = state->data.zone_state.state_name;
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
        }
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        snprintf(field_name, sizeof(field_name), "__layer_active_%s",
            slot->data.zone_layer_slot.slot_name);
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
    }

    llvm_emit_domain_projection_sync_body(stmt, decl_cls, sync_fn, ctx);

    for (size_t i = 0; i < stmt->data.zone_decl.apply_count; i++) {
        ASTNode *apply = stmt->data.zone_decl.applies[i];
        const char *state_name = apply != NULL ? apply->data.zone_apply.state_name : NULL;
        if (state_name == NULL && apply != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && apply->data.zone_apply.effect_slot_name != NULL
                    && apply->data.zone_apply.target_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              apply->data.zone_apply.effect_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              apply->data.zone_apply.target_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            if (apply != NULL) {
                const char *layer_name = apply->data.zone_apply.effect_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && !state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_states[i];
        if (maintain != NULL && maintain->data.zone_maintain_state.state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s",
                maintain->data.zone_maintain_state.state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.state_name != NULL
                    && strcmp(state->data.zone_state.state_name,
                              maintain->data.zone_maintain_state.state_name) == 0) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                        state->data.zone_state.layer_slot_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                    }
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.detach_count; i++) {
        ASTNode *detach = stmt->data.zone_decl.detaches[i];
        const char *state_name = detach != NULL ? detach->data.zone_detach.state_name : NULL;
        if (state_name == NULL && detach != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && detach->data.zone_detach.effect_slot_name != NULL
                    && detach->data.zone_detach.target_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              detach->data.zone_detach.effect_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              detach->data.zone_detach.target_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
            if (detach != NULL) {
                const char *layer_name = detach->data.zone_detach.effect_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && !state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.link_count; i++) {
        ASTNode *link = stmt->data.zone_decl.links[i];
        const char *state_name = link != NULL ? link->data.zone_link.state_name : NULL;
        if (state_name == NULL && link != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && state->data.zone_state.right_slot_name != NULL
                    && link->data.zone_link.relation_slot_name != NULL
                    && link->data.zone_link.left_slot_name != NULL
                    && link->data.zone_link.right_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              link->data.zone_link.relation_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              link->data.zone_link.left_slot_name) == 0
                    && strcmp(state->data.zone_state.right_slot_name,
                              link->data.zone_link.right_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            if (link != NULL) {
                const char *layer_name = link->data.zone_link.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_relations[i];
        if (maintain != NULL && maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                maintain->data.zone_maintain_relation.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
            }
        }
        if (maintain == NULL)
            continue;
        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
            ASTNode *state = stmt->data.zone_decl.states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || !state->data.zone_state.is_relation
                || state->data.zone_state.layer_slot_name == NULL
                || state->data.zone_state.left_or_target_slot_name == NULL
                || state->data.zone_state.right_slot_name == NULL
                || maintain->data.zone_maintain_relation.relation_slot_name == NULL
                || maintain->data.zone_maintain_relation.left_slot_name == NULL
                || maintain->data.zone_maintain_relation.right_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_relation.relation_slot_name) != 0
                || strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_relation.left_slot_name) != 0
                || strcmp(state->data.zone_state.right_slot_name,
                          maintain->data.zone_maintain_relation.right_slot_name) != 0)
                continue;
            state_name = state->data.zone_state.state_name;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = stmt->data.zone_decl.unlinks[i];
        const char *state_name = unlink != NULL ? unlink->data.zone_unlink.state_name : NULL;
        if (state_name == NULL && unlink != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && state->data.zone_state.right_slot_name != NULL
                    && unlink->data.zone_unlink.relation_slot_name != NULL
                    && unlink->data.zone_unlink.left_slot_name != NULL
                    && unlink->data.zone_unlink.right_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              unlink->data.zone_unlink.relation_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              unlink->data.zone_unlink.left_slot_name) == 0
                    && strcmp(state->data.zone_state.right_slot_name,
                              unlink->data.zone_unlink.right_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
            if (unlink != NULL) {
                const char *layer_name = unlink->data.zone_unlink.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                    }
                }
            }
        }
    }

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_class_name = saved_class_name;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

static void
llvm_emit_world_sync(ASTNode *stmt, const char *decl_name,
                     LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                     LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    const char *saved_class_name;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_class_name = ctx->current_class_name;
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;
    ctx->current_class_name = decl_name;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }

    for (size_t i = 0; i < stmt->data.world_decl.zone_count; i++) {
        ASTNode *zone = stmt->data.world_decl.zones[i];
        char active_field[256];
        int active_idx;
        int zone_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef active_ptr;
        if (zone == NULL || zone->type != AST_WORLD_ZONE
            || zone->data.world_zone.slot_name == NULL)
            continue;
        snprintf(active_field, sizeof(active_field), "__zone_active_%s",
            zone->data.world_zone.slot_name);
        active_idx = llvm_class_field_index(decl_cls, active_field);
        zone_idx = llvm_class_field_index(decl_cls, zone->data.world_zone.slot_name);
        self_ptr = LLVMGetParam(sync_fn, 0);
        if (active_idx >= 0) {
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
        }
        if (zone_idx >= 0 && zone->data.world_zone.zone_type != NULL) {
            LLVMClassTypeEntry *zone_cls = llvm_lookup_class(ctx, zone->data.world_zone.zone_type);
            char sync_name[256];
            LLVMFuncEntry *zone_sync;
            snprintf(sync_name, sizeof(sync_name), "%s_sync",
                zone->data.world_zone.zone_type);
            zone_sync = llvm_lookup_function(ctx, sync_name);
            if (zone_cls != NULL && zone_sync != NULL) {
                LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, zone_sync->fn_type, zone_sync->fn, args, 1, "");
            }
        }
    }

    for (size_t i = 0; i < stmt->data.world_decl.activate_count; i++) {
        ASTNode *act = stmt->data.world_decl.activations[i];
        const char *slot_name = act != NULL ? act->data.world_activate.zone_slot_name : NULL;
        if (slot_name == NULL && act != NULL && act->data.world_activate.state_name != NULL) {
            ASTNode *state = llvm_find_world_state_decl(stmt, act->data.world_activate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
        }
        if (slot_name != NULL) {
            char active_field[256];
            int active_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef active_ptr;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }
    }

    for (size_t i = 0; i < stmt->data.world_decl.maintained_zone_count; i++) {
        ASTNode *mnt = stmt->data.world_decl.maintained_zones[i];
        const char *slot_name = mnt != NULL ? mnt->data.world_maintain.zone_slot_name : NULL;
        if (slot_name == NULL && mnt != NULL && mnt->data.world_maintain.state_name != NULL) {
            ASTNode *state = llvm_find_world_state_decl(stmt, mnt->data.world_maintain.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
        }
        if (slot_name != NULL) {
            char active_field[256];
            int active_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef active_ptr;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }
    }

    for (size_t i = 0; i < stmt->data.world_decl.deactivate_count; i++) {
        ASTNode *act = stmt->data.world_decl.deactivations[i];
        const char *slot_name = act != NULL ? act->data.world_deactivate.zone_slot_name : NULL;
        if (slot_name == NULL && act != NULL && act->data.world_deactivate.state_name != NULL) {
            ASTNode *state = llvm_find_world_state_decl(stmt, act->data.world_deactivate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
        }
        if (slot_name != NULL) {
            char active_field[256];
            int active_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef active_ptr;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
        }
    }

    for (size_t i = 0; i < stmt->data.world_decl.state_count; i++) {
        ASTNode *state = stmt->data.world_decl.states[i];
        const char *slot_name;
        char state_field[256];
        char active_field[256];
        int state_idx;
        int active_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef active_ptr;
        LLVMValueRef active_val;
        if (state == NULL || state->type != AST_WORLD_STATE
            || state->data.world_state.state_name == NULL
            || state->data.world_state.zone_slot_name == NULL)
            continue;
        slot_name = state->data.world_state.zone_slot_name;
        snprintf(state_field, sizeof(state_field), "__zone_state_%s",
            state->data.world_state.state_name);
        snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
        state_idx = llvm_class_field_index(decl_cls, state_field);
        active_idx = llvm_class_field_index(decl_cls, active_field);
        if (state_idx < 0 || active_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)state_idx, llvm_tmp_name(ctx));
        active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
        active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            active_ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, active_val, state_ptr);
    }

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_class_name = saved_class_name;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

void
llvm_emit_domain_passes(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    /* Pass 0a: Register domain struct types + methods */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **slots = NULL;
        size_t slot_count = 0;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;
        ASTNode **refreshes = NULL;
        size_t refresh_count = 0;

        llvm_domain_decl_parts(stmt, &decl_name, &slots, &slot_count,
            &shared_fields, &shared_count, &methods, &method_count,
            &refreshes, &refresh_count);
        if (decl_name == NULL) {
            continue;
        }

        /* Count dyn role slots (for vtable pointer fields) */
        size_t dyn_slot_count = 0;
        ASTNode **role_slots = NULL;
        size_t role_count = 0;
        if (stmt->type == AST_PARTY_DECL) {
            role_slots = stmt->data.party_decl.role_slots;
            role_count = stmt->data.party_decl.role_count;
        }
        for (size_t j = 0; j < role_count; j++) {
            if (role_slots[j] != NULL
                && role_slots[j]->type == AST_ROLE_SLOT
                && role_slots[j]->data.role_slot.is_dynamic)
                dyn_slot_count++;
        }

        size_t fc = 0;
        LLVMTypeRef *ftypes = NULL;
        if (stmt->type == AST_ZONE_DECL) {
            fc = stmt->data.zone_decl.slot_count
                + stmt->data.zone_decl.shared_count
                + stmt->data.zone_decl.layer_slot_count
                + stmt->data.zone_decl.layer_slot_count
                + stmt->data.zone_decl.state_count;
            ftypes = calloc(fc > 0 ? fc : 1, sizeof(LLVMTypeRef));
            for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++) {
                ASTNode *slot = stmt->data.zone_decl.slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[j] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++) {
                ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[stmt->data.zone_decl.slot_count + j] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++) {
                size_t idx = stmt->data.zone_decl.slot_count
                    + stmt->data.zone_decl.shared_count + j;
                ftypes[idx] = ctx->type_i8ptr;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++) {
                size_t idx = stmt->data.zone_decl.slot_count
                    + stmt->data.zone_decl.shared_count
                    + stmt->data.zone_decl.layer_slot_count + j;
                ftypes[idx] = ctx->type_i1;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                size_t idx = stmt->data.zone_decl.slot_count
                    + stmt->data.zone_decl.shared_count
                    + stmt->data.zone_decl.layer_slot_count
                    + stmt->data.zone_decl.layer_slot_count + j;
                ftypes[idx] = ctx->type_i1;
            }
        } else if (stmt->type == AST_WORLD_DECL) {
            fc = stmt->data.world_decl.systemic_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.shared_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.state_count;
            ftypes = calloc(fc > 0 ? fc : 1, sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.world_decl.systemic_count; j++, idx++) {
                ASTNode *ws = stmt->data.world_decl.systemics[j];
                LLVMClassTypeEntry *field_cls = ws != NULL
                    ? llvm_lookup_class(ctx, ws->data.world_systemic.systemic_type) : NULL;
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++) {
                ASTNode *wz = stmt->data.world_decl.zones[j];
                LLVMClassTypeEntry *field_cls = wz != NULL
                    ? llvm_lookup_class(ctx, wz->data.world_zone.zone_type) : NULL;
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.world_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type) : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
        } else {
            /* Build struct: { slots..., shared_fields..., vtable_ptrs... } */
            fc = slot_count + shared_count + dyn_slot_count;
            ftypes = calloc(fc > 0 ? fc : 1, sizeof(LLVMTypeRef));
            for (size_t j = 0; j < slot_count; j++) {
                ASTNode *slot = slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[j] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < shared_count; j++) {
                ASTNode *sf = shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[slot_count + j] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < dyn_slot_count; j++)
                ftypes[slot_count + shared_count + j] = ctx->type_i8ptr;
        }

        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                        decl_name);
        LLVMStructSetBody(struct_ty, ftypes,
                           (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            decl_name, struct_ty, false);
        if (entry != NULL) {
            if (stmt->type == AST_ZONE_DECL) {
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.slots[j];
                    llvm_class_add_field(entry, slot->data.domain_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    llvm_class_add_field(entry, slot->data.zone_layer_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_active_%s",
                        slot->data.zone_layer_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
            } else if (stmt->type == AST_WORLD_DECL) {
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.world_decl.systemic_count; j++, field_index++) {
                    ASTNode *ws = stmt->data.world_decl.systemics[j];
                    llvm_class_add_field(entry, ws->data.world_systemic.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    llvm_class_add_field(entry, wz->data.world_zone.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.world_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_active_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
            } else {
                for (size_t j = 0; j < slot_count; j++) {
                    ASTNode *slot = slots[j];
                    llvm_class_add_field(entry,
                        slot->data.domain_slot.slot_name,
                        ftypes[j], (int)j);
                }
                for (size_t j = 0; j < shared_count; j++) {
                    ASTNode *sf = shared_fields[j];
                    llvm_class_add_field(entry,
                        sf->data.party_shared.name,
                        ftypes[slot_count + j], (int)(slot_count + j));
                }
                size_t dyn_idx = 0;
                for (size_t j = 0; j < role_count; j++) {
                    ASTNode *rs = role_slots[j];
                    if (rs == NULL || rs->type != AST_ROLE_SLOT
                        || !rs->data.role_slot.is_dynamic)
                        continue;
                    char vt_field_buf[256];
                    snprintf(vt_field_buf, sizeof(vt_field_buf), "%s_vtable",
                             rs->data.role_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(vt_field_buf),
                        ctx->type_i8ptr,
                        (int)(slot_count + shared_count + dyn_idx));
                    dyn_idx++;
                }
            }
        }
        free(ftypes);

        if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL
            || stmt->type == AST_ZONE_DECL || stmt->type == AST_WORLD_DECL) {
            char sync_name[256];
            LLVMTypeRef sync_params[] = { LLVMPointerType(struct_ty, 0) };
            LLVMTypeRef sync_ft = LLVMFunctionType(ctx->type_void, sync_params, 1, 0);
            LLVMValueRef sync_fn;
            snprintf(sync_name, sizeof(sync_name), "%s_sync", decl_name);
            sync_fn = LLVMAddFunction(ctx->module, sync_name, sync_ft);
            llvm_register_function(ctx, LLVMGetValueName(sync_fn),
                sync_fn, sync_ft, ctx->type_void);
        }

        /* Forward-declare methods */
        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            const char *mname = method->data.func_decl.name;
            size_t pc = method->data.func_decl.param_count;

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                user_pc++;
            }

            LLVMTypeRef *ptypes = calloc(user_pc + 1,
                                           sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr;
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                (unsigned)(user_pc + 1), 0);

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, mname);
            LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn),
                                    fn, ft, ret);
            free(ptypes);
        }
    }

    /* Pass 0b: Register ability vtable types */
    for (size_t i = 0; i < hir->ability_count; i++) {
        ASTNode *stmt = hir->abilities[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        const char *ab_name = stmt->data.ability_decl.name;
        size_t mc = stmt->data.ability_decl.method_count;

        /* Build vtable struct: { fn_ptr_1, fn_ptr_2, ... } */
        LLVMTypeRef *vt_fields = calloc(mc > 0 ? mc : 1,
                                          sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = stmt->data.ability_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t pc = method->data.func_decl.param_count;
            /* Count non-self params */
            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && p->name != NULL
                    && strcmp(p->name, "self") == 0)
                    continue;
                user_pc++;
            }
            LLVMTypeRef *ptypes = calloc(user_pc + 1, sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr; /* self */
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && p->name != NULL
                    && strcmp(p->name, "self") == 0)
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef fn_type = LLVMFunctionType(ret,
                ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
            free(ptypes);
        }

        char vt_name[256];
        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        LLVMTypeRef vt_struct = LLVMStructCreateNamed(ctx->context,
                                                        vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        free(vt_fields);

        /* Register as class type so it's findable.
         * Must strdup because vt_name is a stack local. */
        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            pergyra_strdup(vt_name), vt_struct, false);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = stmt->data.ability_decl.methods[j];
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        method->data.func_decl.name,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }

    /* Pass 0c: Forward-declare role methods + create vtable globals */
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                const char *mname = method->data.func_decl.name;
                size_t pc = method->data.func_decl.param_count;

                LLVMTypeRef ret = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret = ast_type_to_llvm(ctx,
                        method->data.func_decl.return_type);

                /* self + user params */
                size_t user_pc = 0;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    user_pc++;
                }

                LLVMTypeRef *ptypes = calloc(user_pc + 1,
                                               sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                size_t pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    ptypes[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                    (unsigned)(user_pc + 1), 0);

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, mname);
                LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                    fname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn),
                                        fn, ft, ret);
                free(ptypes);
            }
        }

        {
            TokenType ops[] = {
                TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                TOKEN_GREATER, TOKEN_GREATER_EQUAL
            };
            const char *for_type_name = NULL;
            if (stmt->data.role_decl.for_type != NULL
                && stmt->data.role_decl.for_type->type == AST_TYPE) {
                for_type_name = stmt->data.role_decl.for_type->data.type.name;
            }

            for (size_t oi = 0; for_type_name != NULL
                   && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                const char *suffix = llvm_operator_suffix(ops[oi]);
                ASTNode *method = llvm_find_role_operator_method(hir, stmt, ops[oi], 0);
                if (suffix == NULL || method == NULL)
                    continue;

                char opname[256];
                snprintf(opname, sizeof(opname), "operator_%s_%s",
                         suffix, for_type_name);
                if (llvm_lookup_function(ctx, opname) != NULL)
                    continue;

                FuncParam *rhs_param = NULL;
                size_t rhs_param_count = 0;
                for (size_t pj = 0; pj < method->data.func_decl.param_count; pj++) {
                    FuncParam *p = method->data.func_decl.params[pj];
                    if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                        rhs_param = p;
                        rhs_param_count++;
                    }
                }
                if (rhs_param_count != 1)
                    continue;

                LLVMTypeRef lhs_type = ast_type_to_llvm(ctx, stmt->data.role_decl.for_type);
                LLVMTypeRef rhs_type = (rhs_param != NULL && rhs_param->type != NULL)
                    ? ast_type_to_llvm(ctx, rhs_param->type) : ctx->type_i32;
                LLVMTypeRef ret = method->data.func_decl.return_type != NULL
                    ? ast_type_to_llvm(ctx, method->data.func_decl.return_type)
                    : ctx->type_void;
                LLVMTypeRef params[] = { lhs_type, rhs_type };
                LLVMTypeRef ft = LLVMFunctionType(ret, params, 2, 0);
                LLVMValueRef fn = LLVMAddFunction(ctx->module, opname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            }
        }
    }

    /* Pass 0e: Register event types and generate helper functions */
    for (size_t i = 0; i < hir->event_count; i++) {
        ASTNode *stmt = hir->events[i];
        if (stmt == NULL || stmt->type != AST_EVENT_DECL)
            continue;

        const char *ename = stmt->data.event_decl.name;
        int pc = (int)stmt->data.event_decl.param_count;

        /* Event struct: { [16 x ptr], i64 } → handlers + count */
        LLVMTypeRef handler_arr = LLVMArrayType(ctx->type_i8ptr,
                                                 PGY_EVENT_MAX_HANDLERS);
        LLVMTypeRef sfields[] = { handler_arr, ctx->type_i64 };
        char sname[256];
        snprintf(sname, sizeof(sname), "PgyEvent_%s", ename);
        LLVMTypeRef evt_struct = LLVMStructCreateNamed(ctx->context, sname);
        LLVMStructSetBody(evt_struct, sfields, 2, 0);

        /* Collect handler parameter types */
        LLVMTypeRef ptypes[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = stmt->data.event_decl.params[j];
            ptypes[j] = (p->data.let_decl.type != NULL)
                ? ast_type_to_llvm(ctx, p->data.let_decl.type)
                : ctx->type_i32;
        }
        llvm_register_event(ctx, ename, evt_struct, pc, ptypes);

        /* Handler function type: void(param_types...) */
        LLVMTypeRef handler_ft = LLVMFunctionType(ctx->type_void,
            ptypes, (unsigned)pc, 0);
        LLVMTypeRef handler_ptr_t = LLVMPointerTypeInContext(ctx->context, 0);
        (void)handler_ptr_t;

        /* --- Generate EventName_INIT(ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INIT", ename);
            LLVMTypeRef init_params[] = { ctx->type_i8ptr };
            LLVMTypeRef init_ft = LLVMFunctionType(ctx->type_void,
                init_params, 1, 0);
            LLVMValueRef init_fn = LLVMAddFunction(ctx->module, fname, init_ft);
            llvm_register_function(ctx, LLVMGetValueName(init_fn),
                init_fn, init_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, init_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            /* memset(e, 0, sizeof(struct)) */
            LLVMValueRef e_ptr = LLVMGetParam(init_fn, 0);
            LLVMValueRef sz = LLVMSizeOf(evt_struct);
            LLVMBuildMemSet(ctx->builder, e_ptr,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 0, 0),
                sz, 0);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_SUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", ename);
            LLVMTypeRef sub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef sub_ft = LLVMFunctionType(ctx->type_void,
                sub_params, 2, 0);
            LLVMValueRef sub_fn = LLVMAddFunction(ctx->module, fname, sub_ft);
            llvm_register_function(ctx, LLVMGetValueName(sub_fn),
                sub_fn, sub_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            LLVMValueRef e_ptr = LLVMGetParam(sub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(sub_fn, 1);

            /* count_ptr = GEP(e, 0, 1) — the i64 count field */
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* if (count < 16) { handlers[count] = h; count++; } */
            LLVMValueRef max_h = LLVMConstInt(ctx->type_i64,
                PGY_EVENT_MAX_HANDLERS, 0);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, count, max_h, "cmp");

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "then");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "end");
            LLVMBuildCondBr(ctx->builder, cmp, then_bb, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
            /* handlers_ptr = GEP(e, 0, 0, count) */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                count
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMBuildStore(ctx->builder, h_ptr, slot);

            /* count++ */
            LLVMValueRef new_count = LLVMBuildAdd(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "new_count");
            LLVMBuildStore(ctx->builder, new_count, count_ptr);
            LLVMBuildBr(ctx->builder, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_UNSUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", ename);
            LLVMTypeRef unsub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef unsub_ft = LLVMFunctionType(ctx->type_void,
                unsub_params, 2, 0);
            LLVMValueRef unsub_fn = LLVMAddFunction(ctx->module, fname, unsub_ft);
            llvm_register_function(ctx, LLVMGetValueName(unsub_fn),
                unsub_fn, unsub_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(unsub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(unsub_fn, 1);

            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* Loop: for (i = 0; i < count; i++) */
            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "loop");
            LLVMBasicBlockRef found_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "found");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "next");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "body");
            LLVMBuildCondBr(ctx->builder, cmp, body_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");
            LLVMValueRef eq = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, val, h_ptr, "eq");
            LLVMBuildCondBr(ctx->builder, eq, found_bb, next_bb);

            /* found: shift elements left, count-- */
            LLVMPositionBuilderAtEnd(ctx->builder, found_bb);
            /* Simple: set handlers[i] = handlers[count-1], count-- */
            LLVMValueRef last_idx_val = LLVMBuildSub(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "last");
            LLVMValueRef last_gep_idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                last_idx_val
            };
            LLVMValueRef last_slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, last_gep_idx, 3, "last_slot");
            LLVMValueRef last_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, last_slot, "last_val");
            LLVMBuildStore(ctx->builder, last_val, slot);
            LLVMBuildStore(ctx->builder, last_idx_val, count_ptr);
            LLVMBuildBr(ctx->builder, done_bb);

            /* next: i++ */
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_INVOKE(ptr, params...) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", ename);
            /* params: ptr (event), then handler params */
            LLVMTypeRef *inv_params = calloc((size_t)(pc + 1),
                sizeof(LLVMTypeRef));
            inv_params[0] = ctx->type_i8ptr;
            for (int j = 0; j < pc; j++)
                inv_params[j + 1] = ptypes[j];

            LLVMTypeRef inv_ft = LLVMFunctionType(ctx->type_void,
                inv_params, (unsigned)(pc + 1), 0);
            LLVMValueRef inv_fn = LLVMAddFunction(ctx->module, fname, inv_ft);
            llvm_register_function(ctx, LLVMGetValueName(inv_fn),
                inv_fn, inv_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(inv_fn, 0);
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "loop");
            LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "call");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBuildCondBr(ctx->builder, cmp, call_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, call_bb);
            /* Load handler pointer */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef hval = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");

            /* Call handler(params...) via indirect call */
            LLVMValueRef *call_args = calloc((size_t)pc, sizeof(LLVMValueRef));
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");
            free(call_args);

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);

            free(inv_params);
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

    /* Pass 2b: Emit role method bodies + vtable globals */
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name = impl->data.impl_ability.ability_name;

            /* Emit method bodies */
            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                if (fentry == NULL) continue;

                LLVMValueRef fn = fentry->fn;
                LLVMTypeRef ret_type = fentry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    ctx->type_i8ptr);

                /* User params */
                size_t pc = method->data.func_decl.param_count;
                unsigned lpidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef a = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, lpidx++), a);
                    llvm_scope_declare(ctx, p->name, a, pt);
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last =
                        LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }

            {
                TokenType ops[] = {
                    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                    TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                    TOKEN_GREATER, TOKEN_GREATER_EQUAL
                };
                const char *for_type_name = NULL;
                if (stmt->data.role_decl.for_type != NULL
                    && stmt->data.role_decl.for_type->type == AST_TYPE) {
                    for_type_name = stmt->data.role_decl.for_type->data.type.name;
                }

                for (size_t oi = 0; for_type_name != NULL
                       && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                    const char *suffix = llvm_operator_suffix(ops[oi]);
                    ASTNode *method = llvm_find_role_operator_method(hir, stmt, ops[oi], 0);
                    if (suffix == NULL || method == NULL)
                        continue;

                    char opname[256];
                    char mname[256];
                    snprintf(opname, sizeof(opname), "operator_%s_%s",
                             suffix, for_type_name);
                    snprintf(mname, sizeof(mname), "%s_%s",
                             role_name, method->data.func_decl.name);

                    LLVMFuncEntry *op_entry = llvm_lookup_function(ctx, opname);
                    LLVMFuncEntry *method_entry = llvm_lookup_function(ctx, mname);
                    if (op_entry == NULL || method_entry == NULL)
                        continue;
                    if (LLVMCountBasicBlocks(op_entry->fn) > 0)
                        continue;

                    LLVMValueRef saved_fn = ctx->current_function;
                    LLVMTypeRef saved_ret = ctx->current_ret_type;
                    LLVMTypeRef op_ret_type = op_entry->ret_type;
                    ctx->current_function = op_entry->fn;
                    ctx->current_ret_type = op_ret_type;

                    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                        ctx->context, op_entry->fn, "entry");
                    LLVMPositionBuilderAtEnd(ctx->builder, bb);

                    LLVMTypeRef lhs_type = LLVMTypeOf(LLVMGetParam(op_entry->fn, 0));
                    LLVMValueRef lhs_alloca = llvm_create_entry_alloca(
                        ctx, lhs_type, "lhs.addr");
                    LLVMBuildStore(ctx->builder, LLVMGetParam(op_entry->fn, 0), lhs_alloca);

                    LLVMValueRef lhs_self = LLVMBuildBitCast(ctx->builder,
                        lhs_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
                    LLVMValueRef rhs_arg = LLVMGetParam(op_entry->fn, 1);
                    LLVMValueRef args[] = { lhs_self, rhs_arg };

                    if (op_ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, method_entry->fn_type,
                            method_entry->fn, args, 2, "");
                        LLVMBuildRetVoid(ctx->builder);
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                            method_entry->fn_type, method_entry->fn,
                            args, 2, llvm_tmp_name(ctx));
                        LLVMBuildRet(ctx->builder, result);
                    }

                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    if (saved_fn != NULL) {
                        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                        if (last != NULL)
                            LLVMPositionBuilderAtEnd(ctx->builder, last);
                    }
                }
            }

            /* Create vtable global constant */
            char vt_type_name[256];
            snprintf(vt_type_name, sizeof(vt_type_name),
                     "%s_vtable", ab_name);
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = impl->data.impl_ability.method_count;
                LLVMValueRef *vals = calloc(mc > 0 ? mc : 1,
                                              sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = impl->data.impl_ability.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    char fname[256];
                    snprintf(fname, sizeof(fname), "%s_%s",
                             role_name, method->data.func_decl.name);
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    vals[j] = (fe != NULL) ? fe->fn
                        : LLVMConstNull(ctx->type_i8ptr);
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                snprintf(global_name, sizeof(global_name),
                         "%s_%s_vtable_instance", role_name, ab_name);
                LLVMValueRef global = LLVMAddGlobal(ctx->module,
                    vt_cls->struct_type, global_name);
                LLVMSetInitializer(global, vt_const);
                LLVMSetGlobalConstant(global, 1);
                LLVMSetLinkage(global, LLVMInternalLinkage);

                free(vals);
            }
        }
    }

    /* Pass 2c: Emit domain sync helpers + method bodies */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **slots = NULL;
        size_t slot_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **refreshes = NULL;
        size_t refresh_count = 0;

        llvm_domain_decl_parts(stmt, &decl_name, &slots, &slot_count,
            &shared_fields, &shared_count, &methods, &method_count,
            &refreshes, &refresh_count);
        if (decl_name == NULL) {
            continue;
        }

        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);
        if ((stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL
             || stmt->type == AST_ZONE_DECL || stmt->type == AST_WORLD_DECL)
            && cls != NULL) {
            char sync_name[256];
            LLVMFuncEntry *sync_entry;
            snprintf(sync_name, sizeof(sync_name), "%s_sync", decl_name);
            sync_entry = llvm_lookup_function(ctx, sync_name);
            if (sync_entry != NULL) {
                if (stmt->type == AST_ZONE_DECL)
                    llvm_emit_zone_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else if (stmt->type == AST_WORLD_DECL)
                    llvm_emit_world_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else
                    llvm_emit_domain_projection_sync(stmt, decl_name, cls,
                        sync_entry->fn, ctx);
            }
        }

        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMTypeRef ret_type = fentry->ret_type;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            const char *saved_class_name = ctx->current_class_name;
            LLVMFuncEntry *sync_entry = NULL;
            bool has_sync = false;
            ctx->current_function = fn;
            ctx->current_ret_type = ret_type;
            ctx->current_class_name = decl_name;

            if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL
                || stmt->type == AST_ZONE_DECL || stmt->type == AST_WORLD_DECL) {
                char sync_name[256];
                snprintf(sync_name, sizeof(sync_name), "%s_sync", decl_name);
                sync_entry = llvm_lookup_function(ctx, sync_name);
                has_sync = (sync_entry != NULL);
            }

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);
            llvm_scope_push(ctx);

            /* self param */
            LLVMValueRef self_val = LLVMGetParam(fn, 0);
            if (cls != NULL) {
                LLVMTypeRef self_ptr_t = LLVMPointerType(
                    cls->struct_type, 0);
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, self_ptr_t, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, self_ptr_t);
                llvm_register_var_class(ctx, "self", decl_name);
            } else {
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, ctx->type_i8ptr);
            }

            /* User params */
            size_t pc = method->data.func_decl.param_count;
            unsigned lpidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
                LLVMValueRef a = llvm_create_entry_alloca(
                    ctx, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, lpidx++), a);
                llvm_scope_declare(ctx, p->name, a, pt);
            }

            if (has_sync) {
                LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                    LLVMPointerType(cls->struct_type, 0),
                    llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                LLVMValueRef sync_args[] = { self_ptr };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }

            if (method->data.func_decl.body != NULL)
                llvm_emit_block(method->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (has_sync) {
                    LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                        LLVMPointerType(cls->struct_type, 0),
                        llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                    LLVMValueRef sync_args[] = { self_ptr };
                    LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                        sync_args, 1, "");
                }
                if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(ret_type, 0, 0));
            }

            llvm_scope_pop(ctx);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            ctx->current_class_name = saved_class_name;

            if (saved_fn != NULL) {
                LLVMBasicBlockRef last =
                    LLVMGetLastBasicBlock(saved_fn);
                if (last != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, last);
            }
        }
    }

}

#endif /* PGY_LLVM_ENABLED */
