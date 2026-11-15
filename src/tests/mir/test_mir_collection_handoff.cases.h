/* The native semantic hand-off (decision 2026-11-15) reaches MIR on exactly
 * the edges it is issued for, and MIR validation refuses a forged one. */

/* The disposition of local `name` of routine `routine_name`, or 0 when the
 * program does not lower or has no such row. */
static PgyCollectionDisposition
mir_handoff_disposition(const char *source, const char *routine_name,
                        const char *name)
{
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    PgyCollectionDisposition disposition = (PgyCollectionDisposition)0;
    MIRRoutine *routine = lower_mir_from_landed_source(source, &hir, &rir, &mir)
        ? find_mir_routine_mut(mir, routine_name, MIR_SCOPE_FUNCTION) : NULL;
    for (size_t i = 0; routine != NULL && i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *local = &routine->source_local_types[i];
        if (local->name == NULL || strcmp(local->name, name) != 0) continue;
        const MIRCollectionOwnershipFact *fact =
            mir_routine_collection_ownership_fact(routine, local->binding_syntax_id);
        if (fact != NULL) disposition = fact->disposition;
    }
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    return disposition;
}

static void
test_mir_collection_handoff(void)
{
    const char *constructor =
        "struct Pack { texts: Array<String>; }\n"
        "func Main() -> Void {\n"
        "    var texts: Array<String> = [];\n"
        "    ArrayPushOwnedString(texts, Concat(\"owned\", \"-text\"));\n"
        "    let pack: Pack = Pack(texts);\n"
        "    Log(ArrayLength(pack.texts));\n"
        "}\n";
    const char *grown =
        "func GrowCount(values: Array<String>) -> Int {\n"
        "    var grown: Array<String> = values;\n"
        "    ArrayPush(grown, \"b\"); ArrayPush(grown, \"c\");\n"
        "    ArrayPush(grown, \"d\"); ArrayPush(grown, \"e\");\n"
        "    return ArrayLength(grown);\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let names: Array<String> = [\"a\"];\n"
        "    Log(GrowCount(names));\n"
        "}\n";
    const char *read_only =
        "func Count(values: Array<String>) -> Int { return ArrayLength(values); }\n"
        "func Main() -> Void {\n"
        "    let names: Array<String> = [\"a\"];\n"
        "    Log(Count(names));\n"
        "    Log(names[0]);\n"
        "}\n";
    const char *twice =
        "struct Rows { names: Array<String>; }\n"
        "func Main() -> Void {\n"
        "    var names: Array<String> = [];\n"
        "    ArrayPushOwnedString(names, Concat(\"a\", \"\"));\n"
        "    let first: Rows = Rows(names);\n"
        "    let second: Rows = Rows(names);\n"
        "    Log(ArrayLength(first.names) + ArrayLength(second.names));\n"
        "}\n";
    const char *in_loop =
        "struct Rows { names: Array<String>; }\n"
        "func Main() -> Void {\n"
        "    let names: Array<String> = [\"a\"];\n"
        "    var i: Int = 0;\n"
        "    while i < 2 {\n"
        "        let rows: Rows = Rows(names);\n"
        "        Log(ArrayLength(rows.names));\n"
        "        i = i + 1;\n"
        "    }\n"
        "}\n";
    const char *redefined =
        "struct Rows { names: Array<String>; }\n"
        "func Main() -> Void {\n"
        "    var names: Array<String> = [\"a\"];\n"
        "    names = [\"b\"];\n"
        "    let rows: Rows = Rows(names);\n"
        "    Log(ArrayLength(rows.names));\n"
        "}\n";

    TEST("MIR carries the native hand-off of a local a record takes");
    EXPECT(mir_handoff_disposition(constructor, "Main", "texts")
        == PGY_COLLECTION_DISPOSITION_HANDED_OFF);
    TEST("MIR carries the native hand-off of a local a growing callee takes");
    EXPECT(mir_handoff_disposition(grown, "Main", "names")
        == PGY_COLLECTION_DISPOSITION_HANDED_OFF);
    TEST("MIR keeps live a local a reading callee only observes");
    EXPECT(mir_handoff_disposition(read_only, "Main", "names")
        == PGY_COLLECTION_DISPOSITION_LIVE);
    /* The first record takes the Clone the move owner lands there, so the
     * local is the second record's only input. */
    TEST("MIR hands a local given to two records to the second");
    EXPECT(mir_handoff_disposition(twice, "Main", "names")
        == PGY_COLLECTION_DISPOSITION_HANDED_OFF);
    TEST("MIR keeps live a local a record in a loop takes again");
    EXPECT(mir_handoff_disposition(in_loop, "Main", "names")
        == PGY_COLLECTION_DISPOSITION_LIVE);
    TEST("MIR keeps live a local with a second definition");
    EXPECT(mir_handoff_disposition(redefined, "Main", "names")
        == PGY_COLLECTION_DISPOSITION_LIVE);

    /* A hand-off forged onto a member move, whose storage returns to its
     * aggregate, is refused by the MIR validator. */
    {
        const char *member =
            "struct Pack { texts: Array<String>; }\n"
            "func Main() -> Void {\n"
            "    var texts: Array<String> = [];\n"
            "    ArrayPushOwnedString(texts, Concat(\"owned\", \"-text\"));\n"
            "    let pack: Pack = Pack(texts);\n"
            "    let moved: Array<String> = pack.texts;\n"
            "    Log(ArrayLength(moved));\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRCollectionOwnershipFact *moved = NULL;
        char *error = NULL;
        bool baseline = false, refused = false;
        MIRRoutine *routine = lower_mir_from_landed_source(member, &hir, &rir, &mir)
            ? find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
        for (size_t i = 0; routine != NULL && i < routine->collection_ownership_fact_count; i++)
            if (routine->collection_ownership_facts[i].origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE)
                moved = &routine->collection_ownership_facts[i];
        if (moved != NULL) {
            baseline = mir_validate_collection_ownership_facts(routine, &error);
            free(error); error = NULL;
            PgyCollectionDisposition saved = moved->disposition;
            moved->disposition = PGY_COLLECTION_DISPOSITION_HANDED_OFF;
            refused = !mir_validate_collection_ownership_facts(routine, &error);
            free(error); error = NULL;
            moved->disposition = saved;
        }
        TEST("MIR validation refuses a hand-off forged onto a member move");
        EXPECT(moved != NULL && baseline && refused);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
