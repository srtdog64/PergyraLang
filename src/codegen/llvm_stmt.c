#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "../semantic/slot_analyzer.h"

/* =================================================================
 * Statement emission
 * ================================================================= */

static char *
llvm_stmt_render_type_arg(GenericParam *param)
{
    ASTNode *type = NULL;

    if (param == NULL)
        return pergyra_strdup("Int");

    type = param->constraint;
    if (type != NULL && type->type == AST_TYPE && type->data.type.name != NULL) {
        if (type->data.type.generic_args == NULL || type->data.type.generic_args->count == 0)
            return pergyra_strdup(type->data.type.name);

        char *result = pergyra_strdup(type->data.type.name);
        for (size_t i = 0; i < type->data.type.generic_args->count; i++) {
            char *arg = llvm_stmt_render_type_arg(type->data.type.generic_args->params[i]);
            size_t cur_len = strlen(result);
            size_t arg_len = strlen(arg);
            size_t need = cur_len + arg_len + 4;
            char *grown = realloc(result, need);
            if (grown == NULL) {
                free(result);
                free(arg);
                return pergyra_strdup("Int");
            }
            result = grown;
            size_t offset = cur_len;
            if (i == 0) {
                result[offset++] = '<';
            } else {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            memcpy(result + offset, arg, arg_len);
            offset += arg_len;
            result[offset] = '\0';
            free(arg);
        }
        {
            size_t cur_len = strlen(result);
            char *grown = realloc(result, cur_len + 2);
            if (grown == NULL) {
                free(result);
                return pergyra_strdup("Int");
            }
            result = grown;
            result[cur_len] = '>';
            result[cur_len + 1] = '\0';
        }
        return result;
    }

    if (param->name != NULL)
        return pergyra_strdup(param->name);
    return pergyra_strdup("Int");
}

static ASTNode *
llvm_stmt_find_zone_decl(LLVMGenCtx *ctx, const char *zone_name)
{
    if (ctx == NULL || ctx->hir == NULL || zone_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt != NULL && stmt->type == AST_ZONE_DECL
            && stmt->data.zone_decl.name != NULL
            && strcmp(stmt->data.zone_decl.name, zone_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static ASTNode *
llvm_stmt_find_effect_decl(LLVMGenCtx *ctx, const char *effect_name)
{
    if (ctx == NULL || ctx->hir == NULL || effect_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt != NULL && stmt->type == AST_EFFECT_DECL
            && stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, effect_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static ASTNode *
llvm_stmt_find_subject_host_decl(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || ctx->hir == NULL || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static ASTNode *
llvm_stmt_find_function_decl_by_name(LLVMGenCtx *ctx, const char *name)
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

    return NULL;
}

static bool
llvm_stmt_slot_can_sink_locally(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return false;
    if (ctx->current_func_decl->type != AST_FUNC_DECL
        || ctx->current_func_decl->data.func_decl.body == NULL)
        return false;
    return slot_analyze_escape_flags(ctx->current_func_decl->data.func_decl.body, name)
        == SLOT_ESCAPE_NONE;
}

static LLVMValueRef
llvm_stmt_create_slot_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    if (llvm_stmt_slot_can_sink_locally(ctx, name))
        return LLVMBuildAlloca(ctx->builder, type, name);
    return llvm_create_entry_alloca(ctx, type, name);
}

static LLVMClassTypeEntry *
llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    if (ctx == NULL || type == NULL)
        return NULL;

    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == type)
            return &ctx->class_types[i];
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_lambda_signature_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL || expr->type != AST_LAMBDA_EXPR)
        return NULL;

    int pc = (int)expr->data.lambda_expr.param_count;
    LLVMTypeRef *params = NULL;
    LLVMTypeRef ret_type = ctx->type_i32;

    if (expr->data.lambda_expr.return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, expr->data.lambda_expr.return_type);
    }

    if (pc > 0) {
        params = calloc((size_t)pc, sizeof(LLVMTypeRef));
        if (params == NULL)
            return LLVMPointerType(LLVMFunctionType(ret_type, NULL, 0, 0), 0);
        for (int i = 0; i < pc; i++) {
            ASTNode *p = expr->data.lambda_expr.params[i];
            if (p != NULL && p->type == AST_LET_DECL && p->data.let_decl.type != NULL)
                params[i] = ast_type_to_llvm(ctx, p->data.let_decl.type);
            else
                params[i] = ctx->type_i32;
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, params, (unsigned)pc, 0);
    free(params);
    return LLVMPointerType(fn_type, 0);
}

static const char *
llvm_stmt_infer_nominal_name_from_init(LLVMGenCtx *ctx, ASTNode *init)
{
    const char *name;

    if (ctx == NULL || init == NULL)
        return NULL;

    if (init->type == AST_IDENTIFIER && init->data.identifier.name != NULL) {
        name = init->data.identifier.name;
        if (llvm_scope_lookup(ctx, name) != NULL) {
            const char *tracked = llvm_lookup_var_class(ctx, name);
            if (tracked != NULL)
                return tracked;
        }
        if (ctx->current_class_name != NULL && strcmp(name, "self") != 0) {
            LLVMClassTypeEntry *host_cls = llvm_lookup_class(ctx, ctx->current_class_name);
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                        ctx, host_cls->fields[field_idx].field_type);
                    if (field_cls != NULL)
                        return field_cls->class_name;
                }
            }
        }
        return NULL;
    }

    if (init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.callee->data.identifier.name != NULL) {
        name = init->data.call.callee->data.identifier.name;
        if ((strcmp(name, "ListGet") == 0 || strcmp(name, "QueuePop") == 0)
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *collection = init->data.call.arguments[0]->data.identifier.name;
            const char *inner = strcmp(name, "ListGet") == 0
                ? llvm_lookup_list_inner(ctx, collection)
                : llvm_lookup_queue_inner(ctx, collection);
            if (inner != NULL && llvm_lookup_class(ctx, inner) != NULL)
                return inner;
        }
        if (strcmp(name, "MapGet") == 0
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *collection = init->data.call.arguments[0]->data.identifier.name;
            const char *value = llvm_lookup_map_value(ctx, collection);
            if (value != NULL && llvm_lookup_class(ctx, value) != NULL)
                return value;
        }
        if (llvm_lookup_class(ctx, name) != NULL)
            return name;
        {
            LLVMFuncEntry *callee_fn = llvm_lookup_function(ctx, name);
            if (callee_fn == NULL && ctx->current_class_name != NULL) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                    ctx->current_class_name, name);
                callee_fn = llvm_lookup_function(ctx, full_name);
            }
            LLVMClassTypeEntry *ret_cls = callee_fn != NULL
                ? llvm_stmt_lookup_class_by_type(ctx, callee_fn->ret_type)
                : NULL;
            if (ret_cls != NULL)
                return ret_cls->class_name;
        }
    }

    if (init->type == AST_MEMBER_ACCESS
        && init->data.member.object != NULL
        && init->data.member.name != NULL) {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, init->data.member.object);
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, init->data.member.name);
            if (field_idx >= 0) {
                LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                    ctx, base_cls->fields[field_idx].field_type);
                if (field_cls != NULL)
                    return field_cls->class_name;
            }
        }
    }

    return NULL;
}

