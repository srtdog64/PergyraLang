static void
test_mir_lexical_binding_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    MIRRoutine *routine = NULL;
    const char *source =
        "func Main() -> Void { let n: Int = 1; unsafe {"
        " let n: Int = 2; Log(n); unsafe { n = n + 1; Log(n); }"
        " } Log(n); }";
    bool ok = lower_mir_from_source(source, &hir, &rir, &mir);
    if (ok)
        routine = find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION);
    MIRInstruction *defs[2] = {NULL, NULL};
    MIRInstruction *logs[3] = {NULL, NULL, NULL};
    size_t def_count = 0, log_count = 0;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL
                    && strcmp(inst->arg0, "n") == 0 && def_count < 2)
                    defs[def_count++] = inst;
                if (inst->kind == MIR_INST_STMT && inst->expr0 != NULL
                    && inst->expr0->type == AST_CALL
                    && ast_call_arg_count(inst->expr0) == 1 && log_count < 3)
                    logs[log_count++] = inst;
            }
        }
    }
    TEST("MIR keeps distinct same-spelled lexical definitions");
    EXPECT(ok && def_count == 2 && defs[0]->binding_syntax_id != 0
           && defs[1]->binding_syntax_id != 0
           && defs[0]->binding_syntax_id != defs[1]->binding_syntax_id
           && strcmp(defs[0]->result_name, defs[1]->result_name) != 0);
    TEST("MIR inner reads and post-scope reads use exact binding versions");
    EXPECT(def_count == 2 && log_count == 3
           && logs[0]->use_count == 1 && logs[1]->use_count == 1
           && logs[2]->use_count == 1
           && strcmp(logs[0]->uses[0], defs[1]->result_name) == 0
           && strcmp(logs[1]->uses[0], defs[1]->result_name) == 0
           && strcmp(logs[2]->uses[0], defs[0]->result_name) == 0);
    TEST("MIR source-local inventory retains both lexical declarations");
    EXPECT(routine != NULL && routine->source_local_type_count == 2
           && routine->source_local_types[0].binding_syntax_id != 0
           && routine->source_local_types[1].binding_syntax_id != 0
           && routine->source_local_types[0].binding_syntax_id
                != routine->source_local_types[1].binding_syntax_id
           && mir_json_routine_local_refs_required(routine));

    /* Refusal only: corrupt the admitted read fact and ask the use owner.
     * Never emit or execute this altered graph. */
    bool refused = false;
    char *error = NULL;
    if (log_count == 3) {
        ASTNode *read = ast_call_argument(logs[0]->expr0, 0);
        uint32_t identity = ast_identifier_binding_syntax_id(read);
        ast_identifier_set_binding_syntax_id(read, 0);
        refused = !mir_populate_use_edges(routine, &error)
            && error != NULL && strstr(error, "semantic binding identity") != NULL;
        ast_identifier_set_binding_syntax_id(read, identity);
    }
    TEST("MIR refuses missing local read identity without name repair");
    EXPECT(refused);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);

    hir = NULL;
    rir = NULL;
    mir = NULL;
    TEST("MIR Result payload reads retain resolved lexical identities");
    EXPECT(lower_mir_from_source(
        "func Work(ok: Bool) -> Result<Int> { let found: Int = 7;"
        " let reason: String = \"absent\"; if ok { return Ok(found); }"
        " return Err(reason); } func Main() -> Void {}",
        &hir, &rir, &mir));
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);

    hir = NULL;
    rir = NULL;
    mir = NULL;
    bool field_is_not_local = lower_mir_from_source(
        "zone Gauge { shared n: Int = 1"
        " func Change(self) -> Int { n = n + 1;"
        " unsafe { let n: Int = 7; n = n + 1; Log(n); } return n; } }"
        " func Main() -> Void { let gauge = Gauge(); Log(gauge.Change()); }",
        &hir, &rir, &mir);
    routine = field_is_not_local
        ? find_mir_routine_mut(mir, "Change", MIR_SCOPE_METHOD) : NULL;
    size_t field_writes = 0;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind != MIR_INST_ASSIGN
                    || !ast_identifier_binding_is_host_field(inst->expr0))
                    continue;
                uint32_t field_id = ast_identifier_binding_syntax_id(inst->expr0);
                field_writes++;
                field_is_not_local &= field_id != 0;
                for (size_t n = 0; n < routine->ssa_binding_count; n++)
                    field_is_not_local &= routine->ssa_bindings[n].binding_syntax_id != field_id;
                ASTNode *copy = ast_clone(inst->expr0);
                field_is_not_local &= ast_identifier_binding_is_host_field(copy)
                    && ast_identifier_binding_syntax_id(copy) == field_id;
                ast_destroy(copy);
            }
        }
    }
    TEST("host field identity is not promoted to a same-spelled local SSA binding");
    EXPECT(field_is_not_local && field_writes == 1);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_mir_enum_constructor_reference_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "enum Mark { One(Int), Two(Int) }"
        "func Value(m: Mark) -> Int { match m {"
        " case One(n): return n; case Two(n): return n; } }"
        "func Main() -> Void { let n: Int = 3;"
        " let total: Int = Value(One(n)) + Value(Two(n)); Log(total); }",
        &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(
        mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    MIRInstruction *total = NULL;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL
                    && strcmp(inst->arg0, "total") == 0)
                    total = inst;
            }
        }
    }
    TEST("distinct enum constructors sharing a declaration lower in one expression");
    EXPECT(ok && total != NULL);
    TEST("enum declaration references do not become local SSA reads");
    EXPECT(total != NULL && total->use_count == 1
           && strncmp(total->uses[0], "n.", 2) == 0);
    char *error = NULL;
    bool refused = false;
    if (total != NULL && total->expr0 != NULL) {
        ASTNode *read = ast_call_argument(ast_call_argument(
            ast_binary_left(total->expr0), 0), 0);
        uint32_t identity = ast_identifier_binding_syntax_id(read);
        ast_identifier_set_binding_syntax_id(read, total->binding_syntax_id);
        refused = !mir_populate_use_edges(routine, &error)
            && error != NULL && strstr(error, "semantic binding identity") != NULL;
        ast_identifier_set_binding_syntax_id(read, identity);
    }
    TEST("expression collection still rejects a crosswired local identity");
    EXPECT(refused);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_mir_io_summary_operand_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Main() -> Void { let i: Int = 0; while i < 1 {"
        " let path: String = \"fixture.txt\"; if i == 0 {"
        " let content: String = ReadFile(path); Log(content); } i = i + 1; } }",
        &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    MIRInstruction *path = NULL, *read = NULL, *summary = NULL;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->arg0 == NULL) continue;
                if (inst->kind == MIR_INST_DEF && strcmp(inst->arg0, "path") == 0) path = inst;
                if (inst->kind == MIR_INST_DEF && strcmp(inst->arg0, "content") == 0) read = inst;
                if (inst->kind == MIR_INST_RESOURCE_OP && strcmp(inst->arg0, "ReadFile") == 0) summary = inst;
            }
        }
    }
    TEST("IO summary retains evidence without inventing an entry SSA read");
    EXPECT(ok && summary != NULL && summary->use_count == 0
        && summary->arg1 != NULL && strcmp(summary->arg1, "path") == 0);
    TEST("executable nested IO expression retains its exact lexical operand");
    EXPECT(path != NULL && read != NULL && read->use_count == 1
        && strcmp(read->uses[0], path->result_name) == 0);
    bool refused = false;
    char *error = NULL;
    if (read != NULL && read->expr0 != NULL && read->expr0->type == AST_CALL) {
        ASTNode *operand = ast_call_argument(read->expr0, 0);
        uint32_t identity = ast_identifier_binding_syntax_id(operand);
        ast_identifier_set_binding_syntax_id(operand, 0);
        refused = !mir_populate_use_edges(routine, &error)
            && error != NULL && strstr(error, "semantic binding identity") != NULL;
        ast_identifier_set_binding_syntax_id(operand, identity);
    }
    TEST("IO operand missing binding still refuses without executing the mutated input");
    EXPECT(refused);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_mir_scalar_parameter_wire_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Constant(inout n: Int) -> Int { n = 7; return 9; }"
        "func Formal(inout n: Int) -> Int { return n; }"
        "func Shadow(inout n: Int) -> Int {"
        " unsafe { let n: String = \"inner\"; n = Concat(n, \"!\"); Log(n); } return n; }"
        "func Main() -> Void {}", &hir, &rir, &mir);
    TEST("MIR scalar copy-out wire controls lower successfully");
    EXPECT(ok);
    const char *names[] = {"Constant", "Formal", "Shadow"};
    for (size_t r = 0; ok && r < 3; r++) {
        MIRRoutine *routine = find_mir_routine_mut(mir, names[r], MIR_SCOPE_FUNCTION);
        bool prefix_ok = false, rejects_prefix = false, shadow_is_local = false;
        if (routine != NULL) {
            for (size_t b = 0; b < routine->block_count; b++) {
                MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    MIRInstruction *inst = &block->instructions[i];
                    if (inst->kind == MIR_INST_RETURN) {
                        size_t prefix = inst->return_expression_use_count;
                        prefix_ok = inst->use_count == 1 && prefix == (r == 0 ? 0 : 1);
                        char *error = NULL;
                        /* Invalid facts are validation-only; restore before disposal. */
                        inst->return_expression_use_count = inst->use_count + 1;
                        rejects_prefix = !mir_validate_terminator_provenance(
                            routine, block, b, &error) && error != NULL
                            && strstr(error, "return expression use prefix") != NULL;
                        inst->return_expression_use_count = prefix;
                        free(error);
                    }
                    if (r == 2 && inst->kind == MIR_INST_STMT
                        && inst->expr0 != NULL && inst->expr0->type == AST_CALL) {
                        MIRJsonExpressionGraph graph = {0};
                        int root = mir_json_expression_graph_build_for_routine(
                            &graph, ast_call_argument(inst->expr0, 0), routine);
                        shadow_is_local = root >= 0
                            && strcmp(graph.nodes[root].binding_kind, "none") == 0
                            && graph.nodes[root].binding_syntax_id == 0;
                        mir_json_expression_graph_dispose(&graph);
                    }
                }
            }
        }
        TEST("return expression prefix excludes only implicit copy-out uses");
        EXPECT(prefix_ok);
        TEST("invalid return use prefix is refused without projection");
        EXPECT(rejects_prefix);
        if (r == 2) {
            TEST("same-spelled local read is not stamped as a formal parameter");
            EXPECT(shadow_is_local && mir_json_routine_local_refs_required(routine));
        }
    }
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_mir_nominal_field_binding_identity(void)
{
    const char *sources[] = {
        "subject Host { let mut n: Int; action Change(self) -> Void {"
        " n = n + 1; unsafe { let n: Int = 8; Log(n); } } }"
        "func Main() -> Void {}",
        "class Host { let mut n: Int; func Change(inout self) -> Void {"
        " n = n + 1; unsafe { let n: Int = 8; Log(n); } } }"
        "func Main() -> Void {}"
    };
    for (size_t test = 0; test < 2; test++) {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_mir_from_source(sources[test], &hir, &rir, &mir);
        MIRRoutine *routine = ok ? find_mir_routine_mut(
            mir, "Change", MIR_SCOPE_METHOD) : NULL;
        size_t field_writes = 0;
        if (routine != NULL) {
            for (size_t b = 0; b < routine->block_count; b++) {
                MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    MIRInstruction *inst = &block->instructions[i];
                    if (inst->kind != MIR_INST_ASSIGN
                        || !ast_identifier_binding_is_host_field(inst->expr0))
                        continue;
                    uint32_t field_id = ast_identifier_binding_syntax_id(inst->expr0);
                    field_writes++;
                    ok &= field_id != 0 && inst->arg1 != NULL
                        && strcmp(inst->arg1, "owner_field") == 0;
                    for (size_t n = 0; n < routine->ssa_binding_count; n++)
                        ok &= routine->ssa_bindings[n].binding_syntax_id != field_id;
                    for (size_t u = 0; u < inst->use_count; u++)
                        ok &= strcmp(inst->uses[u], "n.0") != 0;
                }
            }
        }
        TEST("nominal field writes retain hosted identity instead of a local SSA entry");
        EXPECT(ok && field_writes == 1);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
