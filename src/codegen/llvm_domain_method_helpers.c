#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

const MIRRoutine *
llvm_find_mir_method_routine_local(const LLVMGenCtx *ctx,
                                   const char *owner_name,
                                   ASTNode *method)
{
    const char *method_name;
    LLVMMIRRoutineInventory routine_inventory;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method == NULL || method->type != AST_FUNC_DECL) {
        return NULL;
    }

    method_name = method->data.func_decl.name;
    if (method_name == NULL)
        return NULL;

    llvm_active_routine_inventory(ctx, &routine_inventory);
    {
        const MIRDeclMethod *method_meta =
            llvm_find_host_method_metadata_in_context(
                ctx, owner_name, method_name);
        if (method_meta != NULL) {
            const MIRRoutine *routine =
                method_meta->has_routine
                    ? llvm_routine_inventory_get(
                        &routine_inventory, method_meta->routine_index)
                    : NULL;
            if (routine != NULL
                && routine->kind == MIR_SCOPE_METHOD
                && routine->name != NULL
                && strcmp(routine->name, method_name) == 0) {
                return routine;
            }
            return NULL;
        }
    }

    return NULL;
}

bool
llvm_param_is_implicit_self_local(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

const char *
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

bool
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

void
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

#endif /* PGY_LLVM_ENABLED */