static LLVMTypeRef
llvm_stmt_infer_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    const char *nominal_name;
    LLVMClassTypeEntry *nominal_cls;

    if (ctx == NULL || expr == NULL)
        return ctx->type_i32;

    nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, expr);
    nominal_cls = nominal_name != NULL ? llvm_lookup_class(ctx, nominal_name) : NULL;
    if (nominal_cls != NULL)
        return nominal_cls->struct_type;

    switch (expr->type) {
    case AST_STRING:
        return ctx->type_i8ptr;
    case AST_BOOLEAN:
        return ctx->type_i1;
    case AST_NUMBER:
        return ctx->type_i32;
    case AST_IDENTIFIER: {
        LLVMVarEntry *var = llvm_scope_lookup(ctx, expr->data.identifier.name);
        return var != NULL ? var->type : ctx->type_i32;
    }
    case AST_MEMBER_ACCESS: {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, expr->data.member.object);
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, expr->data.member.name);
            if (field_idx >= 0)
                return base_cls->fields[field_idx].field_type;
        }
        return ctx->type_i32;
    }
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL
            && expr->data.call.callee->data.member.object != NULL
            && expr->data.call.callee->data.member.object->type == AST_IDENTIFIER) {
            const char *receiver_name =
                expr->data.call.callee->data.member.object->data.identifier.name;
            const char *method_name = expr->data.call.callee->data.member.name;
            const char *inner = llvm_lookup_slot_inner(ctx, receiver_name);
            if (inner == NULL) {
                LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, receiver_name);
                if (view != NULL)
                    inner = view->inner_type;
            }
            if (inner == NULL)
                inner = llvm_lookup_device_slot_inner(ctx, receiver_name);
            if (inner != NULL && strcmp(method_name, "Read") == 0)
                return pergyra_type_to_llvm(ctx, inner);
            if (inner != NULL
                && (strcmp(method_name, "Write") == 0
                    || strcmp(method_name, "Release") == 0)) {
                return ctx->type_void;
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *callee = expr->data.call.callee->data.identifier.name;
            if ((strcmp(callee, "Read") == 0
                 || strcmp(callee, "Write") == 0
                 || strcmp(callee, "Release") == 0)
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *receiver_name =
                    expr->data.call.arguments[0]->data.identifier.name;
                const char *inner = llvm_lookup_slot_inner(ctx, receiver_name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, receiver_name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, receiver_name);
                if (inner != NULL && strcmp(callee, "Read") == 0)
                    return pergyra_type_to_llvm(ctx, inner);
                if (inner != NULL)
                    return ctx->type_void;
            }
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, callee);
            if (fn == NULL && ctx->current_class_name != NULL) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                    ctx->current_class_name, callee);
                fn = llvm_lookup_function(ctx, full_name);
            }
            if (fn != NULL)
                return fn->ret_type;
            if (strcmp(callee, "ToString") == 0
                || strcmp(callee, "ReadFile") == 0
                || strcmp(callee, "Input") == 0
                || strcmp(callee, "Upper") == 0
                || strcmp(callee, "ToUpper") == 0
                || strcmp(callee, "Lower") == 0
                || strcmp(callee, "ToLower") == 0
                || strcmp(callee, "Concat") == 0
                || strcmp(callee, "StringConcat") == 0) {
                return ctx->type_i8ptr;
            }
            if (strcmp(callee, "ListGet") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *inner = llvm_lookup_list_inner(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            if (strcmp(callee, "QueuePop") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *inner = llvm_lookup_queue_inner(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            if (strcmp(callee, "MapGet") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *value = llvm_lookup_map_value(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (value != NULL)
                    return pergyra_type_to_llvm(ctx, value);
            }
            if (strcmp(callee, "ListSize") == 0
                || strcmp(callee, "QueueSize") == 0
                || strcmp(callee, "MapSize") == 0) {
                return ctx->type_i32;
            }
            if (strcmp(callee, "QueueEmpty") == 0
                || strcmp(callee, "MapHas") == 0) {
                return ctx->type_i1;
            }
            if (strcmp(callee, "HasZone") == 0
                || strcmp(callee, "HasState") == 0
                || strcmp(callee, "HasLayer") == 0
                || strcmp(callee, "HasProjection") == 0) {
                return ctx->type_i1;
            }
        }
        return ctx->type_i32;
    case AST_BINARY: {
        PgyTokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return ctx->type_i1;
        }
        if (op == TOKEN_PLUS) {
            LLVMTypeRef left_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.left);
            LLVMTypeRef right_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.right);
            if (left_ty == ctx->type_i8ptr || right_ty == ctx->type_i8ptr)
                return ctx->type_i8ptr;
        }
        return ctx->type_i32;
    }
    default:
        return ctx->type_i32;
    }
}

static ASTNode *
llvm_stmt_find_host_method_decl(ASTNode *host_decl, const char *method_name)
{
    if (host_decl == NULL || method_name == NULL)
        return NULL;

    if (host_decl->type == AST_CLASS_DECL) {
        for (size_t i = 0; i < host_decl->data.class_decl.method_count; i++) {
            ASTNode *method = host_decl->data.class_decl.methods[i];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return method;
            }
        }
    }

    return NULL;
}

static ASTNode *
llvm_stmt_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
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
llvm_stmt_find_nth_subject_slot(ASTNode **slots, size_t slot_count, size_t nth)
{
    size_t seen = 0;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject) {
            continue;
        }
        if (seen == nth)
            return slot;
        seen++;
    }

    return NULL;
}

static bool
llvm_stmt_resolve_zone_subject_receiver(LLVMGenCtx *ctx, ASTNode *receiver,
                                        const char **slot_name_out,
                                        const char **type_name_out)
{
    ASTNode *zone_decl;
    ASTNode *slot_decl = NULL;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || ctx->current_class_name == NULL || receiver == NULL)
        return false;

    zone_decl = llvm_stmt_find_zone_decl(ctx, ctx->current_class_name);
    if (zone_decl == NULL)
        return false;

    if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
        slot_name = receiver->data.identifier.name;
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    } else if (receiver->type == AST_MEMBER_ACCESS
               && receiver->data.member.object != NULL
               && receiver->data.member.object->type == AST_IDENTIFIER
               && receiver->data.member.object->data.identifier.name != NULL
               && strcmp(receiver->data.member.object->data.identifier.name, "self") == 0
               && receiver->data.member.name != NULL) {
        slot_name = receiver->data.member.name;
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    }

    if (slot_decl == NULL || !slot_decl->data.domain_slot.is_subject
        || slot_decl->data.domain_slot.type == NULL
        || slot_decl->data.domain_slot.type->type != AST_TYPE
        || slot_decl->data.domain_slot.type->data.type.name == NULL) {
        return false;
    }

    type_name = slot_decl->data.domain_slot.type->data.type.name;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

static void
llvm_stmt_emit_zone_action_effect_runtime(ASTNode *call, LLVMGenCtx *ctx)
{
    ASTNode *callee;
    ASTNode *receiver;
    ASTNode *zone_decl;
    ASTNode *host_decl;
    ASTNode *method_decl;
    ASTNode *effect_decl;
    LLVMClassTypeEntry *zone_cls;
    LLVMClassTypeEntry *effect_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef self_ptr;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;

    if (ctx == NULL || ctx->current_class_name == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    zone_decl = llvm_stmt_find_zone_decl(ctx, ctx->current_class_name);
    if (zone_decl == NULL)
        return;

    callee = call->data.call.callee;
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = callee->data.member.object;
    method_name = callee->data.member.name;
    if (receiver == NULL || method_name == NULL)
        return;

    if (!llvm_stmt_resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    host_decl = llvm_stmt_find_subject_host_decl(ctx, receiver_type_name);
    method_decl = llvm_stmt_find_host_method_decl(host_decl, method_name);
    if (method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL
        || strcmp(method_decl->data.func_decl.within_zone, ctx->current_class_name) != 0) {
        return;
    }

    effect_name = method_decl->data.func_decl.causes_effect;
    effect_decl = llvm_stmt_find_effect_decl(ctx, effect_name);
    zone_cls = llvm_lookup_class(ctx, ctx->current_class_name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (effect_decl == NULL || zone_cls == NULL || effect_cls == NULL || self_var == NULL)
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(zone_cls->struct_type, 0),
        self_var->alloca, llvm_tmp_name(ctx));

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        ASTNode *subject_slot;
        const char *layer_name;
        int active_idx;
        int layer_idx;
        int target_idx;
        int subject_idx;
        LLVMValueRef active_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef target_ptr;
        LLVMValueRef target_value;
        LLVMFuncEntry *sync_entry;
        char active_field[256];
        char sync_name[256];

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        snprintf(active_field, sizeof(active_field), "__layer_active_%s", layer_name);
        active_idx = llvm_class_field_index(zone_cls, active_field);
        if (active_idx >= 0) {
            active_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }

        subject_slot = llvm_stmt_find_nth_subject_slot(effect_decl->data.effect_decl.slots,
            effect_decl->data.effect_decl.slot_count, 0);
        if (subject_slot == NULL || subject_slot->data.domain_slot.slot_name == NULL)
            continue;

        layer_idx = llvm_class_field_index(zone_cls, layer_name);
        target_idx = llvm_class_field_index(zone_cls, receiver_slot_name);
        subject_idx = llvm_class_field_index(effect_cls, subject_slot->data.domain_slot.slot_name);
        if (layer_idx < 0 || target_idx < 0 || subject_idx < 0)
            continue;

        layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
        target_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            self_ptr, (unsigned)target_idx, llvm_tmp_name(ctx));
        target_value = LLVMBuildLoad2(ctx->builder,
            zone_cls->fields[target_idx].field_type, target_ptr, llvm_tmp_name(ctx));
        {
            LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
                layer_ptr, (unsigned)subject_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, target_value, subject_ptr);
        }

        snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_name);
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

