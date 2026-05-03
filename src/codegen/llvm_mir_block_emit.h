static void
llvm_mir_emit_with_claim_only(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias;
    bool is_secure;
    const char *inner = NULL;
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
        ASTNode *slot_type = node->data.with_stmt.slot_type;
        GenericParams *generic_args = slot_type->data.type.generic_args;
        if (generic_args == NULL || generic_args->count == 0) {
            inner = slot_type->data.type.name;
        } else if (generic_args->params != NULL
            && generic_args->params[0] != NULL) {
            GenericParam *param = generic_args->params[0];
            if (param->constraint != NULL
                && param->constraint->type == AST_TYPE
                && param->constraint->data.type.name != NULL) {
                inner = param->constraint->data.type.name;
            } else if (param->name != NULL) {
                inner = param->name;
            }
        }
    }
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM MIR with-slot claim for '%s' requires concrete Slot<T> metadata",
            alias != NULL ? alias : "<slot>");
        return;
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

static void
llvm_mir_pin_local_name(const MIRBasicBlock *block, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return;
    snprintf(buf, buf_size, "__pgy_mir_pin_%zu",
             block != NULL ? block->id : 0);
}

static LLVMValueRef
llvm_mir_slot_pointer_arg(LLVMGenCtx *ctx, LLVMVarEntry *entry)
{
    if (ctx == NULL || entry == NULL)
        return NULL;
    if (entry->type != NULL
        && LLVMGetTypeKind(entry->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));
    }
    return entry->alloca;
}

