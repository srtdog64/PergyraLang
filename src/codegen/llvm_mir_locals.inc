static void
llvm_emit_mir_local_allocas(const MIRRoutine *routine, LLVMGenCtx *ctx,
                            LLVMMirVar **vars_ptr, size_t *var_capacity_ptr,
                            size_t *var_count_ptr)
{
    LLVMMirVar *vars = vars_ptr != NULL ? *vars_ptr : NULL;
    size_t var_capacity = var_capacity_ptr != NULL ? *var_capacity_ptr : 0;
    size_t var_count = var_count_ptr != NULL ? *var_count_ptr : 0;

    if (routine == NULL || ctx == NULL || vars_ptr == NULL
        || var_capacity_ptr == NULL || var_count_ptr == NULL)
        return;

    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *mir_block = &routine->blocks[b];
        for (size_t j = 0; j < mir_block->instruction_count; j++) {
            const MIRInstruction *inst = &mir_block->instructions[j];
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                && inst->result_name != NULL) {
                LLVMTypeRef alloca_type = ctx->type_i32;
                if (var_count >= var_capacity) {
                    var_capacity *= 2;
                    vars = realloc(vars, var_capacity * sizeof(LLVMMirVar));
                }
                vars[var_count].mir_name = inst->result_name;
                vars[var_count].type = alloca_type;
                vars[var_count].alloca = llvm_create_entry_alloca(
                    ctx, alloca_type, inst->result_name);
                var_count++;
            }
        }
    }

    *vars_ptr = vars;
    *var_capacity_ptr = var_capacity;
    *var_count_ptr = var_count;
}

static void
llvm_emit_mir_param_allocas(const MIRRoutine *routine, ASTNode *func_decl,
                            LLVMValueRef fn, LLVMGenCtx *ctx, bool is_intent,
                            bool is_method, LLVMClassTypeEntry *owner_cls,
                            const char *owner_name, size_t param_count)
{
    (void)routine;
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            ASTNode *involves = func_decl->data.intent_decl.involves[i];
            const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
            ASTNode *subject_type = involves != NULL ? involves->data.intent_involves.subject_type : NULL;
            const char *type_name = (subject_type != NULL && subject_type->type == AST_TYPE)
                ? subject_type->data.type.name : NULL;
            LLVMTypeRef pt = (subject_type != NULL) ? llvm_mir_type_from_ast(ctx, subject_type)
                                                    : ctx->type_i32;
            if (involves != NULL && llvm_intent_involves_uses_pointer_self(ctx, involves))
                pt = LLVMPointerType(pt, 0);
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt,
                alias != NULL ? alias : "participant");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
            llvm_scope_declare(ctx, alias != NULL ? alias : "participant", alloca, pt);
            if (subject_type != NULL)
                llvm_register_typed_var(ctx, alias, subject_type);
            if (type_name != NULL)
                llvm_register_var_class(ctx, alias, type_name);
        } else {
            if (is_method && i == 0) {
                LLVMTypeRef self_type = owner_cls != NULL
                    ? (owner_cls->is_pointer_self_host
                        ? LLVMPointerType(owner_cls->struct_type, 0)
                        : owner_cls->struct_type)
                    : ctx->type_i8ptr;
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, self_type,
                    (owner_cls != NULL && owner_cls->is_pointer_self_host) ? "self.addr" : "self");
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
                llvm_scope_declare(ctx, "self", alloca, self_type);
                if (owner_name != NULL)
                    llvm_register_var_class(ctx, "self", owner_name);
            } else {
                size_t logical_index = is_method ? (i - 1) : i;
                size_t seen = 0;
                FuncParam *p = NULL;
                for (size_t param_index = 0;
                     param_index < func_decl->data.func_decl.param_count;
                     param_index++) {
                    FuncParam *candidate = func_decl->data.func_decl.params[param_index];
                    if (candidate != NULL
                        && candidate->type == NULL
                        && candidate->name != NULL
                        && strcmp(candidate->name, "self") == 0) {
                        continue;
                    }
                    if (seen == logical_index) {
                        p = candidate;
                        break;
                    }
                    seen++;
                }
                if (p == NULL)
                    continue;
                LLVMTypeRef pt = (p->type != NULL) ? llvm_mir_type_from_ast(ctx, p->type)
                                                   : ctx->type_i32;
                if (p->type != NULL && llvm_mir_param_uses_pointer_self(ctx, p->type))
                    pt = LLVMPointerType(pt, 0);
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
                llvm_scope_declare(ctx, p->name, alloca, pt);
                llvm_register_typed_var(ctx, p->name, p->type);
            }
        }
    }
}