static const char *
llvm_simple_expr_type_name(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *callee;
    ASTNode *receiver;
    const char *method_name;

    if (expr == NULL)
        return "Int";

    switch (expr->type) {
    case AST_NUMBER: return "Int";
    case AST_STRING: return "String";
    case AST_BOOLEAN: return "Bool";
    case AST_IDENTIFIER: {
        LLVMVarEntry *entry = llvm_scope_lookup(ctx, expr->data.identifier.name);
        if (entry != NULL)
            return llvm_type_to_suffix(ctx, entry->type);
        return "Int";
    }
    case AST_CALL:
        callee = expr->data.call.callee;
        if (callee != NULL
            && callee->type == AST_MEMBER_ACCESS
            && callee->data.member.name != NULL) {
            receiver = callee->data.member.object;
            method_name = callee->data.member.name;
            if (receiver != NULL && receiver->type == AST_IDENTIFIER) {
                const char *name = receiver->data.identifier.name;
                const char *inner = llvm_lookup_slot_inner(ctx, name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, name);
                if (inner != NULL && strcmp(method_name, "Read") == 0)
                    return inner;
                if (inner != NULL
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
            }
        }
        if (callee != NULL
            && callee->type == AST_IDENTIFIER
            && callee->data.identifier.name != NULL
            && expr->data.call.arg_count >= 1
            && expr->data.call.arguments[0] != NULL
            && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *name = expr->data.call.arguments[0]->data.identifier.name;
            const char *inner = NULL;
            if (strcmp(callee->data.identifier.name, "Read") == 0
                || strcmp(callee->data.identifier.name, "Write") == 0
                || strcmp(callee->data.identifier.name, "Release") == 0) {
                inner = llvm_lookup_slot_inner(ctx, name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, name);
                if (inner != NULL && strcmp(callee->data.identifier.name, "Read") == 0)
                    return inner;
                if (inner != NULL)
                    return "Void";
            }
        }
        return "Int";
    default:
        return "Int";
    }
}

static bool
llvm_is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

void
llvm_defer_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth >= MAX_SCOPE_DEPTH)
        return;
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

void
llvm_defer_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

static void
llvm_register_defer(ASTNode *body, LLVMGenCtx *ctx)
{
    if (body == NULL || ctx->defer_scope_depth <= 0)
        return;
    int scope = ctx->defer_scope_depth - 1;
    int count = ctx->defer_body_counts[scope];
    if (count >= MAX_DEFER_PER_SCOPE)
        return;
    ctx->defer_bodies[scope][count] = body;
    ctx->defer_body_counts[scope]++;
}

void
llvm_emit_defers_from(LLVMGenCtx *ctx, int start_depth)
{
    if (start_depth < 0)
        start_depth = 0;
    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            if (body != NULL)
                llvm_emit_statement(body, ctx);
        }
    }
}

static const char *
llvm_infer_spawn_future_inner(LLVMGenCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    ASTNode *call = NULL;
    ASTNode *callee = target;
    const char *callee_name = NULL;
    static char buf[128];

    if (target != NULL && target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL || ctx->hir == NULL)
        return "Int";

    ASTNode *decl = NULL;
    for (size_t i = 0; i < ctx->hir->function_count; i++) {
        ASTNode *fn = ctx->hir->functions[i];
        if (fn != NULL && fn->type == AST_FUNC_DECL
            && fn->data.func_decl.name != NULL
            && strcmp(fn->data.func_decl.name, callee_name) == 0) {
            decl = fn;
            break;
        }
    }
    if (decl == NULL || decl->data.func_decl.return_type == NULL
        || decl->data.func_decl.return_type->type != AST_TYPE
        || decl->data.func_decl.return_type->data.type.name == NULL) {
        return "Int";
    }

    const char *ret_name = decl->data.func_decl.return_type->data.type.name;
    if (!(ret_name[0] >= 'A' && ret_name[0] <= 'Z' && ret_name[1] == '\0'))
        return ret_name;

    if (call == NULL)
        return "Int";

    for (size_t i = 0; i < decl->data.func_decl.param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE
            || param->type->data.type.name == NULL)
            continue;
        if (strcmp(param->type->data.type.name, ret_name) == 0) {
            snprintf(buf, sizeof(buf), "%s",
                llvm_simple_expr_type_name(ctx, call->data.call.arguments[i]));
            return buf;
        }
    }

    return "Int";
}

