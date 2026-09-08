static void test_mir_phi_type_projection(void)
{
    TEST("assignment DEF retains its checked lexical type across same-spelling scopes");
    char *source = read_file_text(
        "tests/concept_semantics/nominal/lexical_local_type_identity_valid.pgy");
    ASTNode *program = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool lowered = source != NULL && lower_pipeline_from_source(
        source, &program, &hir, &rir, &mir);
    size_t assignments = 0;
    bool exact_type = true;
    if (lowered) {
        for (size_t r = 0; r < mir->routine_count; r++) {
            MIRRoutine *routine = &mir->routines[r];
            for (size_t b = 0; b < routine->block_count; b++) {
                MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    const MIRInstruction *inst = &block->instructions[i];
                    if (inst->kind == MIR_INST_DEF && inst->expr1 != NULL &&
                        inst->expr1->type == AST_IDENTIFIER) {
                        assignments++;
                        exact_type = exact_type && inst->abi_type_name != NULL &&
                            strcmp(inst->abi_type_name, "Int") == 0;
                    }
                }
            }
        }
    }
    EXPECT(lowered && assignments == 2 && exact_type);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    ast_destroy(program);
    free(source);

    const char *names[] = {"value.1", "value.2", "value.3", "value.4"};
    MIRPhiIncoming edges[] = {{.predecessor_block = 0, .value_name = "value.2"},
                              {.predecessor_block = 0, .value_name = "value.4"}};
    MIRInstruction instructions[] = {
        {.kind = MIR_INST_DEF, .result_name = "value.1", .abi_type_name = "String"},
        {.kind = MIR_INST_DEF, .result_name = "value.2", .abi_type_name = "Int"},
        {.kind = MIR_INST_PHI, .result_name = "value.3",
         .phi_incoming_count = 2, .phi_incomings = edges},
        {.kind = MIR_INST_DEF, .result_name = "value.4", .abi_type_name = "Int"}
    };
    MIRBasicBlock block = {.is_reachable = true, .instructions = instructions,
                          .instruction_count = 4};
    MIRRoutine routine = {.blocks = &block, .block_count = 1};
    TranspilerMIRPhiType types[4];
    TranspilerCtx *ctx = transpiler_ctx_create();
    TEST("phi type follows exact incoming SSA identities, not a shadowed spelling");
    bool ok = transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types);
    EXPECT(ok && types[2].is_phi && strcmp(types[2].type_name, "Int") == 0 &&
           !types[0].is_phi && !types[1].is_phi && !types[3].is_phi);
    transpiler_ctx_destroy(ctx);

    TEST("phi refuses a crossed incoming type even when its spelling agrees");
    ctx = transpiler_ctx_create();
    edges[0].value_name = "value.1";
    EXPECT(!transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types));
    EXPECT_STR_CONTAINS(ctx->backend_error, "conflicting incoming types");
    transpiler_ctx_destroy(ctx);
    edges[0].value_name = "value.2";

    TEST("phi refuses an absent incoming identity before C emission");
    ctx = transpiler_ctx_create();
    edges[0].value_name = "value.9";
    EXPECT(!transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types));
    EXPECT_STR_CONTAINS(ctx->backend_error, "missing incoming value identity");
    transpiler_ctx_destroy(ctx);
    edges[0].value_name = "value.2";

    TEST("one known incoming type cannot hide another missing ABI fact");
    ctx = transpiler_ctx_create();
    instructions[1].abi_type_name = NULL;
    EXPECT(!transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types));
    EXPECT_STR_CONTAINS(ctx->backend_error, "missing or crossed incoming type fact");
    transpiler_ctx_destroy(ctx);
    instructions[1].abi_type_name = "Int";

    TEST("a phi cycle resolves only from a connected concrete incoming fact");
    ctx = transpiler_ctx_create();
    instructions[3] = (MIRInstruction){.kind = MIR_INST_PHI,
        .result_name = "value.4", .phi_incoming_count = 2, .phi_incomings = edges};
    ok = transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types);
    EXPECT(ok && strcmp(types[2].type_name, "Int") == 0 &&
           types[3].is_phi && strcmp(types[3].type_name, "Int") == 0);
    transpiler_ctx_destroy(ctx);

    TEST("an unseeded phi cycle cannot borrow another binding's type by name");
    ctx = transpiler_ctx_create();
    edges[0].value_name = "value.3";
    instructions[3] = (MIRInstruction){.kind = MIR_INST_PHI,
        .result_name = "value.4", .phi_incoming_count = 2, .phi_incomings = edges};
    EXPECT(!transpiler_mir_phi_types_from_incomings(ctx, &routine, names, 4, types));
    EXPECT_STR_CONTAINS(ctx->backend_error, "unresolved incoming type cycle");
    transpiler_ctx_destroy(ctx);
}

static void test_mir_value_result_entry_storage(void)
{
    TEST("inout world SSA entry preserves the admitted parameter indirection");
    char *source = read_file_text(
        "tests/self_hosted/fixtures/domain_runtime_world_zone_lifecycle.pgy");
    ASTNode *program = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    TranspilerCtx *ctx = NULL;
    bool ok = source != NULL && lower_pipeline_from_source(
        source, &program, &hir, &rir, &mir);
    if (ok) {
        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ok = bind_test_c_projection_plan(ctx, mir);
        if (ok)
            emit_program(ctx);
    }
    EXPECT(ok && ctx != NULL && ctx->out != NULL);
    if (ok && ctx != NULL && ctx->out != NULL) {
        EXPECT_STR_CONTAINS(ctx->out->data,
            "return ReadWorld(compiler_world);");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data,
            "ReadWorld(&compiler_world)");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data,
            "CartWorld _pgy_ssa_compiler_world_0 = compiler_world;");
    }
    transpiler_ctx_destroy(ctx);

    TEST("inout SSA entry refuses missing parameter storage before projection");
    const MIRRoutine *routine = NULL;
    for (size_t i = 0; ok && i < mir->routine_count; i++) {
        ASTNode *decl = mir->routines[i].ast;
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && strcmp(decl->data.func_decl.name, "ReadWorld") == 0)
            routine = &mir->routines[i];
    }
    ctx = transpiler_ctx_create();
    ctx->mir = mir;
    if (routine != NULL && bind_test_c_projection_plan(ctx, mir)) {
        /* Present an exact formal entry use with its physical registry absent.
         * No malformed program is emitted or executed by this negative case. */
        const char *entry_names[] = {"compiler_world.0"};
        MIRBasicBlock entry_block = routine->blocks[routine->entry_block];
        entry_block.live_in_names = entry_names;
        entry_block.live_in_name_count = 1;
        MIRRoutine missing_storage = *routine;
        missing_storage.blocks = &entry_block;
        missing_storage.block_count = 1;
        ctx->active_mir_routine = &missing_storage;
        ctx->current_func_decl = routine->ast;
        bool emitted = transpiler_emit_mir_func_ssa_local_decls(ctx, routine->ast,
            &missing_storage, "ReadWorld");
        EXPECT(!emitted);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires its parameter storage fact");
    } else {
        EXPECT(false);
    }
    transpiler_ctx_destroy(ctx);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    ast_destroy(program);
    free(source);
}
