static void
test_mir_collection_ownership_validation(void)
{
    const char *source =
        "func Main() -> Void {\n"
        "    let values: HashMap<String, Int> = MapNew();\n"
        "    MapSet(values, \"owned\", 1);\n"
        "    let keys: Array<String> = MapKeys(values);\n"
        "    ArrayDropOwnedStrings(keys);\n"
        "}\n";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    MIRRoutine *routine = NULL;
    MIRCollectionOwnershipFact *fact = NULL;
    MIRSourceLocalType *target = NULL;
    char *error = NULL;
    bool baseline = false;
    bool rejects_function = false;
    bool rejects_binding = false;
    bool rejects_origin_id = false;
    bool rejects_source = false;
    bool rejects_ownership = false;
    bool rejects_disposition = false;
    bool rejects_origin = false;
    bool rejects_unknown_owned = false;
    bool rejects_retired_unknown = false;
    bool rejects_target_type = false;
    bool rejects_storage_mismatch = false;

    if (lower_mir_from_source(source, &hir, &rir, &mir))
        routine = find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION);
    if (routine != NULL && routine->collection_ownership_fact_count == 1) {
        fact = &routine->collection_ownership_facts[0];
        for (size_t i = 0; i < routine->source_local_type_count; i++) {
            if (routine->source_local_types[i].binding_syntax_id
                == fact->binding_syntax_id) {
                target = &routine->source_local_types[i];
                break;
            }
        }
    }
    if (fact != NULL && target != NULL) {
        uint32_t u32;
        PgyStringArrayOwnership ownership;
        PgyCollectionDisposition disposition;
        PgyCollectionOrigin origin;
        const char *type_name;
        size_t count;

        baseline = mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;

        u32 = fact->function_syntax_id;
        fact->function_syntax_id = u32 + 1;
        rejects_function = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->function_syntax_id = u32;

        u32 = fact->binding_syntax_id;
        fact->binding_syntax_id = 0;
        rejects_binding = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->binding_syntax_id = u32;

        u32 = fact->origin_syntax_id;
        fact->origin_syntax_id = 0;
        rejects_origin_id = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->origin_syntax_id = u32;

        u32 = fact->source_binding_syntax_id;
        fact->source_binding_syntax_id = fact->binding_syntax_id;
        rejects_source = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->source_binding_syntax_id = u32;

        ownership = fact->element_ownership;
        fact->element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
        rejects_ownership = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->element_ownership = ownership;

        disposition = fact->disposition;
        fact->disposition = (PgyCollectionDisposition)99;
        rejects_disposition = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->disposition = disposition;

        origin = fact->origin;
        fact->origin = (PgyCollectionOrigin)99;
        rejects_origin = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->origin = origin;

        origin = fact->origin;
        ownership = fact->element_ownership;
        disposition = fact->disposition;
        fact->origin = PGY_COLLECTION_ORIGIN_UNKNOWN;
        fact->element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact->disposition = PGY_COLLECTION_DISPOSITION_LIVE;
        rejects_unknown_owned =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        fact->element_ownership = PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN;
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        rejects_retired_unknown =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        fact->origin = origin;
        fact->element_ownership = ownership;
        fact->disposition = disposition;

        type_name = target->type_name;
        target->type_name = "Array<Int>";
        rejects_target_type = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; target->type_name = (char *)type_name;

        count = routine->collection_ownership_fact_count;
        routine->collection_ownership_fact_count = 0;
        rejects_storage_mismatch =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        routine->collection_ownership_fact_count = count;
    }

    TEST("MIR collection ownership carrier rejects in-memory mutations");
    EXPECT(baseline && rejects_function && rejects_binding &&
           rejects_origin_id && rejects_source && rejects_ownership &&
           rejects_disposition && rejects_origin &&
           rejects_unknown_owned && rejects_retired_unknown &&
           rejects_target_type &&
           rejects_storage_mismatch);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}
