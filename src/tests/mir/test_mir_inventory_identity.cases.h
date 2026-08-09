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
