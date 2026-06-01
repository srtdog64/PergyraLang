/*
 * LLVM MIR type and boundary-slot helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_type_helpers.h"

#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_internal_api.h"

static LLVMTypeRef
llvm_mir_pinned_view_type_from_layout(LLVMGenCtx *ctx, const char *layout_name)
{
    char inner_name[256];
    bool is_secure = false;

    if (ctx == NULL || layout_name == NULL)
        return NULL;
    if (strncmp(layout_name, "PinnedSlotView<", 15) == 0) {
        is_secure = false;
    } else if (strncmp(layout_name, "PinnedSecureSlotView<", 21) == 0) {
        is_secure = true;
    } else {
        return NULL;
    }

    if (!llvm_constructed_arg_name_copy(layout_name, 0,
            inner_name, sizeof(inner_name))
        || inner_name[0] == '\0'
        || strcmp(inner_name, "Unknown") == 0) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM MIR pinned view '%s' requires concrete inner type metadata",
            layout_name);
        return NULL;
    }

    return is_secure
        ? llvm_pinned_secure_slot_struct_type(ctx, inner_name)
        : llvm_pinned_slot_struct_type(ctx, inner_name);
}

LLVMTypeRef
llvm_mir_type_from_abi_layout(LLVMGenCtx *ctx, const MIRTypeLayout *layout)
{
    const char *layout_name;

    if (ctx == NULL || layout == NULL)
        return NULL;

    layout_name = layout->abi_type_name;

    if (layout_name != NULL) {
        LLVMTypeRef pinned_view_ty =
            llvm_mir_pinned_view_type_from_layout(ctx, layout_name);
        if (pinned_view_ty != NULL || ctx->has_error)
            return pinned_view_ty;

        if (strncmp(layout_name, "Slot<", 5) == 0
            || strncmp(layout_name, "SecureSlot<", 11) == 0
            || strncmp(layout_name, "DeviceSlot<", 11) == 0
            || strncmp(layout_name, "Option<", 7) == 0
            || strncmp(layout_name, "Result<", 7) == 0
            || strncmp(layout_name, "Array<", 6) == 0
            || strncmp(layout_name, "Slice<", 6) == 0
            || strncmp(layout_name, "List<", 5) == 0
            || strncmp(layout_name, "Queue<", 6) == 0
            || strncmp(layout_name, "Set<", 4) == 0
            || strncmp(layout_name, "HashMap<", 8) == 0
            || strncmp(layout_name, "Box<", 4) == 0
            || strcmp(layout_name, "Future") == 0
            || strcmp(layout_name, "RemoteFuture") == 0) {
            return pergyra_type_to_llvm(ctx, layout_name);
        }
        if (strcmp(layout_name, "TaskHandle") == 0)
            return ctx->type_task_handle;
    }

    if (layout->inner_c_type != NULL) {
        if (strcmp(layout->inner_c_type, "int32_t") == 0)
            return ctx->type_i32;
        if (strcmp(layout->inner_c_type, "int64_t") == 0)
            return ctx->type_i64;
        if (strcmp(layout->inner_c_type, "float") == 0)
            return ctx->type_f32;
        if (strcmp(layout->inner_c_type, "double") == 0)
            return ctx->type_f64;
        if (strcmp(layout->inner_c_type, "bool") == 0)
            return ctx->type_i1;
        if (strcmp(layout->inner_c_type, "char*") == 0)
            return ctx->type_i8ptr;
    }

    return NULL;
}

LLVMTypeRef
llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    LLVMTypeRef type;

    if (ctx == NULL || type_node == NULL)
        return NULL;

    type = ast_type_to_llvm(ctx, type_node);
    return type;
}

LLVMTypeRef
llvm_mir_required_type_from_ast(LLVMGenCtx *ctx,
                                ASTNode *owner,
                                ASTNode *type_node,
                                const char *slot_kind)
{
    LLVMTypeRef type;

    if (ctx == NULL)
        return NULL;
    if (type_node == NULL) {
        llvm_set_error_at_with_hints(ctx, owner,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM MIR %s requires explicit type metadata; silent i32 fallback is not allowed",
            slot_kind != NULL ? slot_kind : "signature slot");
        return NULL;
    }

    type = ast_type_to_llvm(ctx, type_node);
    if (type != NULL)
        return type;

    llvm_set_error_at_with_hints(ctx, type_node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM MIR %s has unsupported type metadata; silent i32 fallback is not allowed",
        slot_kind != NULL ? slot_kind : "signature slot");
    return NULL;
}

bool
llvm_mir_param_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node)
{
    return llvm_ast_type_uses_pointer_self(ctx, type_node);
}

void
llvm_mir_register_nominal_class(LLVMGenCtx *ctx, const char *name,
                                ASTNode *type_node)
{
    const char *type_name;

    if (ctx == NULL || name == NULL || type_node == NULL
        || type_node->type != AST_TYPE) {
        return;
    }
    type_name = ast_type_name(type_node);
    if (type_name != NULL && type_name[0] != '\0')
        llvm_register_var_class(ctx, name, type_name);
}

const char *
llvm_mir_boundary_slot_inner_name(LLVMGenCtx *ctx, FuncParam *param,
                                  bool *is_secure_out)
{
    const char *type_name;
    GenericParams *generic_args;
    const char *inner_name;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (param == NULL || param->type == NULL || param->type->type != AST_TYPE
        || ast_type_name(param->type) == NULL)
        return NULL;
    if (param->mode != PARAM_MODE_OWN && param->mode != PARAM_MODE_REF)
        return NULL;

    type_name = ast_type_name(param->type);
    if (!pgy_codegen_type_name_is_slot(type_name)
        && !pgy_codegen_type_name_is_secure_slot(type_name))
        return NULL;

    generic_args = ast_type_generic_args(param->type);
    GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
    if (inner_param == NULL)
        return NULL;

    inner_name = llvm_keep_rendered_persistent(ctx,
        llvm_stmt_render_type_arg(inner_param),
        "out of memory copying LLVM MIR slot type");
    if (inner_name == NULL)
        return NULL;

    if (is_secure_out != NULL)
        *is_secure_out = pgy_codegen_type_name_is_secure_slot(type_name);
    return inner_name;
}

#endif