static bool
llvm_mir_emit_pin_enter(const MIRBasicBlock *block, LLVMGenCtx *ctx)
{
    const char *inner;
    bool is_secure;
    LLVMVarEntry *slot_entry;
    LLVMTypeRef pin_ty;
    LLVMValueRef pin_alloca;
    LLVMFuncEntry *pin_fn;
    LLVMValueRef args[2];
    LLVMValueRef slot_ptr_arg;
    LLVMValueRef view;
    LLVMVarEntry *view_entry;
    char pin_name[64];
    char fn_name[128];
    char token_name[256];

    if (block == NULL || ctx == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL)
        return true;

    inner = llvm_lookup_slot_inner(ctx, block->pin_source_name);
    slot_entry = llvm_scope_lookup(ctx, block->pin_source_name);
    if (inner == NULL || slot_entry == NULL || slot_entry->alloca == NULL) {
        llvm_set_error(ctx, "LLVM MIR pin block cannot resolve source slot");
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, block->pin_source_name);
    llvm_mir_pin_local_name(block, pin_name, sizeof(pin_name));
    slot_ptr_arg = llvm_mir_slot_pointer_arg(ctx, slot_entry);
    if (is_secure) {
        LLVMVarEntry *token_entry;
        snprintf(token_name, sizeof(token_name), "%s_token",
                 block->pin_source_name);
        token_entry = llvm_scope_lookup(ctx, token_name);
        if (token_entry == NULL || token_entry->alloca == NULL) {
            llvm_set_error(ctx, "LLVM MIR secure pin block cannot resolve paired token");
            return false;
        }
        snprintf(fn_name, sizeof(fn_name), "pgy_secure_pin_%s_%s",
                 block->pin_view_is_write ? "write" : "read", inner);
        pin_ty = llvm_pinned_secure_slot_struct_type(ctx, inner);
        pin_fn = llvm_lookup_function(ctx, fn_name);
        if (pin_fn == NULL || pin_fn->fn == NULL) {
            llvm_set_error(ctx, "LLVM MIR secure pin runtime function is not registered");
            return false;
        }
        pin_alloca = llvm_create_entry_alloca(ctx, pin_ty, pin_name);
        args[0] = slot_ptr_arg;
        args[1] = token_entry->alloca;
        view = LLVMBuildCall2(ctx->builder, pin_fn->fn_type, pin_fn->fn,
                              args, 2, llvm_tmp_name(ctx));
    } else {
        snprintf(fn_name, sizeof(fn_name), "pgy_pin_%s_%s",
                 block->pin_view_is_write ? "write" : "read", inner);
        pin_ty = llvm_pinned_slot_struct_type(ctx, inner);
        pin_fn = llvm_lookup_function(ctx, fn_name);
        if (pin_fn == NULL || pin_fn->fn == NULL) {
            llvm_set_error(ctx, "LLVM MIR pin runtime function is not registered");
            return false;
        }
        pin_alloca = llvm_create_entry_alloca(ctx, pin_ty, pin_name);
        args[0] = slot_ptr_arg;
        view = LLVMBuildCall2(ctx->builder, pin_fn->fn_type, pin_fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    LLVMBuildStore(ctx->builder, view, pin_alloca);
    llvm_scope_declare(ctx, pergyra_strdup(pin_name), pin_alloca, pin_ty);
    if (block->pin_view_name != NULL) {
        view_entry = llvm_scope_lookup(ctx, block->pin_view_name);
        if (view_entry == NULL) {
            LLVMTypeRef view_slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            llvm_scope_declare(ctx, pergyra_strdup(block->pin_view_name),
                               slot_ptr_arg, view_slot_ty);
        }
        llvm_register_slot_var(ctx, pergyra_strdup(block->pin_view_name),
                               inner, is_secure);
        if (is_secure) {
            LLVMVarEntry *token_entry;
            snprintf(token_name, sizeof(token_name), "%s_token",
                     block->pin_source_name);
            token_entry = llvm_scope_lookup(ctx, token_name);
            if (token_entry != NULL) {
                char view_token_name[256];
                snprintf(view_token_name, sizeof(view_token_name), "%s_token",
                         block->pin_view_name);
                if (llvm_scope_lookup(ctx, view_token_name) == NULL) {
                    llvm_scope_declare(ctx, pergyra_strdup(view_token_name),
                                       token_entry->alloca, token_entry->type);
                }
            }
        }
    }
    return true;
}

static bool
llvm_mir_emit_pin_exit(const MIRBasicBlock *block, LLVMGenCtx *ctx)
{
    const char *inner;
    bool is_secure;
    LLVMVarEntry *pin_entry;
    LLVMFuncEntry *unpin_fn;
    LLVMValueRef args[1];
    char pin_name[64];
    char fn_name[128];

    if (block == NULL || ctx == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL)
        return true;

    inner = llvm_lookup_slot_inner(ctx, block->pin_source_name);
    if (inner == NULL) {
        llvm_set_error(ctx, "LLVM MIR pin block cannot resolve source slot at exit");
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, block->pin_source_name);
    llvm_mir_pin_local_name(block, pin_name, sizeof(pin_name));
    pin_entry = llvm_scope_lookup(ctx, pin_name);
    if (pin_entry == NULL || pin_entry->alloca == NULL) {
        llvm_set_error(ctx, "LLVM MIR pin block cannot resolve pin local at exit");
        return false;
    }

    snprintf(fn_name, sizeof(fn_name), is_secure ? "pgy_secure_unpin_%s"
                                                 : "pgy_unpin_%s",
             inner);
    unpin_fn = llvm_lookup_function(ctx, fn_name);
    if (unpin_fn == NULL || unpin_fn->fn == NULL) {
        llvm_set_error(ctx, "LLVM MIR pin unpin runtime function is not registered");
        return false;
    }

    args[0] = pin_entry->alloca;
    LLVMBuildCall2(ctx->builder, unpin_fn->fn_type, unpin_fn->fn,
                   args, 1, "");
    return true;
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

    if (!llvm_mir_emit_pin_enter(mir_block, ctx))
        return;
    if (!llvm_mir_emit_for_in_body_binding(routine, mir_block, ctx))
        return;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL) {
            fprintf(stderr,
                "[llvm inst] block=%zu inst=%zu kind=%d ast=%d result=%s\n",
                mir_block->id, i, (int)inst->kind,
                inst->has_source_location ? (int)inst->source_ast_type : -1,
                inst->result_name != NULL ? inst->result_name : "-");
        }
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            if (inst->name != NULL
                && strcmp(inst->name, "Claim") == 0
                && inst->ast != NULL
                && inst->source_ast_type == AST_WITH_STMT) {
                llvm_mir_emit_with_claim_only(inst->ast, ctx);
            }
            if (inst->name != NULL
                && (strcmp(inst->name, "BorrowRead") == 0
                    || strcmp(inst->name, "BorrowWrite") == 0)
                && inst->arg0 != NULL
                && inst->arg1 != NULL) {
                LLVMVarEntry *source_entry = llvm_scope_lookup(ctx, inst->arg0);
                const char *inner = llvm_lookup_slot_inner(ctx, inst->arg0);
                bool is_secure = llvm_lookup_slot_is_secure(ctx, inst->arg0);
                if (source_entry != NULL && inner != NULL) {
                    if (llvm_scope_lookup(ctx, inst->arg1) == NULL) {
                        llvm_scope_declare(ctx, pergyra_strdup(inst->arg1),
                                           source_entry->alloca,
                                           source_entry->type);
                    }
                    llvm_register_slot_var(ctx, pergyra_strdup(inst->arg1),
                                           inner, is_secure);
                    if (is_secure) {
                        char source_token_name[256];
                        char view_token_name[256];
                        LLVMVarEntry *token_entry;
                        snprintf(source_token_name, sizeof(source_token_name),
                                 "%s_token", inst->arg0);
                        snprintf(view_token_name, sizeof(view_token_name),
                                 "%s_token", inst->arg1);
                        token_entry = llvm_scope_lookup(ctx, source_token_name);
                        if (token_entry != NULL
                            && llvm_scope_lookup(ctx, view_token_name) == NULL) {
                            llvm_scope_declare(ctx, pergyra_strdup(view_token_name),
                                               token_entry->alloca,
                                               token_entry->type);
                        }
                    }
                }
            }
            break;
        case MIR_INST_DEF:
            if (inst->ast != NULL && inst->result_name != NULL) {
                if (inst->has_source_location
                    && (inst->source_ast_type == AST_LET_DECL
                        || inst->source_ast_type == AST_ASSIGNMENT)) {
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                        fprintf(stderr, "[llvm inst] emit_statement\n");
                    if (!llvm_mir_declare_assignment_recv_target(inst->ast, ctx))
                        return;
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
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL) {
                emitted_terminator = true;
                break;
            }
            if (inst->ast != NULL && mir_block->has_succ_true && mir_block->has_succ_false) {
                LLVMValueRef cond;
                if (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                    || inst->branch_shape == MIR_BRANCH_FOR_IN) {
                    cond = llvm_mir_emit_for_loop_condition(inst, ctx);
                } else if (inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
                    cond = llvm_mir_emit_match_case_condition(func_decl, inst->ast, ctx);
                } else if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH) {
                    cond = llvm_mir_emit_select_dispatch_condition(
                        inst->ast, routine, mir_block->succ_true, ctx);
                    if (cond == NULL)
                        cond = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);
                } else {
                    cond = llvm_emit_expression(inst->ast, ctx);
                }
                if (cond != NULL) {
                    LLVMBasicBlockRef true_bb = llvm_blocks[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_blocks[mir_block->succ_false];
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
                emitted_terminator = true;
            }
            break;
        case MIR_INST_RETURN:
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (inst->ast != NULL) {
                const char *saved_expected_type_name = ctx->expected_type_name;
                LLVMValueRef val;
                if (ctx->current_func_decl != NULL
                    && ctx->current_func_decl->type == AST_FUNC_DECL
                    && ctx->current_func_decl->data.func_decl.return_type != NULL) {
                    ctx->expected_type_name = llvm_stmt_render_type_annotation_static(
                        ctx->current_func_decl->data.func_decl.return_type);
                }
                val = llvm_emit_expression(inst->ast, ctx);
                ctx->expected_type_name = saved_expected_type_name;
                if (val != NULL) {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildRet(ctx->builder, val);
                    emitted_terminator = true;
                } else {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildRetVoid(ctx->builder);
                }
            } else {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                LLVMBuildRetVoid(ctx->builder);
            }
            emitted_terminator = true;
            break;
        case MIR_INST_CLEANUP_EDGE:
            break;
        case MIR_INST_LOOP_INIT:
            if (!llvm_mir_emit_for_loop_init(inst, ctx))
                return;
            break;
        case MIR_INST_STMT:
            if (inst->ast != NULL
                && inst->has_source_location
                && inst->source_ast_type == AST_DEFER_STMT) {
                if (inst->ast->data.defer_stmt.body != NULL)
                    llvm_register_defer(inst->ast->data.defer_stmt.body, ctx);
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
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            LLVMBuildCondBr(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(
                                LLVMGetModuleContext(ctx->module)), 1, false),
                            llvm_blocks[mir_block->succ_true],
                            llvm_blocks[mir_block->succ_false]);
        } else if (mir_block->has_succ_true) {
            if (!llvm_mir_emit_loop_backedge_increment(routine, mir_block, ctx))
                return;
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
        } else {
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            if (ctx->current_ret_type == ctx->type_void) {
                LLVMBuildRetVoid(ctx->builder);
            } else {
                LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
            }
        }
    }
}
