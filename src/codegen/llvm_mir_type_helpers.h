/*
 * LLVM MIR type and boundary-slot helpers.
 */

static LLVMTypeRef
llvm_mir_type_from_abi_layout(LLVMGenCtx *ctx, const MIRTypeLayout *layout)
{
    const char *layout_name;
    const char *runtime_fn;

    if (ctx == NULL || layout == NULL)
        return NULL;

    layout_name = layout->abi_type_name;
    runtime_fn = layout->runtime_fn;

    if (layout_name != NULL) {
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
            || strcmp(layout_name, "RemoteFuture") == 0
            || strcmp(layout_name, "TaskHandle") == 0) {
            return pergyra_type_to_llvm(ctx, layout_name);
        }

        if (strcmp(layout_name, "PinnedSlotView<Int>") == 0)
            return llvm_pinned_slot_struct_type(ctx, "Int");
        if (strcmp(layout_name, "PinnedSecureSlotView<Int>") == 0)
            return llvm_pinned_secure_slot_struct_type(ctx, "Int");

        if (strstr(layout_name, "secure_slot_int") != NULL)
            return llvm_secure_slot_struct_type(ctx, "Int");
        if (strstr(layout_name, "secure_slot_long") != NULL)
            return llvm_secure_slot_struct_type(ctx, "Long");
        if (strstr(layout_name, "secure_slot_float") != NULL)
            return llvm_secure_slot_struct_type(ctx, "Float");
        if (strstr(layout_name, "secure_slot_double") != NULL)
            return llvm_secure_slot_struct_type(ctx, "Double");
        if (strstr(layout_name, "secure_slot_bool") != NULL)
            return llvm_secure_slot_struct_type(ctx, "Bool");
        if (strstr(layout_name, "secure_slot_string") != NULL)
            return llvm_secure_slot_struct_type(ctx, "String");
        if (strstr(layout_name, "device_slot_int") != NULL)
            return llvm_slot_struct_type(ctx, "Int");
        if (strstr(layout_name, "device_slot_string") != NULL)
            return llvm_slot_struct_type(ctx, "String");
        if (strstr(layout_name, "slot_int") != NULL)
            return llvm_slot_struct_type(ctx, "Int");
        if (strstr(layout_name, "slot_long") != NULL)
            return llvm_slot_struct_type(ctx, "Long");
        if (strstr(layout_name, "slot_float") != NULL)
            return llvm_slot_struct_type(ctx, "Float");
        if (strstr(layout_name, "slot_double") != NULL)
            return llvm_slot_struct_type(ctx, "Double");
        if (strstr(layout_name, "slot_bool") != NULL)
            return llvm_slot_struct_type(ctx, "Bool");
        if (strstr(layout_name, "slot_string") != NULL)
            return llvm_slot_struct_type(ctx, "String");
        if (strstr(layout_name, "option_int") != NULL)
            return pergyra_type_to_llvm(ctx, "Option<Int>");
        if (strstr(layout_name, "option_long") != NULL)
            return pergyra_type_to_llvm(ctx, "Option<Long>");
        if (strstr(layout_name, "option_bool") != NULL)
            return pergyra_type_to_llvm(ctx, "Option<Bool>");
        if (strstr(layout_name, "option_string") != NULL)
            return pergyra_type_to_llvm(ctx, "Option<String>");
        if (strstr(layout_name, "result_int") != NULL)
            return pergyra_type_to_llvm(ctx, "Result<Int>");
        if (strstr(layout_name, "result_bool") != NULL)
            return pergyra_type_to_llvm(ctx, "Result<Bool>");
        if (strstr(layout_name, "result_string") != NULL)
            return pergyra_type_to_llvm(ctx, "Result<String>");
    }

    if (runtime_fn != NULL) {
        if (strncmp(runtime_fn, "pgy_claim_secure_", 17) == 0)
            return llvm_secure_slot_struct_type(ctx, runtime_fn + 17);
        if (strncmp(runtime_fn, "pgy_claim_device_", 17) == 0)
            return llvm_slot_struct_type(ctx, runtime_fn + 17);
        if (strncmp(runtime_fn, "pgy_claim_", 10) == 0)
            return llvm_slot_struct_type(ctx, runtime_fn + 10);
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

static LLVMTypeRef
llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return ctx->type_i32;
    LLVMTypeRef type = ast_type_to_llvm(ctx, type_node);
    return type != NULL ? type : ctx->type_i32;
}

static LLVMTypeRef
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
        return ctx->type_i32;
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
    return ctx->type_i32;
}

static bool
llvm_mir_param_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node)
{
    return llvm_ast_type_uses_pointer_self(ctx, type_node);
}

static const char *
llvm_mir_boundary_slot_inner_name(FuncParam *param, bool *is_secure_out)
{
    const char *type_name;
    GenericParams *generic_args;
    const char *inner_name;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (param == NULL || param->type == NULL || param->type->type != AST_TYPE
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

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}