static void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;
    const char *spawn_future_inner = NULL;

    /* Detect ClaimSlot / ClaimSecureSlot / ClaimDeviceSlot */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        if (strcmp(callee, "ClaimSlot") == 0
            || strcmp(callee, "ClaimSecureSlot") == 0) {
            /* Resolve inner type from type annotation */
            const char *inner = "Int";
            bool is_secure = (strcmp(callee, "ClaimSecureSlot") == 0);
            if (type_ann != NULL && type_ann->type == AST_TYPE) {
                /* Check for generic args: Slot<Int> */
                if (type_ann->data.type.generic_args != NULL
                    && type_ann->data.type.generic_args->count > 0)
                    inner = type_ann->data.type.generic_args->params[0]->name;
                else if (type_ann->data.type.name != NULL) {
                    /* Try to extract inner from type name like "Slot_Int" */
                    const char *tn = type_ann->data.type.name;
                    if (strncmp(tn, "Slot", 4) == 0)
                        inner = "Int"; /* default */
                }
            }

            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            /* Inline Claim: initialize the concrete slot storage directly.
             * SecureSlot also materializes a token bound to the owning alloca. */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                char token_name[256];
                snprintf(token_name, sizeof(token_name), "%s_token", name);
                LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

                LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                    alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
                LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                    LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                    llvm_tmp_name(ctx));

                LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);

                LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 0, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, token_id_ptr);

                LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_write_ptr);

                LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_read_ptr);

                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, is_secure);
            return;
        }
        if (strcmp(callee, "ClaimDeviceSlot") == 0) {
            const char *inner = "Int";
            if (type_ann != NULL && type_ann->type == AST_TYPE
                && type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0) {
                inner = type_ann->data.type.generic_args->params[0]->name;
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_device_slot_var(ctx, name, inner);
            return;
        }
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.arg_count >= 1
        && init->data.call.arguments[0] != NULL
        && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *ann_name = type_ann->data.type.name;
        const char *callee = init->data.call.callee->data.identifier.name;
        const char *source_name = init->data.call.arguments[0]->data.identifier.name;
        bool alias_decl =
            ((strcmp(ann_name, "ReadView") == 0 || strncmp(ann_name, "ReadView<", 9) == 0)
             && strcmp(callee, "ViewRead") == 0)
            || ((strcmp(ann_name, "WriteView") == 0 || strncmp(ann_name, "WriteView<", 10) == 0)
                && strcmp(callee, "ViewWrite") == 0)
            || ((strcmp(ann_name, "MoveToken") == 0 || strncmp(ann_name, "MoveToken<", 10) == 0)
                && strcmp(callee, "Move") == 0);
        if (alias_decl) {
            const char *inner = "Int";
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);

            LLVMVarEntry *source = llvm_scope_lookup(ctx, source_name);
            if (source == NULL)
                return;

            bool is_move = (strcmp(callee, "Move") == 0);

            if (is_move) {
                /* Move: structural copy — new alloca owns the data,
                 * source is invalidated */
                LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
                LLVMValueRef alloca_val = llvm_create_entry_alloca(
                    ctx, slot_ty, name);
                LLVMValueRef moved = LLVMBuildLoad2(ctx->builder,
                    source->type, source->alloca, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, moved, alloca_val);
                llvm_scope_declare(ctx, name, alloca_val, slot_ty);
                /* Mark source as consumed */
                for (int i = 0; i < ctx->slot_var_count; i++) {
                    if (strcmp(ctx->slot_vars[i].var_name, source_name) == 0) {
                        ctx->slot_vars[i].released = true;
                        break;
                    }
                }
            } else {
                /* ReadView / WriteView: non-owning alias —
                 * share the source slot's alloca directly.
                 * No separate storage; reads/writes go through
                 * the same address as the owning slot. */
                llvm_scope_declare(ctx, name, source->alloca, source->type);
            }
            llvm_register_view_var(ctx, name, source_name, inner, is_move);
            return;
        }
    }

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "ToObject") == 0
        && init->data.call.arg_count >= 2
        && init->data.call.arguments[1] != NULL
        && init->data.call.arguments[1]->type == AST_IDENTIFIER) {
        LLVMClassTypeEntry *target_cls = llvm_lookup_class(ctx, type_ann->data.type.name);
        if (target_cls != NULL
            && target_cls->is_immutable
            && !target_cls->is_boundary_transfer_contract) {
            const char *source_name = init->data.call.arguments[1]->data.identifier.name;
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
            llvm_register_projection_borrow(ctx, name, type_ann->data.type.name, source_name);
            return;
        }
    }

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *ann_name = type_ann->data.type.name;
        const char *callee = init->data.call.callee->data.identifier.name;
        const char *inner = "Int";

        if (type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);
        }

        if (strcmp(ann_name, "List") == 0 && strcmp(callee, "ListNew") == 0) {
            LLVMTypeRef list_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, list_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_list_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, list_ty);
            llvm_register_list_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "Set") == 0 && strcmp(callee, "SetNew") == 0) {
            LLVMTypeRef set_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, set_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, set_ty);
            llvm_register_set_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "Queue") == 0 && strcmp(callee, "QueueNew") == 0) {
            LLVMTypeRef queue_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, queue_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_queue_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, queue_ty);
            llvm_register_queue_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "HashMap") == 0 && strcmp(callee, "MapNew") == 0) {
            const char *value_type = "Int";
            LLVMTypeRef map_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef value_ty;
            LLVMValueRef alloca_val;
            LLVMFuncEntry *new_fn;

            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 1
                && type_ann->data.type.generic_args->params[1] != NULL) {
                value_type = llvm_stmt_render_type_arg(
                    type_ann->data.type.generic_args->params[1]);
            }
            value_ty = pergyra_type_to_llvm(ctx, value_type);
            alloca_val = llvm_create_entry_alloca(ctx, map_ty, name);
            new_fn = llvm_lookup_function(ctx, "pgy_map_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, map_ty);
            llvm_register_map_var(ctx, name, value_type);
            return;
        }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                           || strncmp(ann_name, "Slot<", 5) == 0);
        bool is_secure_slot_sugar = (strcmp(ann_name, "SecureSlot") == 0
                                  || strncmp(ann_name, "SecureSlot<", 11) == 0);
        if (is_slot_sugar || is_secure_slot_sugar) {
            const char *inner = "Int";
            bool is_secure = is_secure_slot_sugar;
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = type_ann->data.type.generic_args->params[0]->name;

            if (init != NULL && init->type == AST_IDENTIFIER) {
                LLVMViewVarEntry *move_entry = llvm_lookup_view_var(ctx,
                    init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    LLVMTypeRef slot_ty = is_secure
                        ? llvm_secure_slot_struct_type(ctx, inner)
                        : llvm_slot_struct_type(ctx, inner);
                    LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);
                    LLVMVarEntry *source = llvm_scope_lookup(ctx, init->data.identifier.name);
                    if (source == NULL)
                        return;
                    LLVMValueRef moved = LLVMBuildLoad2(ctx->builder, source->type, source->alloca,
                        llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, moved, alloca_val);
                    llvm_scope_declare(ctx, name, alloca_val, slot_ty);
                    llvm_register_slot_var(ctx, name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                        char token_name[256];
                        snprintf(token_name, sizeof(token_name), "%s_token", name);
                        LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                        LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
                    }
                    return;
                }
            }

            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            /* Inline Claim: zero-init + set claimed=true */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                char token_name[256];
                LLVMValueRef token_alloca;
                LLVMValueRef slot_ptr_i64;
                LLVMValueRef token_id;
                LLVMValueRef slot_token_ptr;
                LLVMValueRef token_id_ptr;
                LLVMValueRef token_write_ptr;
                LLVMValueRef token_read_ptr;

                snprintf(token_name, sizeof(token_name), "%s_token", name);
                token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

                slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                    alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
                token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                    LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                    llvm_tmp_name(ctx));

                slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);

                token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 0, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, token_id_ptr);

                token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_write_ptr);

                token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_read_ptr);

                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, is_secure);

            /* Auto Write the initializer value */
            if (init != NULL) {
                LLVMValueRef val = llvm_emit_expression(init, ctx);
                if (val != NULL) {
                    char fn_name[64];
                    snprintf(fn_name, sizeof(fn_name),
                        is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        if (is_secure) {
                            char token_name[256];
                            LLVMVarEntry *token_var;
                            snprintf(token_name, sizeof(token_name), "%s_token", name);
                            token_var = llvm_scope_lookup(ctx, token_name);
                            if (token_var != NULL) {
                                LLVMValueRef args[] = { alloca_val, val, token_var->alloca };
                                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                            }
                        } else {
                            LLVMValueRef args[] = { alloca_val, val };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        }
                    } else {
                        LLVMValueRef value_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 0, llvm_tmp_name(ctx));
                        LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, val, value_ptr);
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                            occ_ptr);
                        if (is_secure) {
                            LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                                slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder,
                                LLVMBuildLoad2(ctx->builder, ctx->type_i64, token_ptr,
                                    llvm_tmp_name(ctx)),
                                token_ptr);
                        }
                    }
                }
            }
            return;
        }
    }

    /* Detect class constructor: let v = ClassName(args...) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
        if (cls != NULL) {
            LLVMValueRef alloca_val = llvm_create_entry_alloca(
                ctx, cls->struct_type, name);
            LLVMValueRef init_val = llvm_emit_expression(init, ctx);
            if (init_val != NULL)
                LLVMBuildStore(ctx->builder, init_val, alloca_val);

            llvm_scope_declare(ctx, name, alloca_val, cls->struct_type);
            llvm_register_var_class(ctx, name, callee);
            return;
        }
    }

    /* Detect Channel constructor: let ch: Channel<Int> = Channel(capacity) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "Channel") == 0) {
        /* Allocate opaque channel as a large-enough byte array.
         * PgyChannel_Int_RT on the runtime side is ~128 bytes;
         * we allocate 256 bytes for safety. */
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        /* Call pgy_channel_init_Int(ptr, capacity) */
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
            "pgy_channel_init_Int");
        if (init_fn != NULL) {
            LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
            if (init->data.call.arg_count > 0)
                cap = LLVMBuildZExt(ctx->builder,
                    llvm_emit_expression(init->data.call.arguments[0], ctx),
                    ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef args[] = { alloca_val, cap };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 2, "");
        }
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            llvm_register_channel_var(ctx, name,
                type_ann->data.type.generic_args->params[0]->name);
        } else {
            llvm_register_channel_var(ctx, name, "Int");
        }
        return;
    }

    /* Array literal: let values: Array<Int> = [1, 2, 3] */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        size_t count = init->data.array_literal.count;
        LLVMTypeRef elem_type = ctx->type_i32;
        const char *inner_name = "Int";

        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.name != NULL
            && (strcmp(type_ann->data.type.name, "Array") == 0
                || strcmp(type_ann->data.type.name, "Slice") == 0)
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            inner_name = type_ann->data.type.generic_args->params[0]->name;
            elem_type = pergyra_type_to_llvm(
                ctx, inner_name);
        } else if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(
                init->data.array_literal.elements[0], ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, array_type, name);
        char new_fn_name[64];
        char push_fn_name[64];
        LLVMFuncEntry *new_fn;
        LLVMFuncEntry *push_fn;

        snprintf(new_fn_name, sizeof(new_fn_name), "pgy_array_new_%s", inner_name);
        new_fn = llvm_lookup_function(ctx, new_fn_name);
        if (new_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
            };
            LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
                new_fn->fn, args, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, arr_val, var_alloca);
        }

        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        push_fn = llvm_lookup_function(ctx, push_fn_name);

        for (size_t i = 0; i < count; i++) {
            LLVMValueRef element = llvm_emit_expression(
                init->data.array_literal.elements[i], ctx);
            if (element != NULL && LLVMTypeOf(element) != elem_type) {
                LLVMTypeRef element_type = LLVMTypeOf(element);
                bool target_is_int = (elem_type == ctx->type_i32
                                   || elem_type == ctx->type_i64);
                bool target_is_fp = (elem_type == ctx->type_f32
                                  || elem_type == ctx->type_f64);
                bool source_is_int = (element_type == ctx->type_i32
                                   || element_type == ctx->type_i64);
                bool source_is_fp = (element_type == ctx->type_f32
                                  || element_type == ctx->type_f64);

                if (target_is_int && source_is_fp)
                    element = LLVMBuildFPToSI(ctx->builder, element, elem_type,
                                              llvm_tmp_name(ctx));
                else if (target_is_fp && source_is_int)
                    element = LLVMBuildSIToFP(ctx->builder, element, elem_type,
                                              llvm_tmp_name(ctx));
            }
            if (push_fn != NULL && element != NULL) {
                LLVMValueRef args[] = { var_alloca, element };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type,
                    push_fn->fn, args, 2, "");
            }
        }

        llvm_scope_declare(ctx, name, var_alloca, array_type);
        llvm_register_array_var(ctx, name, elem_type, (int64_t)count);
        return;
    }

    /* Determine type from annotation or initializer */
    LLVMTypeRef var_type = ctx->type_i32; /* default */
    if (type_ann != NULL)
        var_type = ast_type_to_llvm(ctx, type_ann);
    else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        var_type = ctx->type_task_handle;
        spawn_future_inner = llvm_infer_spawn_future_inner(ctx, init);
    } else if (init != NULL) {
        var_type = llvm_stmt_infer_expr_type(ctx, init);
    }
    if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        LLVMTypeRef lambda_type = llvm_stmt_lambda_signature_type(ctx, init);
        if (lambda_type != NULL)
            var_type = lambda_type;
    }

    /* Create alloca at function entry */
    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, var_type, name);

    /* Store initializer if present */
    if (init != NULL) {
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (val != NULL) {
            LLVMTypeRef val_type = LLVMTypeOf(val);

            /* Type coercion between numeric types */
            if (var_type != val_type) {
                bool var_is_int = (var_type == ctx->type_i32 || var_type == ctx->type_i64);
                bool var_is_fp  = (var_type == ctx->type_f32 || var_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (var_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(var_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_fp)
                    val = (var_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
            }

            LLVMBuildStore(ctx->builder, val, alloca);
        }
    }

    llvm_scope_declare(ctx, name, alloca, var_type);

    if (type_ann != NULL && type_ann->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, name, type_ann);
    } else if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        ASTNode *handler_type = ast_create_event_handler_type();
        handler_type->data.event_handler_type.param_count =
            init->data.lambda_expr.param_count;
        if (init->data.lambda_expr.param_count > 0) {
            handler_type->data.event_handler_type.param_types = calloc(
                init->data.lambda_expr.param_count, sizeof(ASTNode *));
            for (size_t i = 0; i < init->data.lambda_expr.param_count; i++) {
                ASTNode *p = init->data.lambda_expr.params[i];
                handler_type->data.event_handler_type.param_types[i] =
                    (p != NULL && p->type == AST_LET_DECL)
                    ? p->data.let_decl.type : NULL;
            }
        }
        handler_type->data.event_handler_type.return_type =
            init->data.lambda_expr.return_type;
        llvm_register_callable_var(ctx, name, handler_type);
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && init->data.identifier.name != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            init->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            ASTNode *handler_type = ast_create_event_handler_type();
            handler_type->data.event_handler_type.param_count =
                decl->data.func_decl.param_count;
            if (decl->data.func_decl.param_count > 0) {
                handler_type->data.event_handler_type.param_types = calloc(
                    decl->data.func_decl.param_count, sizeof(ASTNode *));
                for (size_t i = 0; i < decl->data.func_decl.param_count; i++) {
                    FuncParam *p = decl->data.func_decl.params[i];
                    handler_type->data.event_handler_type.param_types[i] =
                        p != NULL ? p->type : NULL;
                }
            }
            handler_type->data.event_handler_type.return_type =
                decl->data.func_decl.return_type;
            llvm_register_callable_var(ctx, name, handler_type);
        }
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && init->data.call.callee->data.identifier.name != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            init->data.call.callee->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && decl->data.func_decl.return_type != NULL
            && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
            llvm_register_callable_var(ctx, name, decl->data.func_decl.return_type);
        }
    }

    {
        LLVMClassTypeEntry *value_cls = llvm_stmt_lookup_class_by_type(ctx, var_type);
        if (value_cls != NULL)
            llvm_register_var_class(ctx, name, value_cls->class_name);
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *ann_cls = llvm_lookup_class(ctx, type_ann->data.type.name);
        if (ann_cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    } else if (init != NULL) {
        const char *inferred_nominal = llvm_stmt_infer_nominal_name_from_init(ctx, init);
        LLVMClassTypeEntry *inferred_cls = inferred_nominal != NULL
            ? llvm_lookup_class(ctx, inferred_nominal) : NULL;
        if (inferred_cls != NULL)
            llvm_register_var_class(ctx, name, inferred_nominal);
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && (strcmp(type_ann->data.type.name, "Array") == 0
            || strcmp(type_ann->data.type.name, "Slice") == 0)
        && type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0) {
        char *elem_name = llvm_stmt_render_type_arg(
            type_ann->data.type.generic_args->params[0]);
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        llvm_register_array_var(ctx, name, elem_type, -1);
        free(elem_name);
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        if (strcmp(ann_name, "List") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            char *inner_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            llvm_register_list_var(ctx, name, inner_name);
        } else if (strcmp(ann_name, "Queue") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            char *inner_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            llvm_register_queue_var(ctx, name, inner_name);
        } else if (strcmp(ann_name, "HashMap") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 1
            && type_ann->data.type.generic_args->params[1] != NULL) {
            char *value_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[1]);
            llvm_register_map_var(ctx, name, value_name);
        }
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0) {
        const char *ann_name = type_ann->data.type.name;
        if (strcmp(ann_name, "Future") == 0
            || strcmp(ann_name, "RemoteFuture") == 0) {
            llvm_register_future_var(ctx, name,
                type_ann->data.type.generic_args->params[0]->name,
                strcmp(ann_name, "RemoteFuture") == 0);
        }
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && strcmp(init->data.call.callee->data.identifier.name,
                         "SubmitDeviceRead") == 0) {
        const char *inner = "Int";
        ASTNode *slot_arg = init->data.call.arguments[0];
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            const char *tracked = llvm_lookup_device_slot_inner(
                ctx, slot_arg->data.identifier.name);
            if (tracked != NULL)
                inner = tracked;
        }
        llvm_register_future_var(ctx, name, inner, true);
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        llvm_register_future_var(ctx, name,
            spawn_future_inner != NULL ? spawn_future_inner : "Int",
            false);
    }

    /* Track class type for member access */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx,
            type_ann->data.type.name);
        if (cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    } else if (init != NULL) {
        const char *nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, init);
        LLVMClassTypeEntry *nominal_cls = nominal_name != NULL
            ? llvm_lookup_class(ctx, nominal_name) : NULL;
        if (nominal_cls != NULL) {
            llvm_register_var_class(ctx, name, nominal_name);
        }
    }
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_emit_defers_from(ctx, 0);

    if (node->data.return_stmt.value != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.return_stmt.value,
                                                 ctx);
        if (val != NULL) {
            /* Coerce to expected return type */
            LLVMTypeRef val_type = LLVMTypeOf(val);
            LLVMTypeRef ret_type = ctx->current_ret_type;
            if (ret_type != val_type && ret_type != ctx->type_void) {
                bool ret_is_int = (ret_type == ctx->type_i32 || ret_type == ctx->type_i64);
                bool ret_is_fp  = (ret_type == ctx->type_f32 || ret_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (ret_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(ret_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_fp)
                    val = (ret_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
            }
            LLVMBuildRet(ctx->builder, val);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
        }
    } else {
        if (ctx->current_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ctx->current_ret_type, 0, 0));
    }
}

