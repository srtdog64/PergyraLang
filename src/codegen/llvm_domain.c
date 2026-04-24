/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — domain-specific passes (Party/Roster/World, Ability,
 * Role, Event).  Extracted from llvm_backend.c to keep file sizes
 * manageable.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

enum {
    PGY_PROP_CAUSE_NONE = 0,
    PGY_PROP_CAUSE_REFRESH = 1,
    PGY_PROP_CAUSE_APPLY = 2,
    PGY_PROP_CAUSE_MAINTAIN = 3,
    PGY_PROP_CAUSE_DETACH = 4,
    PGY_PROP_CAUSE_LINK = 5,
    PGY_PROP_CAUSE_UNLINK = 6,
    PGY_PROP_CAUSE_WORLD_ACTIVATE = 7,
    PGY_PROP_CAUSE_WORLD_MAINTAIN = 8,
    PGY_PROP_CAUSE_WORLD_DEACTIVATE = 9,
    PGY_PROP_CAUSE_WORLD_DERIVED = 10,
};

static void llvm_stamp_domain_provenance(LLVMGenCtx *ctx,
                                         LLVMClassTypeEntry *decl_cls,
                                         LLVMValueRef self_ptr,
                                         const char *prefix,
                                         const char *name,
                                         unsigned cause);

static const MIRRoutine *
llvm_find_mir_method_routine_local(const LLVMGenCtx *ctx,
                                   const char *owner_name,
                                   ASTNode *method)
{
    const char *method_name;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method == NULL || method->type != AST_FUNC_DECL) {
        return NULL;
    }

    method_name = method->data.func_decl.name;
    if (method_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, method_name) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }

    return NULL;
}

