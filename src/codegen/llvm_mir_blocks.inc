static void
llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block, const MIRRoutine *routine,
                               LLVMGenCtx *ctx, LLVMBasicBlockRef *llvm_blocks,
                               LLVMMirVar *vars, size_t var_count, ASTNode *func_decl,
                               LLVMClassTypeEntry *owner_cls, LLVMFuncEntry *owner_sync,
                               const char *owner_name)
{
    (void)routine;
    (void)func_decl;
    LLVMBasicBlockRef llvm_block = llvm_blocks[mir_block->id];
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_block);
    bool emitted_terminator = false;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            break;
        case MIR_INST_DEF:
            if (inst->ast != NULL && inst->result_name != NULL) {
                if (inst->ast->type == AST_LET_DECL || inst->ast->type == AST_ASSIGNMENT) {
                    llvm_emit_statement(inst->ast, ctx);
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                    if (val != NULL && alloca != NULL) {
                        LLVMBuildStore(ctx->builder, val, alloca);
                    }
                }
            }
            break;
        case MIR_INST_PHI:
            break;
        case MIR_INST_BRANCH:
            if (inst->ast != NULL && mir_block->has_succ_true && mir_block->has_succ_false) {
                LLVMValueRef cond = llvm_emit_expression(inst->ast, ctx);
                if (cond != NULL) {
                    LLVMBasicBlockRef true_bb = llvm_blocks[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_blocks[mir_block->succ_false];
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
                emitted_terminator = true;
            }
            break;
        case MIR_INST_RETURN:
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (inst->ast != NULL) {
                LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                if (val != NULL) {
                    LLVMBuildRet(ctx->builder, val);
                    emitted_terminator = true;
                } else {
                    LLVMBuildRetVoid(ctx->builder);
                }
            } else {
                LLVMBuildRetVoid(ctx->builder);
            }
            emitted_terminator = true;
            break;
        case MIR_INST_CLEANUP_EDGE:
            break;
        case MIR_INST_STMT:
            if (inst->ast != NULL)
                llvm_emit_statement(inst->ast, ctx);
            break;
        default:
            break;
        }
    }

    if (!emitted_terminator) {
        if (mir_block->has_succ_true && mir_block->has_succ_false) {
            LLVMBuildCondBr(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(
                                LLVMGetModuleContext(ctx->module)), 1, false),
                            llvm_blocks[mir_block->succ_true],
                            llvm_blocks[mir_block->succ_false]);
        } else if (mir_block->has_succ_true) {
            LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
        } else {
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (ctx->current_ret_type == ctx->type_void) {
                LLVMBuildRetVoid(ctx->builder);
            } else {
                LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
            }
        }
    }
}