static void
llvm_emit_if_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef cond = llvm_emit_expression(node->data.if_stmt.condition, ctx);
    if (cond == NULL)
        return;

    /* Ensure cond is i1 */
    if (LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "then");
    LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "else");
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "ifcont");

    LLVMBuildCondBr(ctx->builder, cond, then_bb, else_bb);

    /* Then block */
    LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
    if (node->data.if_stmt.then_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.then_branch, ctx);
    /* Only branch to merge if no terminator (return) was emitted */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Else block */
    LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
    if (node->data.if_stmt.else_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.else_branch, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Merge */
    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.body");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef cond = llvm_emit_expression(node->data.while_loop.condition,
                                              ctx);
    if (cond != NULL && LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));
    if (cond == NULL)
        cond = LLVMConstInt(ctx->type_i1, 0, 0);

    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_continue_blocks[ctx->loop_depth] = cond_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (node->data.while_loop.body != NULL)
        llvm_emit_statement(node->data.while_loop.body, ctx);
    if (ctx->loop_depth > 0)
        ctx->loop_depth--;
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
}

static void
llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *var_name = node->data.for_loop.variable;

    if (node->data.for_loop.iterable != NULL) {
        if (node->data.for_loop.iterable->type == AST_IDENTIFIER) {
            const char *iter_name = node->data.for_loop.iterable->data.identifier.name;
            const char *list_inner = llvm_lookup_list_inner(ctx, iter_name);
            LLVMVarEntry *list_var = llvm_scope_lookup(ctx, iter_name);
            if (list_inner != NULL && list_var != NULL) {
                LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, list_inner);
                LLVMValueRef idx_alloca;
                LLVMValueRef fn = ctx->current_function;
                LLVMBasicBlockRef cond_bb;
                LLVMBasicBlockRef body_bb;
                LLVMBasicBlockRef incr_bb;
                LLVMBasicBlockRef exit_bb;
                LLVMFuncEntry *size_fn = llvm_lookup_function(ctx, "pgy_list_size_raw_export");
                LLVMFuncEntry *get_fn = llvm_lookup_function(ctx, "pgy_list_get_raw_export");

                llvm_scope_push(ctx);
                idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), idx_alloca);
                {
                    LLVMValueRef item_alloca = llvm_create_entry_alloca(ctx, elem_ty, var_name);
                    llvm_scope_declare(ctx, var_name, item_alloca, elem_ty);
                    {
                        LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
                        if (cls != NULL)
                            llvm_register_var_class(ctx, var_name, cls->class_name);
                    }
                }

                cond_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.cond");
                body_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.body");
                incr_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.incr");
                exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.exit");
                LLVMBuildBr(ctx->builder, cond_bb);

                LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMValueRef size_call = LLVMConstInt(ctx->type_i32, 0, 0);
                    if (size_fn != NULL) {
                        LLVMValueRef args[] = {
                            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
                        };
                        size_call = LLVMBuildCall2(ctx->builder, size_fn->fn_type, size_fn->fn, args, 1, llvm_tmp_name(ctx));
                    }
                    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, idx, size_call, llvm_tmp_name(ctx));
                    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);
                }

                LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMVarEntry *loop_var = llvm_scope_lookup(ctx, var_name);
                    if (get_fn != NULL && loop_var != NULL) {
                        LLVMValueRef args[] = {
                            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                            idx,
                            LLVMBuildBitCast(ctx->builder, loop_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                            llvm_sizeof_type_i64(ctx, elem_ty)
                        };
                        LLVMBuildCall2(ctx->builder, get_fn->fn_type, get_fn->fn, args, 4, "");
                    }
                }
                if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
                    ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
                    ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
                    ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
                    ctx->loop_depth++;
                }
                if (node->data.for_loop.body != NULL)
                    llvm_emit_statement(node->data.for_loop.body, ctx);
                if (ctx->loop_depth > 0)
                    ctx->loop_depth--;
                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                    LLVMBuildBr(ctx->builder, incr_bb);

                LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMValueRef next = LLVMBuildAdd(ctx->builder, idx, LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, next, idx_alloca);
                    LLVMBuildBr(ctx->builder, cond_bb);
                }

                LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
                llvm_scope_pop(ctx);
                return;
            }
        }

        LLVMValueRef iterable = llvm_emit_expression(node->data.for_loop.iterable, ctx);
        LLVMTypeRef iterable_ty;
        LLVMTypeRef field_types[5];
        LLVMTypeRef elem_ty;
        LLVMValueRef data_ptr;
        LLVMValueRef count64;
        LLVMValueRef idx_alloca;
        LLVMValueRef fn;
        LLVMBasicBlockRef cond_bb;
        LLVMBasicBlockRef body_bb;
        LLVMBasicBlockRef incr_bb;
        LLVMBasicBlockRef exit_bb;

        if (iterable == NULL)
            return;
        iterable_ty = LLVMTypeOf(iterable);
        if (LLVMGetTypeKind(iterable_ty) != LLVMStructTypeKind
            || LLVMCountStructElementTypes(iterable_ty) < 2) {
            return;
        }

        LLVMGetStructElementTypes(iterable_ty, field_types);
        elem_ty = LLVMGetElementType(field_types[0]);
        data_ptr = LLVMBuildExtractValue(ctx->builder, iterable, 0, llvm_tmp_name(ctx));
        count64 = LLVMBuildExtractValue(ctx->builder, iterable, 1, llvm_tmp_name(ctx));

        llvm_scope_push(ctx);
        idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i64, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i64, 0, 0), idx_alloca);

        {
            LLVMValueRef item_alloca = llvm_create_entry_alloca(ctx, elem_ty, var_name);
            llvm_scope_declare(ctx, var_name, item_alloca, elem_ty);
            {
                LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
                if (cls != NULL)
                    llvm_register_var_class(ctx, var_name, cls->class_name);
            }
        }

        fn = ctx->current_function;
        cond_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.cond");
        body_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.body");
        incr_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.incr");
        exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.exit");

        LLVMBuildBr(ctx->builder, cond_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntULT, idx, count64, llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef item_ptr = LLVMBuildGEP2(ctx->builder, elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            LLVMValueRef item = LLVMBuildLoad2(ctx->builder, elem_ty, item_ptr, llvm_tmp_name(ctx));
            LLVMVarEntry *loop_var = llvm_scope_lookup(ctx, var_name);
            if (loop_var != NULL)
                LLVMBuildStore(ctx->builder, item, loop_var->alloca);
        }
        if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
            ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
            ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
            ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
            ctx->loop_depth++;
        }
        if (node->data.for_loop.body != NULL)
            llvm_emit_statement(node->data.for_loop.body, ctx);
        if (ctx->loop_depth > 0)
            ctx->loop_depth--;
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, incr_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef next = LLVMBuildAdd(ctx->builder, idx,
                LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, next, idx_alloca);
            LLVMBuildBr(ctx->builder, cond_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
        llvm_scope_pop(ctx);
        return;
    }

    llvm_scope_push(ctx);

    /* Create loop variable */
    LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                        var_name);
    LLVMValueRef start = llvm_emit_expression(node->data.for_loop.range_start,
                                               ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, var_name, var_alloca, ctx->type_i32);

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition: i < end */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                           var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef end = llvm_emit_expression(node->data.for_loop.range_end, ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                                       llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (node->data.for_loop.body != NULL)
        llvm_emit_statement(node->data.for_loop.body, ctx);
    if (ctx->loop_depth > 0)
        ctx->loop_depth--;
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, incr_bb);

    /* Increment: i = i + 1 */
    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef cur2 = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                        var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, cur2,
                                      LLVMConstInt(ctx->type_i32, 1, 0),
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, var_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);

    llvm_scope_pop(ctx);
}