static bool
llvm_param_is_implicit_self_local(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

static const char *
llvm_operator_suffix(PgyTokenType op)
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
llvm_operator_method_name_matches(PgyTokenType op, const char *name)
{
    static const struct {
        PgyTokenType op;
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

#include "llvm_domain_helpers.inc"

static void
llvm_stamp_domain_provenance(LLVMGenCtx *ctx,
                             LLVMClassTypeEntry *decl_cls,
                             LLVMValueRef self_ptr,
                             const char *prefix,
                             const char *name,
                             unsigned cause)
{
    char field_name[256];
    int field_idx;

    if (ctx == NULL || decl_cls == NULL || self_ptr == NULL
        || prefix == NULL || name == NULL) {
        return;
    }

    snprintf(field_name, sizeof(field_name), "__%s_epoch_%s", prefix, name);
    field_idx = llvm_class_field_index(decl_cls, field_name);
    if (field_idx >= 0) {
        LLVMValueRef epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
            decl_cls->struct_type, self_ptr, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMValueRef epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            epoch_ptr, llvm_tmp_name(ctx));
        LLVMValueRef next_epoch = LLVMBuildAdd(ctx->builder, epoch_val,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, next_epoch, epoch_ptr);
    }

    snprintf(field_name, sizeof(field_name), "__%s_cause_%s", prefix, name);
    field_idx = llvm_class_field_index(decl_cls, field_name);
    if (field_idx >= 0) {
        LLVMValueRef cause_ptr = LLVMBuildStructGEP2(ctx->builder,
            decl_cls->struct_type, self_ptr, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, cause, 0),
            cause_ptr);
    }
}

static LLVMTypeRef
llvm_zone_effect_pool_struct_type(LLVMGenCtx *ctx, LLVMTypeRef effect_ty, int capacity)
{
    LLVMTypeRef fields[4];
    LLVMTypeRef i8_ty;
    unsigned cap;

    if (ctx == NULL || effect_ty == NULL)
        return NULL;

    if (capacity <= 0)
        capacity = 1;
    cap = (unsigned)capacity;
    i8_ty = LLVMInt8TypeInContext(ctx->context);

    fields[0] = LLVMArrayType(effect_ty, cap);
    fields[1] = LLVMArrayType(ctx->type_i1, cap);
    fields[2] = i8_ty;
    fields[3] = i8_ty;
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

static void
llvm_emit_zone_sync(ASTNode *stmt, const char *decl_name,
                    LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                    LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_ZONE_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }
    LLVMValueRef frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
        "zone.frontier.pass.addr");
    LLVMValueRef frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "zone.frontier.continue.addr");
    LLVMValueRef frontier_limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)(stmt->data.zone_decl.state_count
            + stmt->data.zone_decl.layer_slot_count + 1), 0);
    LLVMBasicBlockRef frontier_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.check");
    LLVMBasicBlockRef frontier_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.body");
    LLVMBasicBlockRef frontier_done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.done");
    LLVMBasicBlockRef frontier_overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.overflow");
    LLVMBasicBlockRef frontier_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.exit");
    LLVMValueRef *prev_state_addrs = pgy_arena_calloc(&ctx->scratch,
        (stmt->data.zone_decl.state_count > 0 ? stmt->data.zone_decl.state_count : 1)
            * sizeof(LLVMValueRef));
    LLVMValueRef *prev_layer_addrs = pgy_arena_calloc(&ctx->scratch,
        (stmt->data.zone_decl.layer_slot_count > 0 ? stmt->data.zone_decl.layer_slot_count : 1)
            * sizeof(LLVMValueRef));

    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        char prev_name[256];
        if (state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        snprintf(prev_name, sizeof(prev_name), "zone.prev_state.%s",
            state->data.zone_state.state_name);
        prev_state_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char prev_name[256];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        snprintf(prev_name, sizeof(prev_name), "zone.prev_layer.%s",
            slot->data.zone_layer_slot.slot_name);
        prev_layer_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }

    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), frontier_pass_addr);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), frontier_continue_addr);
    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_check_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            frontier_pass_addr, llvm_tmp_name(ctx));
        LLVMValueRef under_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
            pass_val, frontier_limit_val, llvm_tmp_name(ctx));
        LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
            under_limit, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, loop_cond, frontier_body_bb, frontier_done_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_body_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), frontier_continue_addr);
    LLVMBuildStore(ctx->builder,
        LLVMBuildAdd(ctx->builder,
            LLVMBuildLoad2(ctx->builder, ctx->type_i32, frontier_pass_addr, llvm_tmp_name(ctx)),
            LLVMConstInt(ctx->type_i32, 1, 0),
            llvm_tmp_name(ctx)),
        frontier_pass_addr);

    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef state_val;
        if (prev_state_addrs[i] == NULL || state == NULL || state->type != AST_ZONE_STATE
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
        state_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, state_val, prev_state_addrs[i]);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef layer_val;
        if (prev_layer_addrs[i] == NULL || slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
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
        layer_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            layer_ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, layer_val, prev_layer_addrs[i]);
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
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef pool_ptr;
        LLVMTypeRef pool_ty;
        LLVMValueRef items_ptr;
        LLVMValueRef active_ptr;
        LLVMValueRef count_ptr;
        LLVMValueRef cap_ptr;
        LLVMTypeRef i8_ty;

        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || !slot->data.zone_layer_slot.is_pool
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;

        field_idx = llvm_class_field_index(decl_cls,
            slot->data.zone_layer_slot.slot_name);
        if (field_idx < 0)
            continue;

        self_ptr = LLVMGetParam(sync_fn, 0);
        pool_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        pool_ty = decl_cls->fields[field_idx].field_type;
        i8_ty = LLVMInt8TypeInContext(ctx->context);

        items_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 0, llvm_tmp_name(ctx));
        active_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 1, llvm_tmp_name(ctx));
        count_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 2, llvm_tmp_name(ctx));
        cap_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 3, llvm_tmp_name(ctx));

        LLVMBuildStore(ctx->builder,
            LLVMConstNull(LLVMStructGetTypeAtIndex(pool_ty, 0)), items_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstNull(LLVMStructGetTypeAtIndex(pool_ty, 1)), active_ptr);
        LLVMBuildStore(ctx->builder, LLVMConstInt(i8_ty, 0, 0), count_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(i8_ty,
                slot->data.zone_layer_slot.pool_capacity > 0
                    ? (unsigned)slot->data.zone_layer_slot.pool_capacity : 1,
                0),
            cap_ptr);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_APPLY);
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
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_APPLY);
                    }
                    if (apply->data.zone_apply.target_slot_name != NULL) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            layer_name, apply->data.zone_apply.target_slot_name);
                    } else {
                        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                            ASTNode *state = stmt->data.zone_decl.states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && !state->data.zone_state.is_relation
                                && state->data.zone_state.state_name != NULL
                                && strcmp(state->data.zone_state.state_name, state_name) == 0
                                && state->data.zone_state.left_or_target_slot_name != NULL) {
                                llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    state->data.zone_state.left_or_target_slot_name);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (apply != NULL
                   && apply->data.zone_apply.effect_slot_name != NULL
                   && apply->data.zone_apply.target_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                apply->data.zone_apply.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    apply->data.zone_apply.effect_slot_name,
                    PGY_PROP_CAUSE_APPLY);
            }
            llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                apply->data.zone_apply.effect_slot_name,
                apply->data.zone_apply.target_slot_name);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_effects[i];
        if (maintain == NULL
            || maintain->data.zone_maintain_effect.effect_slot_name == NULL
            || maintain->data.zone_maintain_effect.target_slot_name == NULL) {
            continue;
        }
        {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                maintain->data.zone_maintain_effect.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    maintain->data.zone_maintain_effect.effect_slot_name,
                    PGY_PROP_CAUSE_MAINTAIN);
            }
            if (maintain->data.zone_maintain_relation.left_slot_name != NULL
                && maintain->data.zone_maintain_relation.right_slot_name != NULL) {
                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                    maintain->data.zone_maintain_relation.relation_slot_name,
                    maintain->data.zone_maintain_relation.left_slot_name,
                    maintain->data.zone_maintain_relation.right_slot_name);
            }
        }
        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name);
        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
            ASTNode *state = stmt->data.zone_decl.states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.layer_slot_name == NULL
                || state->data.zone_state.left_or_target_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_effect.effect_slot_name) != 0
                || strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_effect.target_slot_name) != 0) {
                continue;
            }
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_MAINTAIN);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                maintain->data.zone_maintain_state.state_name,
                PGY_PROP_CAUSE_MAINTAIN);
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
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", state->data.zone_state.layer_slot_name,
                            PGY_PROP_CAUSE_MAINTAIN);
                    }
                    if (!state->data.zone_state.is_relation) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            state->data.zone_state.layer_slot_name,
                            state->data.zone_state.left_or_target_slot_name);
                    } else if (state->data.zone_state.right_slot_name != NULL) {
                        llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                            state->data.zone_state.layer_slot_name,
                            state->data.zone_state.left_or_target_slot_name,
                            state->data.zone_state.right_slot_name);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_DETACH);
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
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_DETACH);
                    }
                }
            }
        } else if (detach != NULL && detach->data.zone_detach.effect_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                detach->data.zone_detach.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    detach->data.zone_detach.effect_slot_name,
                    PGY_PROP_CAUSE_DETACH);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_LINK);
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
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_LINK);
                    }
                    if (link->data.zone_link.left_slot_name != NULL
                        && link->data.zone_link.right_slot_name != NULL) {
                        llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                            layer_name,
                            link->data.zone_link.left_slot_name,
                            link->data.zone_link.right_slot_name);
                    } else {
                        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                            ASTNode *state = stmt->data.zone_decl.states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && state->data.zone_state.is_relation
                                && state->data.zone_state.state_name != NULL
                                && strcmp(state->data.zone_state.state_name, state_name) == 0
                                && state->data.zone_state.left_or_target_slot_name != NULL
                                && state->data.zone_state.right_slot_name != NULL) {
                                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    state->data.zone_state.left_or_target_slot_name,
                                    state->data.zone_state.right_slot_name);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (link != NULL
                   && link->data.zone_link.relation_slot_name != NULL
                   && link->data.zone_link.left_slot_name != NULL
                   && link->data.zone_link.right_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                link->data.zone_link.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    link->data.zone_link.relation_slot_name,
                    PGY_PROP_CAUSE_LINK);
            }
            llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                link->data.zone_link.relation_slot_name,
                link->data.zone_link.left_slot_name,
                link->data.zone_link.right_slot_name);
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
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    maintain->data.zone_maintain_relation.relation_slot_name,
                    PGY_PROP_CAUSE_MAINTAIN);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_MAINTAIN);
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
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_UNLINK);
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
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_UNLINK);
                    }
                }
            }
        } else if (unlink != NULL && unlink->data.zone_unlink.relation_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                unlink->data.zone_unlink.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef current_val;
        LLVMValueRef prev_val;
        LLVMValueRef changed_val;
        LLVMValueRef pending_val;
        if (prev_state_addrs[i] == NULL || state == NULL || state->type != AST_ZONE_STATE
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
        current_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            prev_state_addrs[i], llvm_tmp_name(ctx));
        changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, current_val, prev_val,
            llvm_tmp_name(ctx));
        pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder, pending_val, changed_val, llvm_tmp_name(ctx)),
            frontier_continue_addr);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef current_val;
        LLVMValueRef prev_val;
        LLVMValueRef changed_val;
        LLVMValueRef pending_val;
        if (prev_layer_addrs[i] == NULL || slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
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
        current_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            layer_ptr, llvm_tmp_name(ctx));
        prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            prev_layer_addrs[i], llvm_tmp_name(ctx));
        changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, current_val, prev_val,
            llvm_tmp_name(ctx));
        pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder, pending_val, changed_val, llvm_tmp_name(ctx)),
            frontier_continue_addr);
    }

    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
    {
        LLVMTypeRef abort_ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
        LLVMFuncEntry *abort_fn = llvm_lookup_or_create_function(ctx, "abort",
            abort_ft, ctx->type_void);
        if (abort_fn != NULL) {
            LLVMBuildCall2(ctx->builder, abort_fn->fn_type, abort_fn->fn,
                NULL, 0, "");
        }
        LLVMBuildUnreachable(ctx->builder);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

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
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMValueRef derived_dirty_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.derived_dirty.addr");
        LLVMValueRef needs_derived_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.needs_derived.addr");
        int derived_idx = llvm_class_field_index(decl_cls, "__world_derived_dirty");
        LLVMValueRef derived_ptr = NULL;
        LLVMValueRef derived_val = LLVMConstInt(ctx->type_i1, 0, 0);
        size_t zone_count = stmt->data.world_decl.zone_count;
        /* Per-zone "previously active" pointer cache — populated during
         * world sync emission and consumed once before this function
         * returns.  Never escapes. */
        LLVMValueRef *prev_active_addrs = pgy_arena_calloc(&ctx->scratch,
            (zone_count > 0 ? zone_count : 1) * sizeof(LLVMValueRef));

        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
        llvm_scope_declare(ctx, "__world_derived_dirty_local", derived_dirty_addr, ctx->type_i1);

        if (derived_idx >= 0) {
            derived_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                LLVMGetParam(sync_fn, 0), (unsigned)derived_idx, llvm_tmp_name(ctx));
            derived_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                derived_ptr, llvm_tmp_name(ctx));
        }
        LLVMBuildStore(ctx->builder, derived_val, derived_dirty_addr);

        /* world command pass: reset */
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *zone = stmt->data.world_decl.zones[i];
            char active_field[256];
            char prev_name[256];
            int active_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef prev_addr;
            LLVMValueRef prev_val;
            if (zone == NULL || zone->type != AST_WORLD_ZONE
                || zone->data.world_zone.slot_name == NULL)
                continue;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s",
                zone->data.world_zone.slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            self_ptr = LLVMGetParam(sync_fn, 0);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            snprintf(prev_name, sizeof(prev_name), "world.prev_active.%s",
                zone->data.world_zone.slot_name);
            prev_addr = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, prev_val, prev_addr);
            prev_active_addrs[i] = prev_addr;
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
        }

        /* world command pass: directives */
        for (size_t i = 0; i < stmt->data.world_decl.activate_count; i++) {
            ASTNode *act = stmt->data.world_decl.activations[i];
            const char *slot_name = act != NULL ? act->data.world_activate.zone_slot_name : NULL;
            if (slot_name == NULL && act != NULL && act->data.world_activate.state_name != NULL) {
                ASTNode *state = llvm_find_world_state_decl(stmt, act->data.world_activate.state_name);
                if (state != NULL)
                    slot_name = state->data.world_state.zone_slot_name;
                else if (llvm_world_has_zone_slot(stmt, act->data.world_activate.state_name))
                    slot_name = act->data.world_activate.state_name;
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
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone",
                    slot_name, PGY_PROP_CAUSE_WORLD_ACTIVATE);
            }
        }
        for (size_t i = 0; i < stmt->data.world_decl.maintained_zone_count; i++) {
            ASTNode *mnt = stmt->data.world_decl.maintained_zones[i];
            const char *slot_name = mnt != NULL ? mnt->data.world_maintain.zone_slot_name : NULL;
            if (slot_name == NULL && mnt != NULL && mnt->data.world_maintain.state_name != NULL) {
                ASTNode *state = llvm_find_world_state_decl(stmt, mnt->data.world_maintain.state_name);
                if (state != NULL)
                    slot_name = state->data.world_state.zone_slot_name;
                else if (llvm_world_has_zone_slot(stmt, mnt->data.world_maintain.state_name))
                    slot_name = mnt->data.world_maintain.state_name;
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
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone",
                    slot_name, PGY_PROP_CAUSE_WORLD_MAINTAIN);
            }
        }
        for (size_t i = 0; i < stmt->data.world_decl.deactivate_count; i++) {
            ASTNode *act = stmt->data.world_decl.deactivations[i];
            const char *slot_name = act != NULL ? act->data.world_deactivate.zone_slot_name : NULL;
            if (slot_name == NULL && act != NULL && act->data.world_deactivate.state_name != NULL) {
                ASTNode *state = llvm_find_world_state_decl(stmt, act->data.world_deactivate.state_name);
                if (state != NULL)
                    slot_name = state->data.world_state.zone_slot_name;
                else if (llvm_world_has_zone_slot(stmt, act->data.world_deactivate.state_name))
                    slot_name = act->data.world_deactivate.state_name;
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
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone",
                    slot_name, PGY_PROP_CAUSE_WORLD_DEACTIVATE);
            }
        }

        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *zone = stmt->data.world_decl.zones[i];
            const char *slot_name;
            char active_field[256];
            char dirty_field[256];
            int active_idx;
            int dirty_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef active_val;
            LLVMValueRef prev_val;
            LLVMValueRef changed_val;
            if (zone == NULL || zone->type != AST_WORLD_ZONE
                || zone->data.world_zone.slot_name == NULL
                || prev_active_addrs[i] == NULL)
                continue;
            slot_name = zone->data.world_zone.slot_name;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
            if (active_idx < 0 || dirty_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                prev_active_addrs[i], llvm_tmp_name(ctx));
            changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, active_val, prev_val,
                llvm_tmp_name(ctx));
            {
                LLVMValueRef prev_dirty = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    dirty_ptr, llvm_tmp_name(ctx));
                changed_val = LLVMBuildOr(ctx->builder, prev_dirty, changed_val,
                    llvm_tmp_name(ctx));
            }
            LLVMBuildStore(ctx->builder, changed_val, dirty_ptr);
            {
                LLVMValueRef derived_dirty_val = LLVMBuildOr(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                        llvm_tmp_name(ctx)),
                    changed_val, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, derived_dirty_val, derived_dirty_addr);
                if (derived_ptr != NULL)
                    LLVMBuildStore(ctx->builder, derived_dirty_val, derived_ptr);
            }
        }

        {
            LLVMValueRef frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
                "world.frontier.pass.addr");
            LLVMValueRef frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
                "world.frontier.continue.addr");
            LLVMValueRef pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
                "world.derived.pass.addr");
            LLVMValueRef continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
                "world.derived.continue.addr");
            LLVMValueRef frontier_limit_val = LLVMConstInt(ctx->type_i32,
                (unsigned long long)(zone_count + stmt->data.world_decl.state_count + 1), 0);
            LLVMValueRef limit_val = LLVMConstInt(ctx->type_i32,
                (unsigned long long)(stmt->data.world_decl.state_count + 1), 0);
            LLVMBasicBlockRef frontier_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.check");
            LLVMBasicBlockRef frontier_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.body");
            LLVMBasicBlockRef frontier_done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.done");
            LLVMBasicBlockRef frontier_overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.overflow");
            LLVMBasicBlockRef frontier_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.exit");
            LLVMBasicBlockRef derived_init_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.init");
            LLVMBasicBlockRef loop_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.check");
            LLVMBasicBlockRef loop_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.body");
            LLVMBasicBlockRef overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.overflow");
            LLVMBasicBlockRef finalize_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.finalize");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.done");
            LLVMBasicBlockRef derived_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.exit");

            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), frontier_pass_addr);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), frontier_continue_addr);
            LLVMBuildBr(ctx->builder, frontier_check_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, frontier_check_bb);
            {
                LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    frontier_continue_addr, llvm_tmp_name(ctx));
                LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    frontier_pass_addr, llvm_tmp_name(ctx));
                LLVMValueRef under_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
                    pass_val, frontier_limit_val, llvm_tmp_name(ctx));
                LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
                    under_limit, llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, loop_cond, frontier_body_bb, frontier_done_bb);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, frontier_body_bb);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), frontier_continue_addr);
            {
                LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    frontier_pass_addr, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildAdd(ctx->builder, pass_val,
                        LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                    frontier_pass_addr);
            }
            if (derived_ptr != NULL) {
                LLVMBuildStore(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx)),
                    needs_derived_addr);
            } else {
                LLVMBuildStore(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                        llvm_tmp_name(ctx)),
                    needs_derived_addr);
            }

            /* world zone sync pass */
            for (size_t i = 0; i < zone_count; i++) {
                ASTNode *zone = stmt->data.world_decl.zones[i];
                int zone_idx;
                int dirty_idx;
                char dirty_field[256];
                LLVMValueRef self_ptr;
                LLVMValueRef dirty_ptr;
                LLVMValueRef dirty_val;
                LLVMBasicBlockRef sync_bb;
                LLVMBasicBlockRef cont_bb;
                if (zone == NULL || zone->type != AST_WORLD_ZONE
                    || zone->data.world_zone.slot_name == NULL)
                    continue;
                zone_idx = llvm_class_field_index(decl_cls, zone->data.world_zone.slot_name);
                snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s",
                    zone->data.world_zone.slot_name);
                dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
                self_ptr = LLVMGetParam(sync_fn, 0);
                if (zone_idx < 0 || dirty_idx < 0 || zone->data.world_zone.zone_type == NULL)
                    continue;
                {
                    LLVMClassTypeEntry *zone_cls = llvm_lookup_class(ctx, zone->data.world_zone.zone_type);
                    char sync_name[256];
                    LLVMFuncEntry *zone_sync;
                    snprintf(sync_name, sizeof(sync_name), "%s_sync",
                        zone->data.world_zone.zone_type);
                    zone_sync = llvm_lookup_function(ctx, sync_name);
                    if (zone_cls == NULL || zone_sync == NULL)
                        continue;
                    dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                        self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
                    dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                        dirty_ptr, llvm_tmp_name(ctx));
                    sync_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.sync");
                    cont_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.cont");
                    LLVMBuildCondBr(ctx->builder, dirty_val, sync_bb, cont_bb);
                    LLVMPositionBuilderAtEnd(ctx->builder, sync_bb);
                    {
                        LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { zone_ptr };
                        LLVMBuildCall2(ctx->builder, zone_sync->fn_type, zone_sync->fn, args, 1, "");
                    }
                    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), dirty_ptr);
                    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), needs_derived_addr);
                    LLVMBuildBr(ctx->builder, cont_bb);
                    LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
                }
            }

            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), pass_addr);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);
            {
                LLVMValueRef needs_derived = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    needs_derived_addr, llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, needs_derived, derived_init_bb, done_bb);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, derived_init_bb);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), pass_addr);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), continue_addr);
            LLVMBuildBr(ctx->builder, loop_check_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, loop_check_bb);
            {
                LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    continue_addr, llvm_tmp_name(ctx));
                LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    pass_addr, llvm_tmp_name(ctx));
                LLVMValueRef under_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
                    pass_val, limit_val, llvm_tmp_name(ctx));
                LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
                    under_limit, llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, loop_cond, loop_body_bb, done_bb);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, loop_body_bb);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);
            {
                LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    pass_addr, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildAdd(ctx->builder, pass_val,
                        LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                    pass_addr);
            }
            for (size_t i = 0; i < stmt->data.world_decl.state_count; i++) {
                ASTNode *state = stmt->data.world_decl.states[i];
                const char *slot_name;
                char state_field[256];
                char active_field[256];
                int state_idx;
                int active_idx = -1;
                LLVMValueRef self_ptr;
                LLVMValueRef state_ptr;
                LLVMValueRef prev_state_val;
                LLVMValueRef active_ptr = NULL;
                LLVMValueRef active_val = LLVMConstInt(ctx->type_i1, 0, 0);
                LLVMValueRef derived_val = NULL;
                LLVMValueRef changed_val;
                if (state == NULL || state->type != AST_WORLD_STATE
                    || state->data.world_state.state_name == NULL)
                    continue;
                slot_name = state->data.world_state.zone_slot_name;
                snprintf(state_field, sizeof(state_field), "__zone_state_%s",
                    state->data.world_state.state_name);
                state_idx = llvm_class_field_index(decl_cls, state_field);
                if (state_idx < 0)
                    continue;
                self_ptr = LLVMGetParam(sync_fn, 0);
                state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)state_idx, llvm_tmp_name(ctx));
                prev_state_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    state_ptr, llvm_tmp_name(ctx));
                if (slot_name != NULL) {
                    snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
                    active_idx = llvm_class_field_index(decl_cls, active_field);
                    if (active_idx >= 0) {
                        active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
                        active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                            active_ptr, llvm_tmp_name(ctx));
                    }
                }
                derived_val = active_val;

                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                    || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
                    derived_val = LLVMConstInt(ctx->type_i1,
                        state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
                    for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                        const char *input_name = state->data.world_state.input_names[input_i];
                        int input_idx = -1;
                        LLVMValueRef input_ptr;
                        LLVMValueRef input_val;
                        if (input_name == NULL)
                            continue;
                        if (llvm_world_has_zone_slot(stmt, input_name)) {
                            char input_field[256];
                            snprintf(input_field, sizeof(input_field), "__zone_active_%s", input_name);
                            input_idx = llvm_class_field_index(decl_cls, input_field);
                        } else {
                            char input_field[256];
                            snprintf(input_field, sizeof(input_field), "__zone_state_%s", input_name);
                            input_idx = llvm_class_field_index(decl_cls, input_field);
                        }
                        if (input_idx < 0)
                            continue;
                        input_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)input_idx, llvm_tmp_name(ctx));
                        input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                            input_ptr, llvm_tmp_name(ctx));
                        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL)
                            derived_val = LLVMBuildAnd(ctx->builder, derived_val, input_val,
                                llvm_tmp_name(ctx));
                        else
                            derived_val = LLVMBuildOr(ctx->builder, derived_val, input_val,
                                llvm_tmp_name(ctx));
                    }
                }

                if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE
                    && state->data.world_state.source_kind != WORLD_STATE_SOURCE_ALL
                    && state->data.world_state.source_kind != WORLD_STATE_SOURCE_ANY
                    && state->data.world_state.detail_name != NULL) {
                    int zone_idx = llvm_class_field_index(decl_cls, slot_name);
                    LLVMClassTypeEntry *zone_cls = NULL;
                    if (zone_idx >= 0) {
                        LLVMTypeRef zone_field_ty = decl_cls->fields[zone_idx].field_type;
                        zone_cls = llvm_lookup_class_by_struct_type(ctx, zone_field_ty);
                    }
                    if (zone_cls != NULL && zone_idx >= 0) {
                        char detail_field[256];
                        int detail_idx = -1;
                        LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                        LLVMValueRef detail_ptr;
                        LLVMValueRef detail_val;

                        switch (state->data.world_state.source_kind) {
                        case WORLD_STATE_SOURCE_PROJECTION:
                            snprintf(detail_field, sizeof(detail_field), "__projection_ready_%s",
                                state->data.world_state.detail_name);
                            break;
                        case WORLD_STATE_SOURCE_LAYER:
                            snprintf(detail_field, sizeof(detail_field), "__layer_active_%s",
                                state->data.world_state.detail_name);
                            break;
                        case WORLD_STATE_SOURCE_STATE:
                            snprintf(detail_field, sizeof(detail_field), "__state_%s",
                                state->data.world_state.detail_name);
                            break;
                        case WORLD_STATE_SOURCE_ZONE:
                        default:
                            detail_field[0] = '\0';
                            break;
                        }

                        if (detail_field[0] != '\0')
                            detail_idx = llvm_class_field_index(zone_cls, detail_field);
                        if (detail_idx >= 0) {
                            detail_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
                                zone_ptr, (unsigned)detail_idx, llvm_tmp_name(ctx));
                            detail_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                                detail_ptr, llvm_tmp_name(ctx));
                            derived_val = LLVMBuildAnd(ctx->builder, active_val, detail_val,
                                llvm_tmp_name(ctx));
                        }
                    }
                }

                LLVMBuildStore(ctx->builder, derived_val, state_ptr);
                changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, prev_state_val,
                    derived_val, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildOr(ctx->builder,
                        LLVMBuildLoad2(ctx->builder, ctx->type_i1, continue_addr, llvm_tmp_name(ctx)),
                        changed_val, llvm_tmp_name(ctx)),
                    continue_addr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone_state",
                    state->data.world_state.state_name, PGY_PROP_CAUSE_WORLD_DERIVED);
            }
            LLVMBuildBr(ctx->builder, loop_check_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            {
                LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    continue_addr, llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, continue_val, overflow_bb, finalize_bb);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, overflow_bb);
            {
                LLVMTypeRef abort_ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
                LLVMFuncEntry *abort_fn = llvm_lookup_or_create_function(ctx, "abort",
                    abort_ft, ctx->type_void);
                if (abort_fn != NULL) {
                    LLVMBuildCall2(ctx->builder, abort_fn->fn_type, abort_fn->fn,
                        NULL, 0, "");
                }
                LLVMBuildUnreachable(ctx->builder);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, finalize_bb);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_dirty_addr);
            if (derived_ptr != NULL)
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_ptr);
            LLVMBuildBr(ctx->builder, derived_exit_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, derived_exit_bb);
            {
                LLVMValueRef pending_val = derived_ptr != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx))
                    : LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                        llvm_tmp_name(ctx));
                for (size_t i = 0; i < zone_count; i++) {
                    ASTNode *zone = stmt->data.world_decl.zones[i];
                    char dirty_field[256];
                    int dirty_idx;
                    LLVMValueRef self_ptr;
                    LLVMValueRef dirty_ptr;
                    LLVMValueRef dirty_val;
                    if (zone == NULL || zone->type != AST_WORLD_ZONE
                        || zone->data.world_zone.slot_name == NULL)
                        continue;
                    snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s",
                        zone->data.world_zone.slot_name);
                    dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
                    if (dirty_idx < 0)
                        continue;
                    self_ptr = LLVMGetParam(sync_fn, 0);
                    dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                        self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
                    dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                        dirty_ptr, llvm_tmp_name(ctx));
                    pending_val = LLVMBuildOr(ctx->builder, pending_val, dirty_val,
                        llvm_tmp_name(ctx));
                }
                LLVMBuildStore(ctx->builder, pending_val, frontier_continue_addr);
            }
            LLVMBuildBr(ctx->builder, frontier_check_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
            {
                LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    frontier_continue_addr, llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
            {
                LLVMTypeRef abort_ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
                LLVMFuncEntry *abort_fn = llvm_lookup_or_create_function(ctx, "abort",
                    abort_ft, ctx->type_void);
                if (abort_fn != NULL) {
                    LLVMBuildCall2(ctx->builder, abort_fn->fn_type, abort_fn->fn,
                        NULL, 0, "");
                }
                LLVMBuildUnreachable(ctx->builder);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
        }

        /* prev_active_addrs is ctx->scratch-owned. */
    }

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

void
llvm_emit_domain_passes(LLVMGenCtx *ctx)
{
    LLVMDomainInventory inventory;
    ASTNode **abilities;
    ASTNode **relations;
    ASTNode **effects;
    ASTNode **zones;
    ASTNode **worlds;
    ASTNode **parties;
    ASTNode **rosters;
    ASTNode **roles;
    ASTNode **events;
    size_t ability_count;
    size_t relation_count;
    size_t effect_count;
    size_t zone_count;
    size_t world_count;
    size_t party_count;
    size_t roster_count;
    size_t role_count;
    size_t event_count;

    if (ctx == NULL)
        return;

    llvm_active_domain_inventory(ctx, &inventory);
    abilities = inventory.abilities;
    relations = inventory.relations;
    effects = inventory.effects;
    zones = inventory.zones;
    worlds = inventory.worlds;
    parties = inventory.parties;
    rosters = inventory.rosters;
    roles = inventory.roles;
    events = inventory.events;
    ability_count = inventory.ability_count;
    relation_count = inventory.relation_count;
    effect_count = inventory.effect_count;
    zone_count = inventory.zone_count;
    world_count = inventory.world_count;
    party_count = inventory.party_count;
    roster_count = inventory.roster_count;
    role_count = inventory.role_count;
    event_count = inventory.event_count;

    ASTNode **domain_groups[] = {
        relations,
        effects,
        zones,
        worlds,
        parties,
        rosters,
    };
    size_t domain_group_counts[] = {
        relation_count,
        effect_count,
        zone_count,
        world_count,
        party_count,
        roster_count,
    };

    /* Pass 0a: Register domain struct types + methods */
    for (size_t group = 0;
         group < sizeof(domain_groups) / sizeof(domain_groups[0]);
         group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
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
            size_t projection_count =
                llvm_count_domain_projection_slots(stmt->data.zone_decl.slots,
                    stmt->data.zone_decl.slot_count,
                    stmt->data.zone_decl.refreshes,
                    stmt->data.zone_decl.refresh_count);
            fc = stmt->data.zone_decl.slot_count
                + stmt->data.zone_decl.shared_count
                + stmt->data.zone_decl.layer_slot_count
                + stmt->data.zone_decl.layer_slot_count
                + (stmt->data.zone_decl.layer_slot_count * 2)
                + stmt->data.zone_decl.state_count
                + (stmt->data.zone_decl.state_count * 2)
                + (projection_count * 4);
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, idx++) {
                ASTNode *slot = stmt->data.zone_decl.slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[idx] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, idx++) {
                ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                LLVMClassTypeEntry *layer_cls = NULL;
                if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
                    && slot->data.zone_layer_slot.layer_type != NULL) {
                    layer_cls = llvm_lookup_class(ctx,
                        slot->data.zone_layer_slot.layer_type);
                }
                if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
                    && slot->data.zone_layer_slot.is_pool
                    && layer_cls != NULL) {
                    ftypes[idx] = llvm_zone_effect_pool_struct_type(ctx,
                        layer_cls->struct_type,
                        slot->data.zone_layer_slot.pool_capacity);
                } else {
                    ftypes[idx] = layer_cls != NULL ? layer_cls->struct_type : ctx->type_i8ptr;
                }
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, idx++) {
                ftypes[idx] = ctx->type_i1;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count * 2; j++, idx++) {
                ftypes[idx] = ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, idx++) {
                ftypes[idx] = ctx->type_i1;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.state_count * 2; j++, idx++) {
                ftypes[idx] = ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++) {
                ASTNode *slot = stmt->data.zone_decl.slots[j];
                if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                    || (!slot->data.domain_slot.is_tobject
                        && !llvm_domain_slot_is_projection_target(slot,
                            stmt->data.zone_decl.refreshes,
                            stmt->data.zone_decl.refresh_count))) {
                    continue;
                }
                ftypes[idx++] = ctx->type_i1;
                ftypes[idx++] = ctx->type_i1;
                ftypes[idx++] = ctx->type_i32;
                ftypes[idx++] = ctx->type_i32;
            }
        } else if (stmt->type == AST_ROSTER_DECL) {
            fc = stmt->data.roster_decl.party_count
                + stmt->data.roster_decl.shared_count;
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.roster_decl.party_count; j++, idx++) {
                ASTNode *slot = stmt->data.roster_decl.party_slots[j];
                LLVMClassTypeEntry *field_cls = NULL;
                if (slot != NULL && slot->type == AST_SYSTEMIC_SLOT
                    && slot->data.roster_slot.party_type != NULL) {
                    field_cls = llvm_lookup_class(ctx,
                        slot->data.roster_slot.party_type);
                }
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.roster_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.roster_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
        } else if (stmt->type == AST_WORLD_DECL) {
            fc = stmt->data.world_decl.roster_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.shared_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.state_count
                + (stmt->data.world_decl.zone_count * 2)
                + (stmt->data.world_decl.state_count * 2)
                + 1;
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.world_decl.roster_count; j++, idx++) {
                ASTNode *ws = stmt->data.world_decl.rosters[j];
                LLVMClassTypeEntry *field_cls = ws != NULL
                    ? llvm_lookup_class(ctx, ws->data.world_roster.roster_type) : NULL;
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
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.zone_count * 2; j++, idx++)
                ftypes[idx] = ctx->type_i32;
            for (size_t j = 0; j < stmt->data.world_decl.state_count * 2; j++, idx++)
                ftypes[idx] = ctx->type_i32;
            ftypes[idx] = ctx->type_i1;
        } else {
            /* Build struct: { slots..., shared_fields..., vtable_ptrs... } */
            size_t projection_count =
                (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                ? llvm_count_domain_projection_slots(slots, slot_count,
                    refreshes, refresh_count)
                : 0;
            fc = slot_count + shared_count + dyn_slot_count + projection_count;
            if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                fc = slot_count + shared_count + dyn_slot_count + (projection_count * 4);
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < slot_count; j++, idx++) {
                ASTNode *slot = slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[idx] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < shared_count; j++, idx++) {
                ASTNode *sf = shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < dyn_slot_count; j++, idx++)
                ftypes[idx] = ctx->type_i8ptr;
            if (projection_count > 0) {
                for (size_t j = 0; j < slot_count; j++) {
                    ASTNode *slot = slots[j];
                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || (!slot->data.domain_slot.is_tobject
                            && !llvm_domain_slot_is_projection_target(slot,
                                refreshes, refresh_count))) {
                        continue;
                    }
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i32;
                    ftypes[idx++] = ctx->type_i32;
                }
            }
        }

        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                        decl_name);
        LLVMStructSetBody(struct_ty, ftypes,
                           (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            decl_name, struct_ty, false, true);
        if (entry != NULL) {
            if (stmt->type == AST_ZONE_DECL) {
                entry->domain_kind = LLVM_DOMAIN_ZONE;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.slots[j];
                    llvm_class_add_field_ex(entry, slot->data.domain_slot.slot_name,
                        ftypes[field_index], field_index,
                        slot->data.domain_slot.is_subject);
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
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_epoch_%s",
                        slot->data.zone_layer_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_cause_%s",
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
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_epoch_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_cause_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++) {
                    ASTNode *slot = stmt->data.zone_decl.slots[j];
                    char field_name[256];
                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || (!slot->data.domain_slot.is_tobject
                            && !llvm_domain_slot_is_projection_target(slot,
                                stmt->data.zone_decl.refreshes,
                                stmt->data.zone_decl.refresh_count))
                        || slot->data.domain_slot.slot_name == NULL) {
                        continue;
                    }
                    snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_epoch_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_cause_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                }
            } else if (stmt->type == AST_ROSTER_DECL) {
                entry->domain_kind = LLVM_DOMAIN_SYSTEMIC;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.roster_decl.party_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.roster_decl.party_slots[j];
                    llvm_class_add_field(entry, slot->data.roster_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.roster_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.roster_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
            } else if (stmt->type == AST_WORLD_DECL) {
                entry->domain_kind = LLVM_DOMAIN_WORLD;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.world_decl.roster_count; j++, field_index++) {
                    ASTNode *ws = stmt->data.world_decl.rosters[j];
                    llvm_class_add_field(entry, ws->data.world_roster.slot_name,
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
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_dirty_%s",
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
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_epoch_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_cause_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_epoch_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_cause_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                llvm_class_add_field(entry, pergyra_strdup("__world_derived_dirty"),
                    ftypes[field_index], field_index);
            } else {
                if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                    entry->domain_kind = LLVM_DOMAIN_PROJECTION;
                int field_index = 0;
                for (size_t j = 0; j < slot_count; j++, field_index++) {
                    ASTNode *slot = slots[j];
                    llvm_class_add_field(entry,
                        slot->data.domain_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < shared_count; j++, field_index++) {
                    ASTNode *sf = shared_fields[j];
                    llvm_class_add_field(entry,
                        sf->data.party_shared.name,
                        ftypes[field_index], field_index);
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
                        field_index);
                    dyn_idx++;
                    field_index++;
                }
                if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL) {
                    for (size_t j = 0; j < slot_count; j++) {
                        ASTNode *slot = slots[j];
                        char field_name[256];
                        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                            || (!slot->data.domain_slot.is_tobject
                                && !llvm_domain_slot_is_projection_target(slot,
                                    refreshes, refresh_count))
                            || slot->data.domain_slot.slot_name == NULL) {
                            continue;
                        }
                        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_epoch_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_cause_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                    }
                }
            }
        }
        /* ftypes is ctx->scratch-owned. */

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
            if (entry != NULL)
                entry->sync_function_name = pergyra_strdup(sync_name);
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
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                user_pc++;
            }

            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (user_pc + 1) * sizeof(LLVMTypeRef));
            ptypes[0] = LLVMPointerType(struct_ty, 0);
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                const char *type_name = NULL;
                LLVMClassTypeEntry *param_cls = NULL;
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                if (p->type != NULL && p->type->type == AST_TYPE)
                    type_name = p->type->data.type.name;
                param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
                if (param_cls != NULL && param_cls->is_pointer_self_host)
                    ptypes[pidx++] = LLVMPointerType(param_cls->struct_type, 0);
                else
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
            /* ptypes is ctx->scratch-owned. */
        }
        }
    }

    /* Pass 0b: Register ability vtable types */
    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        const char *ab_name = stmt->data.ability_decl.name;
        size_t mc = stmt->data.ability_decl.method_count;

        /* Build vtable struct: { fn_ptr_1, fn_ptr_2, ... }.  Type arrays
         * are consumed by LLVMFunctionType / LLVMStructSetBody and never
         * retained after the struct type is registered below. */
        LLVMTypeRef *vt_fields = pgy_arena_calloc(&ctx->scratch,
            (mc > 0 ? mc : 1) * sizeof(LLVMTypeRef));
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
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                user_pc++;
            }
            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (user_pc + 1) * sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr; /* self */
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef fn_type = LLVMFunctionType(ret,
                ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
            /* ptypes is ctx->scratch-owned. */
        }

        char vt_name[256];
        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        LLVMTypeRef vt_struct = LLVMStructCreateNamed(ctx->context,
                                                        vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        /* vt_fields is ctx->scratch-owned. */

        /* Register as class type so it's findable.
         * Must strdup because vt_name is a stack local. */
        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            pergyra_strdup(vt_name), vt_struct, false, false);
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
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
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
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    user_pc++;
                }

                LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                    (user_pc + 1) * sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                size_t pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
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
                /* ptypes is ctx->scratch-owned. */
            }
        }

        {
            PgyTokenType ops[] = {
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
                ASTNode *method = llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
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
                    if (!llvm_param_is_implicit_self_local(p)) {
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
    for (size_t i = 0; i < event_count; i++) {
        ASTNode *stmt = events[i];
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
            /* params: ptr (event), then handler params.  Consumed by
             * LLVMFunctionType (which copies the type array) and never
             * retained beyond this block. */
            LLVMTypeRef *inv_params = pgy_arena_calloc(&ctx->scratch,
                (size_t)(pc + 1) * sizeof(LLVMTypeRef));
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

            /* Call handler(params...) via indirect call.  Arg buffer is
             * consumed by LLVMBuildCall2 and never retained. */
            LLVMValueRef *call_args = pgy_arena_calloc(&ctx->scratch,
                (size_t)pc * sizeof(LLVMValueRef));
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");
            /* call_args is ctx->scratch-owned. */

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);

            /* inv_params is ctx->scratch-owned. */
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

    /* Pass 2b: Emit role method bodies + vtable globals */
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name =
                (impl->data.impl_ability.ability_ref != NULL
                 && impl->data.impl_ability.ability_ref->type == AST_TYPE)
                ? impl->data.impl_ability.ability_ref->data.type.name : NULL;

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
                const MIRRoutine *mir_method =
                    llvm_find_mir_method_routine_local(ctx, role_name, method);
                if (fentry == NULL) continue;
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    continue;
                }
                if (ctx->mir != NULL) {
                    llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_MIR_ROUTINE_MISSING, PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING, PGY_FIX_INSPECT_MIR_INVENTORY, "MIR-only LLVM path missing routine for "
                                   "domain method '%s.%s'",
                                   role_name, method->data.func_decl.name);
                    return;
                }

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
                    if (llvm_param_is_implicit_self_local(p))
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

                {
                    char msg[384];
                    snprintf(msg, sizeof(msg),
                             "MIR-only LLVM path missing routine for role method '%s.%s'",
                             role_name != NULL ? role_name : "(anonymous-role)",
                             method->data.func_decl.name != NULL
                                 ? method->data.func_decl.name
                                 : "(anonymous)");
                    llvm_set_error_with_hints(ctx,
                        PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "%s", msg);
                    llvm_scope_pop(ctx);
                    return;
                }

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
                PgyTokenType ops[] = {
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
                    ASTNode *method = llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
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
                /* Vtable method value array — consumed by
                 * LLVMConstNamedStruct (copies) for the global initializer. */
                LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
                    (mc > 0 ? mc : 1) * sizeof(LLVMValueRef));
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

                /* vals is ctx->scratch-owned. */
            }
        }
    }

    /* Pass 2c: Emit domain sync helpers + method bodies */
    for (size_t group = 0;
         group < sizeof(domain_groups) / sizeof(domain_groups[0]);
         group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
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
        if (cls != NULL && cls->domain_kind != LLVM_DOMAIN_NONE
            && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC
            && cls->sync_function_name != NULL) {
            LLVMFuncEntry *sync_entry;
            sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
            if (sync_entry != NULL) {
                if (cls->domain_kind == LLVM_DOMAIN_ZONE)
                    llvm_emit_zone_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else if (cls->domain_kind == LLVM_DOMAIN_WORLD)
                    llvm_emit_world_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else
                    llvm_emit_domain_projection_sync(stmt, decl_name, cls,
                        sync_entry->fn, ctx);
            }
        }

        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            const MIRRoutine *mir_method;
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            mir_method = llvm_find_mir_method_routine_local(ctx, decl_name, method);
            if (mir_method != NULL) {
                llvm_emit_func_from_mir(mir_method, ctx);
                continue;
            }
            if (ctx->mir != NULL) {
                char msg[384];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path missing routine for domain method '%s.%s'",
                         decl_name != NULL ? decl_name : "(anonymous-domain)",
                         method->data.func_decl.name != NULL
                             ? method->data.func_decl.name
                             : "(anonymous)");
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "%s", msg);
                return;
            }

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMTypeRef ret_type = fentry->ret_type;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ASTNode *saved_host_decl = NULL;
            LLVMFuncEntry *sync_entry = NULL;
            bool has_sync = false;
            ctx->current_function = fn;
            ctx->current_ret_type = ret_type;
            saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);

            if (cls != NULL && cls->sync_function_name != NULL
                && cls->domain_kind != LLVM_DOMAIN_NONE
                && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC) {
                sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
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
                const char *type_name = NULL;
                LLVMClassTypeEntry *param_cls = NULL;
                LLVMTypeRef pt;
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                if (p->type != NULL && p->type->type == AST_TYPE)
                    type_name = p->type->data.type.name;
                param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
                if (param_cls != NULL && param_cls->is_pointer_self_host)
                    pt = LLVMPointerType(param_cls->struct_type, 0);
                else
                    pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                LLVMValueRef a = llvm_create_entry_alloca(
                    ctx, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, lpidx++), a);
                llvm_scope_declare(ctx, p->name, a, pt);
                if (type_name != NULL && param_cls != NULL)
                    llvm_register_var_class(ctx, p->name, type_name);
            }

            if (has_sync) {
                LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                    LLVMPointerType(cls->struct_type, 0),
                    llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                LLVMValueRef sync_args[] = { self_ptr };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }

            {
                char msg[384];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path missing routine for domain method '%s.%s'",
                         decl_name != NULL ? decl_name : "(anonymous-domain)",
                         method->data.func_decl.name != NULL
                             ? method->data.func_decl.name
                             : "(anonymous)");
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "%s", msg);
                return;
            }

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (stmt->type == AST_WORLD_DECL && cls != NULL) {
                    for (size_t k = 0; k < stmt->data.world_decl.zone_count; k++) {
                        ASTNode *zone = stmt->data.world_decl.zones[k];
                        char dirty_field[256];
                        int dirty_idx;
                        LLVMValueRef self_ptr;
                        LLVMValueRef dirty_ptr;
                        const char *slot_name = zone != NULL
                            ? zone->data.world_zone.slot_name
                            : NULL;
                        if (slot_name == NULL)
                            continue;
                        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
                        dirty_idx = llvm_class_field_index(cls, dirty_field);
                        if (dirty_idx < 0)
                            continue;
                        self_ptr = LLVMBuildLoad2(ctx->builder,
                            LLVMPointerType(cls->struct_type, 0),
                            llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                        dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
                    }
                    int derived_idx = llvm_class_field_index(cls, "__world_derived_dirty");
                    if (derived_idx >= 0) {
                        LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                            LLVMPointerType(cls->struct_type, 0),
                            llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, self_ptr, (unsigned)derived_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), derived_ptr);
                    }
                }
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
            llvm_restore_current_host_decl(ctx, saved_host_decl);

            if (saved_fn != NULL) {
                LLVMBasicBlockRef last =
                    LLVMGetLastBasicBlock(saved_fn);
                if (last != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, last);
            }
        }
        }
    }

}

#endif /* PGY_LLVM_ENABLED */
