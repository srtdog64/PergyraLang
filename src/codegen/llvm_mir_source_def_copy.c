#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_source_def_copy.h"

#include <stdlib.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_host_field.h"
#include "llvm_mir_resource_view.h"
#include "llvm_mir_scope_bind.h"
#include "../parser/ast_api.h"

/* The MIR routine inventory owns source-local type shape.  Keep the special
 * registry updates needed after a versioned DEF copy on that fact instead of
 * reopening the declaration's AST type node. */
static const MIRSourceLocalType *
llvm_mir_source_local_type_fact(const LLVMGenCtx *ctx, const char *name)
{
    char base_name[128];
    const MIRSourceLocalType *fact;

    if (ctx == NULL || ctx->current_mir_routine == NULL || name == NULL)
        return NULL;
    fact = mir_routine_source_local_type_fact(
        ctx->current_mir_routine, name);
    if (fact == NULL
        && llvm_mir_base_name_from_versioned(name, base_name,
            sizeof(base_name))) {
        fact = mir_routine_source_local_type_fact(
            ctx->current_mir_routine, base_name);
    }
    return fact;
}

static void
llvm_mir_register_source_local_type_fact(LLVMGenCtx *ctx,
                                         const char *name,
                                         LLVMValueRef binding,
                                         const MIRSourceLocalType *fact)
{
    if (ctx == NULL || name == NULL || fact == NULL)
        return;
    if (fact->is_callable) {
        llvm_register_callable_signature_names(ctx, name,
            fact->callable_param_count,
            (const char *const *)fact->callable_param_type_names,
            fact->callable_return_type_name);
        return;
    }
    if (fact->type_name == NULL || fact->type_name[0] == '\0')
        return;
    llvm_register_typed_var_abi_binding(ctx, name, binding,
        fact->type_name);
    if (strncmp(fact->type_name, "Future<", 7) == 0
        || strncmp(fact->type_name, "RemoteFuture<", 13) == 0) {
        char inner[256];
        if (llvm_constructed_arg_name_copy(fact->type_name, 0,
                inner, sizeof(inner)) && inner[0] != '\0') {
            llvm_register_future_var_binding(ctx, name, binding, inner,
                strncmp(fact->type_name, "RemoteFuture<", 13) == 0);
        }
    } else if (strncmp(fact->type_name, "Channel<", 8) == 0) {
        char inner[256];
        if (llvm_constructed_arg_name_copy(fact->type_name, 0,
            inner, sizeof(inner)) && inner[0] != '\0')
            llvm_register_channel_var_binding(ctx, name, binding, inner);
    } else if (strncmp(fact->type_name, "SecureSlot<", 11) == 0
               || strncmp(fact->type_name, "Slot<", 5) == 0) {
        char inner[256];
        if (llvm_constructed_arg_name_copy(fact->type_name, 0,
                inner, sizeof(inner)) && inner[0] != '\0') {
            llvm_register_slot_var_binding(ctx, name, binding, inner,
                strncmp(fact->type_name, "SecureSlot<", 11) == 0);
        }
    } else if (strncmp(fact->type_name, "DeviceSlot<", 11) == 0) {
        char inner[256];
        if (llvm_constructed_arg_name_copy(fact->type_name, 0,
                inner, sizeof(inner)) && inner[0] != '\0')
            llvm_register_device_slot_var_binding(ctx, name, binding, inner);
    }
    if (llvm_lookup_class(ctx, fact->type_name) != NULL)
        llvm_register_var_class(ctx, name, fact->type_name);
}