static void
llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef subject = llvm_emit_expression(node->data.match_stmt.subject,
                                                 ctx);
    if (subject == NULL)
        return;

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "match.end");

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;

        const char *option_kind = NULL;
        const char *option_binding = NULL;
        LLVMValueRef cmp = NULL;

        if (mc->data.match_case.patterns != NULL
            && mc->data.match_case.pattern_count > 1) {
            for (size_t p = 0; p < mc->data.match_case.pattern_count; p++) {
                LLVMValueRef pattern = llvm_emit_expression(
                    mc->data.match_case.patterns[p], ctx);
                LLVMValueRef alt_cmp;
                if (pattern == NULL)
                    continue;
                alt_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                        subject, pattern,
                                        llvm_tmp_name(ctx));
                cmp = (cmp == NULL)
                    ? alt_cmp
                    : LLVMBuildOr(ctx->builder, cmp, alt_cmp, llvm_tmp_name(ctx));
            }
            if (cmp == NULL)
                continue;
        } else if (llvm_is_option_destructor(mc->data.match_case.pattern,
                                      &option_kind, &option_binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(option_kind, "Some") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        } else {
            LLVMValueRef pattern = llvm_emit_expression(mc->data.match_case.pattern,
                                                         ctx);
            if (pattern == NULL)
                continue;
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                subject, pattern,
                                llvm_tmp_name(ctx));
        }

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.case");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.next");

        LLVMBuildCondBr(ctx->builder, cmp, case_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        llvm_scope_push(ctx);
        if (option_binding != NULL) {
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, subject, 1,
                llvm_tmp_name(ctx));
            LLVMTypeRef payload_ty = LLVMTypeOf(payload);
            LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx, payload_ty,
                option_binding);
            LLVMBuildStore(ctx->builder, payload, payload_alloca);
            llvm_scope_declare(ctx, pergyra_strdup(option_binding),
                payload_alloca, payload_ty);
        }
        if (mc->data.match_case.body != NULL)
            llvm_emit_statement(mc->data.match_case.body, ctx);
        llvm_scope_pop(ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    /* Default case */
    if (node->data.match_stmt.default_body != NULL) {
        llvm_emit_statement(node->data.match_stmt.default_body, ctx);
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL
        && node->data.with_stmt.slot_type->type == AST_TYPE
        && node->data.with_stmt.slot_type->data.type.name != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    LLVMTypeRef slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, alias);

    /* Inline claim: zero-init + set claimed=true (avoids ABI mismatch) */
    char fn_name[64];
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    /* Push scope, register slot variable */
    llvm_scope_push(ctx);
    if (is_secure) {
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        char token_name[256];
        snprintf(token_name, sizeof(token_name), "%s_token", alias);
        LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
        LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

        LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
            alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
        LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
            LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
            llvm_tmp_name(ctx));
        LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
            slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
        LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 0, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
        LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_write_ptr);
        LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
            token_ty, token_alloca, 2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_read_ptr);
        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
    }
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner, is_secure);

    /* Emit body */
    if (node->data.with_stmt.body != NULL)
        llvm_emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        snprintf(fn_name, sizeof(fn_name), is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *release_fn = llvm_lookup_function(ctx, fn_name);
        if (release_fn != NULL) {
            if (is_secure) {
                LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, alias);
                if (token_var != NULL) {
                    LLVMValueRef args[] = { alloca_val, token_var->alloca };
                    LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                                   release_fn->fn, args, 2, "");
                }
            } else {
                LLVMValueRef args[] = { alloca_val };
                LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                               release_fn->fn, args, 1, "");
            }
        } else if (is_secure) {
            LLVMValueRef occupied_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                occupied_ptr);
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
        }
    }

    llvm_scope_pop(ctx);
}

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    int saved_slot_count = ctx->slot_var_count;
    llvm_defer_scope_push(ctx);
    llvm_scope_push(ctx);
    for (size_t i = 0; i < node->data.block.count; i++) {
        llvm_emit_statement(node->data.block.statements[i], ctx);
        /* Stop emitting after a terminator (return) */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }

    /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
     * Skip slots already explicitly released by the user. */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        llvm_emit_defers_from(ctx, ctx->defer_scope_depth - 1);
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            if (ctx->slot_vars[i].released) continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            char fn_name[64];
            bool is_secure = ctx->slot_vars[i].is_secure;
            snprintf(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, vname);
            if (fn != NULL && var != NULL) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, vname);
                    if (token_var != NULL) {
                        LLVMValueRef args[] = { var->alloca, token_var->alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    }
                } else {
                    LLVMValueRef args[] = { var->alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                }
            } else if (is_secure && var != NULL) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 1, llvm_tmp_name(ctx));
                LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
            } else if (!is_secure && var != NULL) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
            }
        }
    }

    ctx->slot_var_count = saved_slot_count;
    llvm_scope_pop(ctx);
    llvm_defer_scope_pop(ctx);
}

