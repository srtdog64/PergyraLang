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
test_mir_resource_summary_operand_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Main() -> Void { let slot: Slot<Int> = ClaimSlot();"
        " Write(slot, 41); pin slot as view: ReadView<Int> {"
        " let observed: Int = Read(view); Log(observed); } }",
        &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    MIRInstruction *view = NULL, *read = NULL;
    size_t summaries = 0;
    bool summary_reads = false;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind == MIR_INST_RESOURCE_OP) {
                    summaries++;
                    summary_reads |= inst->use_count != 0;
                }
                if (inst->kind != MIR_INST_DEF || inst->arg0 == NULL) continue;
                if (strcmp(inst->arg0, "view") == 0) view = inst;
                if (strcmp(inst->arg0, "observed") == 0) read = inst;
            }
        }
    }
    TEST("resource summaries retain evidence without pre-scope SSA reads");
    EXPECT(ok && summaries >= 4 && !summary_reads);
    TEST("resource view read consumes the exact lexical DEF");
    EXPECT(view != NULL && read != NULL && read->use_count == 1
        && strcmp(read->uses[0], view->result_name) == 0);
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
    TEST("resource read missing lexical identity still refuses before emission");
    EXPECT(refused);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_ast_growth_preserves_binding_identity(void)
{
    Lexer *lexer = lexer_create("func Keep(x: Int) -> Void { let (a, b) = (x, 2); }");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    ASTNode *func = ast_program_statement(program, 0);
    ASTNode *body = ast_func_body(func);
    ASTNode *destructure = ast_block_statement(body, 0);
    FuncParam *param = ast_func_param(func, 0);
    uint32_t root_id = ast_node_stable_id(program);
    uint32_t param_id = ast_func_param_stable_id(param);
    uint32_t binding_id = ast_let_destructure_binding_stable_id(destructure, 0);
    ASTNode *block = ast_create_block();
    ASTNode *local = ast_create_let_declaration("later");
    ast_add_statement(block, local);
    ast_add_statement(body, block);
    bool completed = !parser_has_error(parser) && ast_complete_stable_ids(program);
    TEST("AST growth preserves node, delayed formal and destructure identities");
    EXPECT(completed && root_id != 0 && param_id != 0 && binding_id != 0
        && ast_node_stable_id(program) == root_id
        && ast_func_param_stable_id(param) == param_id
        && ast_let_destructure_binding_stable_id(destructure, 0) == binding_id);
    uint32_t local_id = ast_node_stable_id(local);
    uint32_t block_id = ast_node_stable_id(block);
    TEST("synthetic identities extend beyond the existing delayed formal namespace");
    EXPECT(completed && local_id > param_id && block_id > param_id && local_id != block_id);
    TEST("repeated AST completion does not renumber completed identities");
    EXPECT(ast_complete_stable_ids(program) && ast_node_stable_id(local) == local_id
        && ast_node_stable_id(block) == block_id);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);

    block = ast_create_block();
    local = ast_create_identifier("pending");
    block->stable_id = UINT32_MAX;
    ast_add_statement(block, local);
    TEST("synthetic identity exhaustion refuses without reusing an existing key");
    EXPECT(!ast_complete_stable_ids(block) && ast_node_stable_id(local) == 0
        && ast_node_stable_id(block) == UINT32_MAX);
    ast_destroy(block);
}

static void
test_mir_select_receive_binding_type_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Main() -> Void { let ch: Channel<Int> = Channel(2); ch <- 2;"
        " select { case v = <-ch: Log(v); default: Log(0); } }",
        &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    bool typed = false;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (mir_instruction_uses_select_receive_statement_emit(inst))
                    typed = inst->abi_type_name != NULL
                        && strcmp(inst->abi_type_name, "Int") == 0;
            }
        }
    }
    TEST("select receive DEF carries its checker-owned binding type");
    EXPECT(ok && typed);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