bool
llvm_mir_copy_source_def_to_versioned_local(const MIRInstruction *inst,
                                            const MIRBasicBlock *mir_block,
                                            LLVMGenCtx *ctx,
                                            LLVMMirVar *vars,
                                            size_t var_count)
{
    char base_name[128];
    LLVMVarEntry source;
    bool has_source;
    LLVMMirVar *target;
    LLVMValueRef loaded;
    ASTNode *type_ann;
    LLVMValueRef active_alloca;
    const char *source_future_inner;
    const char *source_channel_inner;
    const MIRSourceLocalType *source_local_type_fact;
    bool mir_active;
    bool source_future_is_remote = false;

    if (inst == NULL || ctx == NULL || inst->result_name == NULL)
        return true;
    if (!llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        return true;
    }
    mir_active = ctx->current_mir_routine != NULL;
    type_ann = !mir_active && mir_instruction_uses_source_local_decl_emit(inst)
        ? inst->expr1 : NULL;
    source_local_type_fact = mir_active
        ? llvm_mir_source_local_type_fact(ctx, base_name)
        : NULL;
    if (llvm_mir_def_is_resource_view_alias(inst))
        return llvm_mir_bind_resource_view_def_alias(inst, mir_block, ctx, vars,
            var_count);
    has_source = llvm_scope_lookup_snapshot(ctx, base_name, &source);
    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL)
        return true;
    if (mir_instruction_uses_source_local_decl_emit(inst)) {
        ASTNode *init = inst->expr0;
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        if (source_local_type_fact != NULL) {
            llvm_mir_register_source_local_type_fact(ctx, base_name,
                target->alloca, source_local_type_fact);
        } else if (mir_active && inst->abi_type_name != NULL) {
            llvm_register_typed_var_abi_binding(ctx, base_name,
                target->alloca, inst->abi_type_name);
        } else if (type_ann != NULL) {
            llvm_register_typed_var_binding(ctx, base_name, target->alloca,
                type_ann);
        } else if (inst->abi_type_name != NULL) {
            llvm_register_typed_var_abi_binding(ctx, base_name,
                target->alloca, inst->abi_type_name);
        }
        if (!mir_active && type_ann != NULL
            && type_ann->type == AST_TYPE
            && ast_type_name(type_ann) != NULL) {
            const char *ann_name = ast_type_name(type_ann);
            if (strcmp(ann_name, "Future") == 0
                || strcmp(ann_name, "RemoteFuture") == 0) {
                GenericParams *generic_args = ast_type_generic_args(type_ann);
                GenericParam *inner_param =
                    ast_generic_param_at(generic_args, 0);
                char *future_inner = inner_param != NULL
                    ? llvm_stmt_render_type_arg(inner_param)
                    : NULL;
                if (future_inner != NULL && future_inner[0] != '\0') {
                    llvm_register_future_var_binding(ctx, base_name,
                        target->alloca, future_inner,
                        strcmp(ann_name, "RemoteFuture") == 0);
                }
                free(future_inner);
            }
            if (llvm_lookup_class(ctx, ann_name) != NULL)
                llvm_register_var_class(ctx, base_name, ann_name);
        } else if (!mir_active && init != NULL && init->type == AST_SPAWN_EXPR) {
            const char *future_inner = llvm_infer_spawn_future_inner(ctx,
                init);
            if (future_inner != NULL && future_inner[0] != '\0') {
                llvm_register_future_var_binding(ctx, base_name,
                    target->alloca, future_inner, false);
            }
        }
        return !ctx->has_error;
    }
    if (llvm_mir_copy_host_field_to_versioned_local(ctx, base_name,
            target)) {
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        return !ctx->has_error;
    }
    if (!has_source) {
        return !ctx->has_error;
    }
    if (source.alloca == NULL) {
        if (llvm_lookup_projection_borrow(ctx, base_name) != NULL)
            return true;
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source local '%s' has no backing storage",
            base_name);
        return false;
    }
    source_future_inner = llvm_lookup_future_inner(ctx, base_name);
    if (source_future_inner != NULL)
        source_future_is_remote = llvm_lookup_future_is_remote(ctx, base_name);
    source_channel_inner = llvm_lookup_channel_inner(ctx, base_name);
    if (mir_instruction_uses_source_statement_emit(inst)
        && !mir_instruction_uses_source_local_decl_emit(inst)) {
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        if (inst->abi_type_name != NULL) {
            llvm_register_typed_var_abi_binding(ctx, base_name,
                target->alloca, inst->abi_type_name);
        } else if (source_future_inner != NULL
                   && source_future_inner[0] != '\0') {
            llvm_register_future_var_binding(ctx, base_name, target->alloca,
                source_future_inner, source_future_is_remote);
        } else if (source_channel_inner != NULL
                   && source_channel_inner[0] != '\0') {
            llvm_register_channel_var_binding(ctx, base_name, target->alloca,
                source_channel_inner);
        }
        return !ctx->has_error;
    }
    active_alloca = target->alloca;
    if (source_channel_inner != NULL) {
        active_alloca = source.alloca;
    } else if (source.alloca != target->alloca
        && source.type != NULL
        && target->type != NULL
        && source.type == target->type) {
        loaded = LLVMBuildLoad2(ctx->builder, source.type, source.alloca,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, loaded, target->alloca);
    } else if (source.alloca != target->alloca
               && source.type != NULL
               && target->type != NULL
               && source.type != target->type) {
        active_alloca = source.alloca;
    }
    llvm_mir_bind_base_local_scope(ctx, base_name, active_alloca,
        active_alloca == source.alloca ? source.type : target->type,
        inst->arg1);
    if (source_local_type_fact != NULL) {
        llvm_mir_register_source_local_type_fact(ctx, base_name,
            active_alloca, source_local_type_fact);
    } else if (mir_active && inst->abi_type_name != NULL) {
        llvm_register_typed_var_abi_binding(ctx, base_name, active_alloca,
            inst->abi_type_name);
    } else if (type_ann != NULL)
        llvm_register_typed_var_binding(ctx, base_name, active_alloca,
            type_ann);
    else if (inst->abi_type_name != NULL) {
        llvm_register_typed_var_abi_binding(ctx, base_name, active_alloca,
            inst->abi_type_name);
    } else if (source_future_inner != NULL
               && source_future_inner[0] != '\0') {
        llvm_register_future_var_binding(ctx, base_name, active_alloca,
            source_future_inner, source_future_is_remote);
    } else if (source_channel_inner != NULL
               && source_channel_inner[0] != '\0') {
        llvm_register_channel_var_binding(ctx, base_name, active_alloca,
            source_channel_inner);
    }
    if (!mir_active && type_ann != NULL
        && type_ann->type == AST_TYPE
        && ast_type_name(type_ann) != NULL) {
        const char *ann_name = ast_type_name(type_ann);
        if (llvm_lookup_class(ctx, ann_name) != NULL)
            llvm_register_var_class(ctx, base_name, ann_name);
    }
    return !ctx->has_error;
}

#endif