/* =================================================================
 * Parallel block — real concurrency via thread pool
 *
 * For each task, generate an LLVM function `_pgy_par_N(i8*) -> i8*`
 * that contains the task body, then spawn all + await all.
 * ================================================================= */

static void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    /* -----------------------------------------------------------
     * 1) Collect all variables from the current scope stack.
     *    These will be captured into a context struct so that
     *    wrapper functions can access them.
     * ----------------------------------------------------------- */
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }

    /* -----------------------------------------------------------
     * 2) Build a context struct type: { ptr, ptr, ... }
     *    Each field is a pointer to the captured variable's alloca.
     *    In the wrapper, we GEP to get the pointer, then load/store
     *    through it — exactly like the C transpiler's approach.
     * ----------------------------------------------------------- */
    LLVMTypeRef *ctx_fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
    for (int i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;   /* all fields are opaque ptr */

    char ctx_name[64];
    snprintf(ctx_name, sizeof(ctx_name), "_pgy_par_ctx_%d", ctx->parallel_counter);
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);
    free(ctx_fields);

    /* -----------------------------------------------------------
     * 3) In the OUTER function: allocate + fill the context struct.
     * ----------------------------------------------------------- */
    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (int i = 0; i < n_captured; i++) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                                                 ctx_alloca, (unsigned)i,
                                                 llvm_tmp_name(ctx));
        /* Store the alloca address (pointer to the variable) */
        LLVMBuildStore(ctx->builder, captured[i].alloca, gep);
    }

    /* Cast context struct pointer to i8* for spawn argument */
    LLVMValueRef ctx_i8ptr = LLVMBuildBitCast(ctx->builder, ctx_alloca,
                                               ctx->type_i8ptr,
                                               llvm_tmp_name(ctx));

    /* -----------------------------------------------------------
     * 4) Generate wrapper functions for each parallel task.
     *    Each wrapper receives the context struct as i8* arg,
     *    casts it back, and GEPs to access captured variable pointers.
     * ----------------------------------------------------------- */
    LLVMValueRef    saved_fn  = ctx->current_function;
    LLVMTypeRef     saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns = calloc(count, sizeof(LLVMValueRef));

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "_pgy_par_%d_%zu",
                 ctx->parallel_counter, i);

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;

        llvm_scope_push(ctx);

        /* Cast arg (i8*) back to context struct pointer */
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        /* For each captured variable: GEP → load pointer → declare in scope.
         * The loaded pointer points to the original alloca, so
         * load/store through it accesses the outer variable. */
        for (int c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            /* Declare in wrapper scope — the "alloca" is actually the
             * loaded pointer to the outer function's alloca.  Since
             * llvm_emit_identifier does Load2(type, alloca, ...) and
             * store operations do Store(val, alloca), this transparent
             * pointer indirection works correctly. */
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
        }

        /* Emit the task body */
        llvm_emit_statement(node->data.parallel.tasks[i], ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
    }

    ctx->parallel_counter++;

    /* Restore insertion point */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    /* -----------------------------------------------------------
     * 5) Spawn all tasks, await all.
     * ----------------------------------------------------------- */
    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_spawn_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");

    if (spawn_fn == NULL || await_fn == NULL) {
        /* Fallback: emit sequentially */
        for (size_t i = 0; i < count; i++)
            llvm_emit_statement(node->data.parallel.tasks[i], ctx);
        free(wrapper_fns);
        return;
    }

    LLVMValueRef *handles = calloc(count, sizeof(LLVMValueRef));
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = { fn_ptr, ctx_i8ptr };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 2,
                                     llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }

    free(handles);
    free(wrapper_fns);
}

static bool
llvm_select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                       const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

static void
llvm_emit_async_block(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode fake_block = {0};
    fake_block.type = AST_BLOCK;
    fake_block.data.block.statements = node->data.async_block.statements;
    fake_block.data.block.count = node->data.async_block.statement_count;

    LLVMValueRef saved_fn  = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    /* Reuse parallel wrapper generation, but spawn via async runtime and detach. */
    int saved_parallel_counter = ctx->parallel_counter;
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;
    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }
    bool has_captures = n_captured > 0;
    LLVMValueRef ctx_alloca = NULL;
    LLVMTypeRef ctx_struct_type = NULL;

    if (has_captures) {
        LLVMTypeRef *fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
        for (int i = 0; i < n_captured; i++)
            fields[i] = ctx->type_i8ptr;
        ctx_struct_type = LLVMStructCreateNamed(ctx->context, llvm_tmp_name(ctx));
        LLVMStructSetBody(ctx_struct_type, fields, (unsigned)n_captured, 0);
        free(fields);

        ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type, "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_alloca, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef cast = LLVMBuildBitCast(ctx->builder, captured[i].alloca,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast, gep);
        }
    }

    LLVMValueRef ctx_i8ptr = has_captures
        ? LLVMBuildBitCast(ctx->builder, ctx_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
        : LLVMConstNull(ctx->type_i8ptr);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "_pgy_async_%d_0", ctx->parallel_counter++);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    llvm_scope_push(ctx);
    if (has_captures) {
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_ptr, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr_i8 = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                field_ptr, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildBitCast(ctx->builder, var_ptr_i8,
                LLVMPointerType(captured[i].type, 0), llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[i].name, var_ptr, captured[i].type);
        }
    }
    llvm_emit_statement(&fake_block, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    llvm_scope_pop(ctx);

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_async_spawn_export");
    LLVMFuncEntry *detach_fn = llvm_lookup_function(ctx, "pgy_async_detach_export");
    if (spawn_fn == NULL || detach_fn == NULL) {
        ctx->parallel_counter = saved_parallel_counter;
        llvm_emit_statement(&fake_block, ctx);
        return;
    }

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, fn, ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = { fn_ptr, ctx_i8ptr };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 2, llvm_tmp_name(ctx));
    LLVMValueRef detach_args[] = { handle };
    LLVMBuildCall2(ctx->builder, detach_fn->fn_type, detach_fn->fn, detach_args, 1, "");
}

