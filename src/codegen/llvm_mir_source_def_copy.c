#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_source_def_copy.h"

#include <stdlib.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_host_field.h"
#include "llvm_mir_resource_view.h"
#include "llvm_mir_scope_bind.h"
#include "../parser/ast_api.h"

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
    ASTNode *source_payload;
    ASTNode *type_ann;
    LLVMValueRef active_alloca;
    const char *source_future_inner;
    const char *source_channel_inner;
    bool source_future_is_remote = false;

    if (inst == NULL || ctx == NULL || inst->result_name == NULL)
        return true;
    if (!llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        return true;
    }
    source_payload = mir_instruction_source_payload(inst);
    type_ann = source_payload != NULL && source_payload->type == AST_LET_DECL
        ? ast_let_type(source_payload) : NULL;
    if (llvm_mir_def_is_resource_view_alias(inst))
        return llvm_mir_bind_resource_view_def_alias(inst, mir_block, ctx, vars,
            var_count);
    has_source = llvm_scope_lookup_snapshot(ctx, base_name, &source);
    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL)
        return true;
    if (source_payload != NULL && source_payload->type == AST_LET_DECL) {
        ASTNode *init = ast_let_initializer(source_payload);
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        if (type_ann != NULL) {
            llvm_register_typed_var_binding(ctx, base_name, target->alloca,
                type_ann);
        } else if (inst->abi_type_name != NULL) {
            llvm_register_typed_var_abi_binding(ctx, base_name,
                target->alloca, inst->abi_type_name);
        }
        if (type_ann != NULL
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
        } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
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
    if (source_payload != NULL && source_payload->type == AST_ASSIGNMENT) {
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
    if (type_ann != NULL)
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
    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && ast_type_name(type_ann) != NULL) {
        const char *ann_name = ast_type_name(type_ann);
        if (llvm_lookup_class(ctx, ann_name) != NULL)
            llvm_register_var_class(ctx, base_name, ann_name);
    }
    return !ctx->has_error;
}

#endif
