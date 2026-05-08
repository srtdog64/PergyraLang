/*
 * LLVM projection literal helpers for projection-borrowed bindings.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_host_spawn_literal_helpers.h"

#include "llvm_expr_projection_path_helpers.h"
#include "llvm_internal_api.h"

static LLVMValueRef
llvm_projection_binding_error(ASTNode *node, LLVMGenCtx *ctx,
                              const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM projection binding could not be lowered");
    }
    return NULL;
}

LLVMValueRef
llvm_emit_projection_from_binding(ASTNode *node, LLVMGenCtx *ctx,
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
        return llvm_projection_binding_error(node, ctx,
            "LLVM projection binding requires target class and source names");

    target_cls = llvm_lookup_class(ctx, target_class_name);
    source_var = llvm_scope_lookup(ctx, source_name);
    source_class_name = llvm_lookup_var_class(ctx, source_name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL
        || source_decl == NULL) {
        return llvm_projection_binding_error(node, ctx,
            "LLVM projection binding requires target/source metadata and source storage");
    }

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMGetUndef(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *field = &target_cls->fields[i];
        LLVMValueRef field_val = llvm_load_projection_path_value(ctx,
            source_decl, source_cls, source_base, field->field_name);
        if (ctx->has_error || field_val == NULL)
            return NULL;
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_val,
            (unsigned)field->index, llvm_tmp_name(ctx));
    }
    return projected;
}

#endif