static void
llvm_emit_select_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "select.end");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            llvm_emit_statement(node->data.select_stmt.default_case, ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        return;
    }

    {
        int select_id = ctx->tmp_counter++;
        char rr_name[64];
        snprintf(rr_name, sizeof(rr_name), "__pgy_select_rr_%d", select_id);

        LLVMValueRef rr_global = LLVMAddGlobal(ctx->module, ctx->type_i32, rr_name);
        LLVMSetInitializer(rr_global, LLVMConstInt(ctx->type_i32, 0, 0));
        LLVMSetLinkage(rr_global, LLVMInternalLinkage);

        LLVMValueRef rr_cur = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            rr_global, llvm_tmp_name(ctx));
        LLVMValueRef rr_next = LLVMBuildAdd(ctx->builder, rr_cur,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, rr_next, rr_global);

        LLVMValueRef start = LLVMBuildURem(ctx->builder, rr_cur,
            LLVMConstInt(ctx->type_i32, (unsigned long long)case_count, 0),
            llvm_tmp_name(ctx));

        LLVMBasicBlockRef default_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "select.default");
        LLVMBasicBlockRef *rotation_bbs = calloc(case_count, sizeof(LLVMBasicBlockRef));
        for (size_t i = 0; i < case_count; i++) {
            rotation_bbs[i] = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "select.rotation");
        }

        LLVMValueRef dispatch = LLVMBuildSwitch(ctx->builder, start,
            rotation_bbs[0], (unsigned)(case_count > 0 ? case_count - 1 : 0));
        for (size_t i = 1; i < case_count; i++) {
            LLVMAddCase(dispatch, LLVMConstInt(ctx->type_i32,
                (unsigned long long)i, 0), rotation_bbs[i]);
        }

        for (size_t start_idx = 0; start_idx < case_count; start_idx++) {
            LLVMBasicBlockRef next_check_bb = NULL;
            LLVMPositionBuilderAtEnd(ctx->builder, rotation_bbs[start_idx]);

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start_idx + offset) % case_count;
                ASTNode *case_node = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = llvm_select_case_parts(case_node, &channel, &bind_name, &body);

                LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.case");
                LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.next");

                if (next_check_bb != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *channel_name = channel->data.identifier.name;
                    const char *inner = llvm_lookup_channel_inner(ctx, channel_name);
                    LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, channel_name);
                    if (inner == NULL) inner = "Int";

                    if (ch_var != NULL) {
                        char fn_name[128];
                        if (bind_name != NULL) {
                            LLVMTypeRef val_ty = pergyra_type_to_llvm(ctx, inner);
                            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, val_ty, llvm_tmp_name(ctx));
                            snprintf(fn_name, sizeof(fn_name), "pgy_channel_try_recv_%s", inner);
                            LLVMFuncEntry *try_fn = llvm_lookup_function(ctx, fn_name);
                            if (try_fn != NULL) {
                                LLVMValueRef args[] = { ch_var->alloca, tmp };
                                LLVMValueRef ok = LLVMBuildCall2(ctx->builder, try_fn->fn_type,
                                    try_fn->fn, args, 2, llvm_tmp_name(ctx));
                                LLVMBuildCondBr(ctx->builder, ok, case_bb, fail_bb);

                                LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                                llvm_scope_push(ctx);
                                {
                                    LLVMValueRef bind_alloca =
                                        llvm_create_entry_alloca(ctx, val_ty, bind_name);
                                    LLVMValueRef received = LLVMBuildLoad2(ctx->builder, val_ty, tmp,
                                        llvm_tmp_name(ctx));
                                    LLVMBuildStore(ctx->builder, received, bind_alloca);
                                    llvm_scope_declare(ctx, pergyra_strdup(bind_name),
                                                       bind_alloca, val_ty);
                                }
                                if (body != NULL)
                                    llvm_emit_statement(body, ctx);
                                llvm_scope_pop(ctx);
                                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                                    LLVMBuildBr(ctx->builder, merge_bb);
                                next_check_bb = fail_bb;
                                continue;
                            }
                        } else {
                            snprintf(fn_name, sizeof(fn_name), "pgy_channel_ready_%s", inner);
                            LLVMFuncEntry *ready_fn = llvm_lookup_function(ctx, fn_name);
                            if (ready_fn != NULL) {
                                LLVMValueRef args[] = { ch_var->alloca };
                                LLVMValueRef ready = LLVMBuildCall2(ctx->builder, ready_fn->fn_type,
                                    ready_fn->fn, args, 1, llvm_tmp_name(ctx));
                                LLVMBuildCondBr(ctx->builder, ready, case_bb, fail_bb);

                                LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                                {
                                    char recv_name[128];
                                    snprintf(recv_name, sizeof(recv_name), "pgy_channel_recv_val_%s", inner);
                                    LLVMFuncEntry *recv_fn = llvm_lookup_function(ctx, recv_name);
                                    if (recv_fn != NULL) {
                                        LLVMValueRef recv_args[] = { ch_var->alloca };
                                        (void)LLVMBuildCall2(ctx->builder, recv_fn->fn_type,
                                            recv_fn->fn, recv_args, 1, "");
                                    }
                                }
                                if (body != NULL)
                                    llvm_emit_statement(body, ctx);
                                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                                    LLVMBuildBr(ctx->builder, merge_bb);
                                next_check_bb = fail_bb;
                                continue;
                            }
                        }
                    }
                }

                LLVMBuildBr(ctx->builder, case_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                if (case_node != NULL)
                    llvm_emit_statement(case_node, ctx);
                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                    LLVMBuildBr(ctx->builder, merge_bb);
                next_check_bb = fail_bb;
            }

            if (next_check_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);
            LLVMBuildBr(ctx->builder, default_bb);
        }

        free(rotation_bbs);
        LLVMPositionBuilderAtEnd(ctx->builder, default_bb);
    }

    if (node->data.select_stmt.default_case != NULL)
        llvm_emit_statement(node->data.select_stmt.default_case, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

void
llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    /* If current block already has a terminator, skip */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        llvm_emit_let_decl(node, ctx);
        break;

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_BREAK:
        if (ctx->loop_depth > 0) {
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[ctx->loop_depth - 1]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_break_blocks[ctx->loop_depth - 1]);
        }
        break;
    case AST_ENUM_DECL:
        /* Enums are compile-time only — no IR needed */
        break;
    case AST_CONTINUE:
        if (ctx->loop_depth > 0) {
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[ctx->loop_depth - 1]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_continue_blocks[ctx->loop_depth - 1]);
        }
        break;

    case AST_IF_STMT:
        llvm_emit_if_stmt(node, ctx);
        break;

    case AST_WHILE_LOOP:
        llvm_emit_while_loop(node, ctx);
        break;

    case AST_FOR_LOOP:
        llvm_emit_for_loop(node, ctx);
        break;

    case AST_MATCH_STMT:
        llvm_emit_match_stmt(node, ctx);
        break;

    case AST_WITH_STMT:
        llvm_emit_with_stmt(node, ctx);
        break;

    case AST_BLOCK:
        llvm_emit_block(node, ctx);
        break;

    case AST_ASYNC_BLOCK:
        llvm_emit_async_block(node, ctx);
        break;

    case AST_PARALLEL_BLOCK:
        llvm_emit_parallel_block(node, ctx);
        break;

    case AST_SELECT_STMT:
        llvm_emit_select_stmt(node, ctx);
        break;

    case AST_FUNC_DECL:
    case AST_CLASS_DECL:
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
    case AST_EVENT_DECL:
    case AST_IMPORT_DECL:
    case AST_NAMESPACE_DECL:
        /* Handled in program pass or declaration-only — skip here */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } — handled in program pass (Pass 0) */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } — emit body directly, no safety wrappers */
        if (node->data.unsafe_block.body != NULL)
            llvm_emit_block(node->data.unsafe_block.body, ctx);
        break;

    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            llvm_register_defer(node->data.defer_stmt.body, ctx);
        break;

    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → party_var.slot_vtable = &Role_Ability_vtable_instance */
        const char *party_var  = node->data.bind_stmt.party_var;
        const char *slot_name  = node->data.bind_stmt.slot_name;
        const char *role_name  = node->data.bind_stmt.role_name;

        if (party_var == NULL || slot_name == NULL || role_name == NULL)
            break;

        /* Look up the party variable */
        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
        if (pvar == NULL) break;

        const char *party_class_name = llvm_lookup_var_class(ctx, party_var);
        LLVMClassTypeEntry *cls = party_class_name
            ? llvm_lookup_class(ctx, party_class_name) : NULL;
        if (cls == NULL) break;

        char vt_field[256];
        snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
        int field_idx = -1;
        for (int fi = 0; fi < cls->field_count; fi++) {
            if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                field_idx = cls->fields[fi].index;
                break;
            }
        }
        if (field_idx < 0) break;

        /* Find the Role's vtable global.
         * Convention: RoleName_AbilityName_vtable_instance */
        char global_prefix[256];
        snprintf(global_prefix, sizeof(global_prefix), "%s_", role_name);
        LLVMValueRef vt_global = NULL;
        LLVMValueRef g = LLVMGetFirstGlobal(ctx->module);
        while (g != NULL) {
            const char *gname = LLVMGetValueName(g);
            if (gname != NULL
                && strncmp(gname, global_prefix, strlen(global_prefix)) == 0
                && strstr(gname, "_vtable_instance") != NULL) {
                vt_global = g;
                break;
            }
            g = LLVMGetNextGlobal(g);
        }
        if (vt_global == NULL) break;

        /* GEP to vtable pointer field + store */
        LLVMValueRef party_alloca = pvar->alloca;
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
            cls->struct_type, party_alloca, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMValueRef vt_ptr = LLVMBuildBitCast(ctx->builder,
            vt_global, ctx->type_i8ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, vt_ptr, field_ptr);
        break;
    }

    /* Expression statements */
    case AST_CALL:
    case AST_ASSIGNMENT:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_ARRAY_ACCESS:
    case AST_PARTY_INSTANCE:
    case AST_CONTEXT_ACCESS:
    case AST_TASK_GROUP:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
        llvm_emit_expression(node, ctx);
        if (node->type == AST_CALL)
            llvm_stmt_emit_zone_action_effect_runtime(node, ctx);
        break;

    default:
        fprintf(stderr, "[llvm] warning: unhandled statement AST type %d\n",
                (int)node->type);
        break;
    }
}

#endif /* PGY_LLVM_ENABLED */
