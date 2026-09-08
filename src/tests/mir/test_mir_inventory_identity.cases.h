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