static void
test_mir_destructure_output_identity(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Main() -> Void { let (n, s) = (42, \"outer\"); Log(n); Log(s);"
        " unsafe { let (n, s) = (7, \"inner\"); Log(n); Log(s); } Log(n); }",
        &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    MIRInstruction *outputs[2] = {0};
    size_t count = 0;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind == MIR_INST_DESTRUCTURE && count < 2)
                    outputs[count++] = inst;
            }
        }
    }
    bool distinct = count == 2 && outputs[0]->destructure_result_names != NULL
        && outputs[1]->destructure_result_names != NULL;
    for (size_t d = 0; distinct && d < 2; d++)
        distinct = outputs[0]->destructure_binding_ids[d] != outputs[1]->destructure_binding_ids[d]
            && strcmp(outputs[0]->destructure_result_names[d], outputs[1]->destructure_result_names[d]) != 0;
    TEST("destructure outputs preserve shadowed positional SSA identities");
    EXPECT(ok && distinct && mir_validate(mir, NULL));
    bool independent = false, missing = false, crosswired = false;
    char *error = NULL;
    if (count == 2) {
        ASTNode *ast = outputs[0]->ast;
        outputs[0]->ast = NULL;
        independent = mir_populate_use_edges(routine, &error);
        free(error); error = NULL;
        uint32_t id = outputs[0]->destructure_binding_ids[0];
        outputs[0]->destructure_binding_ids[0] = 0;
        missing = !mir_populate_use_edges(routine, &error) && error != NULL
            && strstr(error, "positional SSA binding identity") != NULL;
        free(error); error = NULL;
        outputs[0]->destructure_binding_ids[0] = outputs[0]->destructure_binding_ids[1];
        crosswired = !mir_populate_use_edges(routine, &error) && error != NULL
            && strstr(error, "positional SSA binding identity") != NULL;
        outputs[0]->destructure_binding_ids[0] = id;
        outputs[0]->ast = ast;
    }
    TEST("destructure SSA projection no longer reads the statement AST payload");
    EXPECT(independent);
    TEST("missing destructure identity refuses before code emission"); EXPECT(missing);
    TEST("crosswired destructure identity refuses before code emission"); EXPECT(crosswired);
    free(error);
    mir_destroy(mir); rir_destroy(rir); hir_destroy(hir);
}

static void
test_mir_builtin_before_callable_shadow(void)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(
        "func Main() -> Int { let before: Int = StringLength(\"abc\");"
        " let StringLength: func(Int) -> Int = (value: Int) => value + 2;"
        " return StringLength(before); }", &hir, &rir, &mir);
    MIRRoutine *routine = ok ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
    MIRInstruction *builtin = NULL, *local_call = NULL;
    if (routine != NULL) {
        for (size_t b = 0; b < routine->block_count; b++) {
            MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL && strcmp(inst->arg0, "before") == 0)
                    builtin = inst;
                if (inst->kind == MIR_INST_RETURN && inst->expr0 != NULL && inst->expr0->type == AST_CALL)
                    local_call = inst;
            }
        }
    }
    TEST("builtin before local callable shadow is not a read of the later binding");
    EXPECT(ok && builtin != NULL && builtin->use_count == 0 && local_call != NULL
        && local_call->use_count == 2);
    bool missing = false;
    char *error = NULL;
    if (local_call != NULL) {
        ASTNode *callee = ast_call_callee(local_call->expr0);
        uint32_t id = ast_identifier_binding_syntax_id(callee);
        ast_identifier_set_binding_syntax_id(callee, 0);
        missing = !mir_populate_use_edges(routine, &error) && error != NULL
            && strstr(error, "semantic binding identity") != NULL;
        ast_identifier_set_binding_syntax_id(callee, id);
    }
    TEST("local callable shadow still requires its exact binding identity"); EXPECT(missing);
    free(error);
    error = NULL;
    bool missing_stdlib = false;
    if (builtin != NULL && builtin->expr0 != NULL) {
        ast_call_set_semantic_callee_is_stdlib(builtin->expr0, false);
        missing_stdlib = !mir_populate_use_edges(routine, &error) && error != NULL
            && strstr(error, "semantic binding identity") != NULL;
        ast_call_set_semantic_callee_is_stdlib(builtin->expr0, true);
    }
    TEST("missing standard-library target fact is not repaired from its spelling"); EXPECT(missing_stdlib);
    free(error);
    mir_destroy(mir); rir_destroy(rir); hir_destroy(hir);
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

