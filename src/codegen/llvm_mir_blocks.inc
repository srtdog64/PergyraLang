static void
llvm_mir_emit_with_claim_only(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias;
    bool is_secure;
    const char *inner = "Int";
    LLVMTypeRef slot_ty;
    LLVMValueRef alloca_val;
    LLVMValueRef claimed_ptr;

    if (node == NULL || ctx == NULL || node->type != AST_WITH_STMT)
        return;

    alias = node->data.with_stmt.alias;
    if (alias == NULL || llvm_lookup_slot_inner(ctx, alias) != NULL)
        return;

    is_secure = node->data.with_stmt.is_secure;
    if (node->data.with_stmt.slot_type != NULL
        && node->data.with_stmt.slot_type->type == AST_TYPE
        && node->data.with_stmt.slot_type->data.type.name != NULL) {
        inner = node->data.with_stmt.slot_type->data.type.name;
    }

    slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);

    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    claimed_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca_val, 1,
        llvm_tmp_name(ctx));
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

        snprintf(token_name, sizeof(token_name), "%s_token", alias);
        token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
        LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);
        slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder, alloca_val, ctx->type_i64,
            llvm_tmp_name(ctx));
        token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
            LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
            llvm_tmp_name(ctx));
        slot_token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca_val, 2,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
        token_id_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca, 0,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
        token_write_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca, 1,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_write_ptr);
        token_read_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca, 2,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_read_ptr);
        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
    }

    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner, is_secure);
}

static bool
llvm_mir_stmt_is_cfg_container(ASTNode *node)
{
    if (node == NULL)
        return false;
    return node->type == AST_WITH_STMT;
}

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
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL) {
            fprintf(stderr,
                "[llvm inst] block=%zu inst=%zu kind=%d ast=%d result=%s\n",
                mir_block->id, i, (int)inst->kind,
                inst->ast != NULL ? (int)inst->ast->type : -1,
                inst->result_name != NULL ? inst->result_name : "-");
        }
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            break;
        case MIR_INST_DEF:
            if (inst->ast != NULL && inst->result_name != NULL) {
                if (inst->ast->type == AST_LET_DECL || inst->ast->type == AST_ASSIGNMENT) {
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                        fprintf(stderr, "[llvm inst] emit_statement\n");
                    llvm_emit_statement(inst->ast, ctx);
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                        fprintf(stderr, "[llvm inst] emit_expression_store\n");
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
            if (inst->ast != NULL && inst->ast->type == AST_WITH_STMT) {
                llvm_mir_emit_with_claim_only(inst->ast, ctx);
            } else if (inst->ast != NULL && !llvm_mir_stmt_is_cfg_container(inst->ast)) {
                llvm_emit_statement(inst->ast, ctx);
            }
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
