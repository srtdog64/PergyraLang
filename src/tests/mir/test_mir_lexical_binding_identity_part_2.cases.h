
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
        " unsafe { let label: String = \"inner\"; label = Concat(label, \"!\"); Log(label); } return n; }"
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
            /* A local cannot reuse a parameter's name (docs/206 section 3). */
            TEST("local read beside an inout formal is not stamped as a formal parameter");
            EXPECT(shadow_is_local);
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
        "subject Host { var n: Int; action Change(self) -> Void {"
        " n = n + 1; unsafe { let n: Int = 8; Log(n); } } }"
        "func Main() -> Void {}",
        "class Host { var n: Int; func Change(inout self) -> Void {"
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