static void
test_mir_inventory_source_identity_lookup(void)
{
    MIRRoutine routines[2];
    MIRRoutineInventory inventory;
    MIRRoutineSourceLookup lookup;
    bool invalid_rejected;
    bool missing_rejected;
    bool unique_found;
    bool duplicate_rejected;
    bool method_keeps_kind;

    memset(routines, 0, sizeof(routines));
    inventory.routines = routines;
    inventory.count = 2;

    lookup = mir_routine_inventory_find_unique_by_source_syntax_id(
        &inventory, 0);
    invalid_rejected = lookup.status == MIR_ROUTINE_SOURCE_LOOKUP_INVALID
        && lookup.routine == NULL;

    lookup = mir_routine_inventory_find_unique_by_source_syntax_id(
        &inventory, 17);
    missing_rejected = lookup.status == MIR_ROUTINE_SOURCE_LOOKUP_MISSING
        && lookup.routine == NULL;

    routines[0].source_syntax_id = 7;
    routines[0].kind = MIR_SCOPE_FUNCTION;
    lookup = mir_routine_inventory_find_unique_by_source_syntax_id(
        &inventory, 7);
    unique_found = lookup.status == MIR_ROUTINE_SOURCE_LOOKUP_UNIQUE
        && lookup.routine == &routines[0];

    routines[1].source_syntax_id = 7;
    lookup = mir_routine_inventory_find_unique_by_source_syntax_id(
        &inventory, 7);
    duplicate_rejected =
        lookup.status == MIR_ROUTINE_SOURCE_LOOKUP_DUPLICATE
        && lookup.routine == NULL;

    routines[1].source_syntax_id = 8;
    routines[1].kind = MIR_SCOPE_METHOD;
    lookup = mir_routine_inventory_find_unique_by_source_syntax_id(
        &inventory, 8);
    method_keeps_kind = lookup.status == MIR_ROUTINE_SOURCE_LOOKUP_UNIQUE
        && lookup.routine == &routines[1]
        && mir_routine_kind(lookup.routine) == MIR_SCOPE_METHOD;

    TEST("MIR source identity lookup distinguishes invalid/missing/duplicate/method");
    EXPECT(invalid_rejected && missing_rejected && unique_found
        && duplicate_rejected && method_keeps_kind);
}

static void
test_mir_decl_header_storage_layout_receipt(void)
{
    bool exact = MIR_DECL_HEADER_STORAGE_LAYOUT_MATCHES_LOCAL();
    bool size_skew = mir_decl_header_storage_layout_matches(
        sizeof(MIRDeclHeader) + 1, _Alignof(MIRDeclHeader),
        offsetof(MIRDeclHeader, method_metadata),
        offsetof(MIRDeclHeader, abi_layout),
        offsetof(MIRDeclHeader, option_abi_type_name),
        offsetof(MIRDeclHeader, option_abi_layout_id));
    bool offset_skew = mir_decl_header_storage_layout_matches(
        sizeof(MIRDeclHeader), _Alignof(MIRDeclHeader),
        offsetof(MIRDeclHeader, method_metadata),
        offsetof(MIRDeclHeader, abi_layout),
        offsetof(MIRDeclHeader, option_abi_type_name) + 1,
        offsetof(MIRDeclHeader, option_abi_layout_id));

    TEST("MIR declaration header storage layout rejects partial-link skew");
    EXPECT(exact && !size_skew && !offset_skew);
}

static void
test_mir_routine_generic_constraint_carriage(void)
{
    const char *source =
        "func Bounds<T, U>(x: T, y: U) -> Int where T: Int, U: Bool { return 1; }\n"
        "func Open<V>(x: V) -> Void { return; }\n"
        "func Main() -> Void { Bounds(1, true); Open(2); }\n";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    bool ok = lower_mir_from_source(source, &hir, &rir, &mir);
    MIRRoutine *bounds = ok
        ? (MIRRoutine *)find_mir_routine(mir, "Bounds", MIR_SCOPE_FUNCTION) : NULL;
    const MIRRoutine *open = ok
        ? find_mir_routine(mir, "Open", MIR_SCOPE_FUNCTION) : NULL;
    bool exact = bounds != NULL && open != NULL
        && mir_routine_generic_param_count(bounds) == 2
        && mir_routine_generic_param_constraint(bounds, 0) != NULL
        && mir_routine_generic_param_constraint(bounds, 1) != NULL
        && strcmp(mir_routine_generic_param_constraint(bounds, 0), "Int") == 0
        && strcmp(mir_routine_generic_param_constraint(bounds, 1), "Bool") == 0
        && mir_routine_generic_param_constraint(open, 0) != NULL
        && strcmp(mir_routine_generic_param_constraint(open, 0), "") == 0;
    TEST("MIR signature preserves ordered bounds and explicit unbounded fact");
    EXPECT(exact && mir_validate(mir, NULL));
    bool missing_row_rejected = false;
    bool missing_table_rejected = false;
    if (exact) {
        char *saved = bounds->generic_param_constraints[0];
        bounds->generic_param_constraints[0] = NULL;
        missing_row_rejected = !mir_validate(mir, NULL);
        bounds->generic_param_constraints[0] = saved;
        char **saved_table = bounds->generic_param_constraints;
        bounds->generic_param_constraints = NULL;
        missing_table_rejected = !mir_validate(mir, NULL);
        bounds->generic_param_constraints = saved_table;
    }
    TEST("MIR signature rejects missing bound row or constraint table");
    EXPECT(missing_row_rejected && missing_table_rejected && mir_validate(mir, NULL));
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}
