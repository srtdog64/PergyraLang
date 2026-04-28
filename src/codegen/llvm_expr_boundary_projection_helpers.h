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
    (void)entry;

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}

static LLVMValueRef
llvm_boundary_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    if (ctx == NULL || slot_var == NULL)
        return NULL;
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, slot_var->type, slot_var->alloca,
                              llvm_tmp_name(ctx));
    }
    return slot_var->alloca;
}

static ASTNode *
llvm_find_function_decl(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    return llvm_lookup_generic_template(ctx, name);
}

static ASTNode *
llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_INTENT_DECL, name);
}

static bool
llvm_boundary_param_uses_pointer_self(LLVMGenCtx *ctx, FuncParam *param)
{
    return param != NULL && llvm_ast_type_uses_pointer_self(ctx, param->type);
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

    args = pgy_arena_calloc(&ctx->scratch,
                            (emitted_count > 0 ? emitted_count : 1) * sizeof(LLVMValueRef));
    if (args == NULL)
        return NULL;

    unsigned arg_idx = 0;
    unsigned emitted_idx = 0;
    for (size_t i = 0; i < decl->data.func_decl.param_count && arg_idx < argc; i++) {
        bool is_secure = false;
        FuncParam *p = decl->data.func_decl.params[i];
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        ASTNode *arg_node = arg_nodes[arg_idx++];
        bool pointer_self = llvm_boundary_param_uses_pointer_self(ctx, p);

        if (inner != NULL && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *source_name = arg_node->data.identifier.name;
            LLVMVarEntry *slot_var = llvm_scope_lookup(ctx, source_name);
            args[emitted_idx++] = slot_var != NULL
                ? llvm_boundary_slot_runtime_arg(ctx, slot_var)
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

        if (pointer_self && arg_node != NULL) {
            if (arg_node->type == AST_IDENTIFIER) {
                const char *source_name = arg_node->data.identifier.name;
                LLVMVarEntry *var = llvm_scope_lookup(ctx, source_name);
                if (var != NULL) {
                    args[emitted_idx++] = LLVMGetTypeKind(var->type) == LLVMPointerTypeKind
                        ? LLVMBuildLoad2(ctx->builder, var->type, var->alloca, llvm_tmp_name(ctx))
                        : var->alloca;
                    continue;
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                LLVMValueRef ptr = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                if (ptr != NULL) {
                    args[emitted_idx++] = ptr;
                    continue;
                }
            }
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

#include "llvm_expr_projection_path_helpers.h"

static ASTNode *
llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
}

static ASTNode *
llvm_find_nominal_host_method_decl(LLVMGenCtx *ctx, const char *host_type_name,
                                   const char *method_name)
{
    return llvm_find_host_method_decl_in_context(ctx, host_type_name, method_name);
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

bool
llvm_type_name_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *mir_decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    mir_decl = ctx->mir != NULL ? mir_find_decl_header(ctx->mir, type_name) : NULL;
    if (mir_decl != NULL)
        return mir_decl->uses_pointer_self;

    if (llvm_find_named_domain_decl(ctx, AST_PARTY_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_ROSTER_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_RELATION_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_ZONE_DECL, type_name) != NULL) {
        return true;
    }
    {
        ASTNode *stmt = llvm_find_projection_nominal_decl(ctx, type_name);
        if (stmt != NULL && stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL) {
            return true;
        }
    }

    return false;
}

static const char *
llvm_current_host_class_name(LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    decl = llvm_current_host_decl(ctx);
    if (decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            return decl->data.class_decl.name;
        case AST_ENUM_DECL:
            return decl->data.enum_decl.name;
        case AST_RELATION_DECL:
            return decl->data.relation_decl.name;
        case AST_EFFECT_DECL:
            return decl->data.effect_decl.name;
        case AST_ZONE_DECL:
            return decl->data.zone_decl.name;
        case AST_WORLD_DECL:
            return decl->data.world_decl.name;
        default:
            break;
        }
    }

    return NULL;
}

static const char *
llvm_current_field_class_name(LLVMGenCtx *ctx, const char *field_name)
{
    LLVMClassTypeEntry *parent_cls;
    LLVMClassTypeEntry *field_cls;
    ASTNode *host_decl;
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
    if (field_cls != NULL)
        return field_cls->class_name;

    host_decl = llvm_find_projection_nominal_decl(ctx, host_name);
    if (host_decl != NULL) {
        size_t field_count = llvm_projection_field_count(host_decl);
        for (size_t i = 0; i < field_count; i++) {
            ClassField *field = llvm_projection_field_at(host_decl, i);
            if (field == NULL || field->name == NULL
                || strcmp(field->name, field_name) != 0
                || field->type == NULL || field->type->type != AST_TYPE
                || field->type->data.type.name == NULL) {
                continue;
            }
            if (llvm_lookup_class(ctx, field->type->data.type.name) != NULL)
                return field->type->data.type.name;
        }
    }

    return NULL;
}
