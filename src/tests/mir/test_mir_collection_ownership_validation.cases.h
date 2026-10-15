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
    const char *empty_source =
        "func Main() -> Void {\n"
        "    let values: Array<String> = [];\n"
        "    Log(ArrayLength(values));\n"
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
    bool empty_baseline = false;
    bool rejects_empty_owned = false;

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

    {
        HIRProgram *empty_hir = NULL;
        RIRProgram *empty_rir = NULL;
        MIRProgram *empty_mir = NULL;
        MIRRoutine *empty_routine = NULL;
        MIRCollectionOwnershipFact *empty_fact = NULL;

        if (lower_mir_from_source(
                empty_source, &empty_hir, &empty_rir, &empty_mir))
            empty_routine = find_mir_routine_mut(
                empty_mir, "Main", MIR_SCOPE_FUNCTION);
        if (empty_routine != NULL
            && empty_routine->collection_ownership_fact_count == 1) {
            empty_fact = &empty_routine->collection_ownership_facts[0];
            empty_baseline =
                empty_fact->element_ownership
                    == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
                && empty_fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
                && empty_fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && empty_fact->source_binding_syntax_id == 0
                && mir_validate_collection_ownership_facts(
                    empty_routine, &error);
            free(error); error = NULL;
            if (empty_baseline) {
                empty_fact->element_ownership =
                    PGY_STRING_ARRAY_OWNED_ELEMENTS;
                rejects_empty_owned =
                    !mir_validate_collection_ownership_facts(
                        empty_routine, &error);
                free(error); error = NULL;
            }
        }
        mir_destroy(empty_mir);
        rir_destroy(empty_rir);
        hir_destroy(empty_hir);
    }

    TEST("MIR collection ownership carrier rejects in-memory mutations");
    EXPECT(baseline && rejects_function && rejects_binding &&
           rejects_origin_id && rejects_source && rejects_ownership &&
           rejects_disposition && rejects_origin &&
           rejects_unknown_owned && rejects_retired_unknown &&
           rejects_target_type && empty_baseline && rejects_empty_owned &&
           rejects_storage_mismatch);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);

    /* One real typed binding move, with final destination lifetime preserved.
     * This does not grant an unsupported second-hop move or revive the source. */
    for (unsigned dropped = 0; dropped < 2; dropped++) {
        const char *move_source = dropped
            ? "func Main() -> Void { let values: HashMap<String,Int> = MapNew();"
              " MapSet(values,\"owned\",1); let keys: Array<String> = MapKeys(values);"
              " let alias: Array<String> = keys; ArrayDropOwnedStrings(alias); }"
            : "func Main() -> Void { let values: HashMap<String,Int> = MapNew();"
              " MapSet(values,\"owned\",1); let keys: Array<String> = MapKeys(values);"
              " let alias: Array<String> = keys; Log(ArrayLength(alias)); }";
        HIRProgram *move_hir = NULL;
        RIRProgram *move_rir = NULL;
        MIRProgram *move_mir = NULL;
        MIRRoutine *move = NULL;
        if (lower_mir_from_source(move_source, &move_hir, &move_rir, &move_mir))
            move = find_mir_routine_mut(move_mir, "Main", MIR_SCOPE_FUNCTION);
        MIRCollectionOwnershipFact *src = NULL, *dst = NULL;
        MIRSourceLocalType *dst_type = NULL;
        MIRInstruction *def = NULL;
        if (move != NULL && move->collection_ownership_fact_count == 2) {
            src = &move->collection_ownership_facts[0];
            dst = &move->collection_ownership_facts[1];
            for (size_t i = 0; i < move->source_local_type_count; i++)
                if (move->source_local_types[i].binding_syntax_id == dst->binding_syntax_id)
                    dst_type = &move->source_local_types[i];
            for (size_t b = 0; b < move->block_count; b++)
                for (size_t i = 0; i < move->blocks[b].instruction_count; i++)
                    if (move->blocks[b].instructions[i].binding_syntax_id == dst->binding_syntax_id)
                        def = &move->blocks[b].instructions[i];
        }
        bool control = src != NULL && dst != NULL && dst_type != NULL && def != NULL
            && src->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            && src->disposition == PGY_COLLECTION_DISPOSITION_RETIRED
            && dst->origin == PGY_COLLECTION_ORIGIN_BINDING
            && dst->source_binding_syntax_id == src->binding_syntax_id
            && dst->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
            && dst->disposition == (dropped ? PGY_COLLECTION_DISPOSITION_RETIRED
                                           : PGY_COLLECTION_DISPOSITION_LIVE)
            && mir_validate_collection_ownership_facts(move, NULL);
        TEST(dropped ? "MIR moved MapKeys destination remains retired after deep drop"
                     : "MIR moved MapKeys destination remains live before deep drop");
        EXPECT(control);
        if (control) {
            char *full_error = NULL;
            bool full_control = mir_validate(move_mir, &full_error);
            free(full_error); full_error = NULL;
            TEST("MIR full typed binding-move program is valid before corruption");
            EXPECT(full_control);
            MIRSourceLocalType *birth_type = NULL;
            for (size_t i = 0; i < move->source_local_type_count; i++)
                if (move->source_local_types[i].binding_syntax_id == src->binding_syntax_id)
                    birth_type = &move->source_local_types[i];
            TEST("MIR MapKeys birth retains actual initializer result evidence");
            EXPECT(birth_type != NULL && birth_type->initializer_call_syntax_id == src->origin_syntax_id
                && birth_type->initializer_collection_producer == 1
                && birth_type->initializer_call_value_type_name != NULL
                && strcmp(birth_type->initializer_call_value_type_name, "Array<String>") == 0);
            if (full_control && birth_type != NULL) {
                for (unsigned birth_fault = 0; birth_fault < 6; birth_fault++) {
                    MIRSourceLocalType saved_birth = *birth_type;
                    bool birth_control = mir_validate(move_mir, NULL);
                    if (birth_fault == 0) birth_type->initializer_call_syntax_id = 0;
                    if (birth_fault == 1) birth_type->initializer_call_syntax_id++;
                    if (birth_fault == 2) birth_type->initializer_collection_producer = 0;
                    if (birth_fault == 3) birth_type->initializer_collection_producer = 2;
                    if (birth_fault == 4) birth_type->initializer_call_value_type_name = NULL;
                    if (birth_fault == 5) birth_type->initializer_call_value_type_name = "Array<Int>";
                    bool birth_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                    free(full_error); full_error = NULL;
                    *birth_type = saved_birth;
                    TEST("MIR full validation rejects missing or stale actual birth evidence");
                    if (!birth_rejected) printf("\n  dropped=%u birth fault=%u not refused\n", dropped, birth_fault);
                    EXPECT(birth_control && birth_rejected && mir_validate(move_mir, NULL));
                }
            }
            if (full_control) {
                PgyCollectionDisposition saved_status = dst->disposition;
                dst->disposition = dropped ? PGY_COLLECTION_DISPOSITION_LIVE
                                           : PGY_COLLECTION_DISPOSITION_RETIRED;
                bool rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                free(full_error); full_error = NULL;
                dst->disposition = saved_status;
                TEST("MIR full validation rejects final disposition flip");
                EXPECT(rejected && mir_validate(move_mir, NULL));
                bool checked_orphan = false;
                for (size_t b = 0; b < move->block_count && !checked_orphan; b++) {
                    for (size_t i = 0; i < move->blocks[b].instruction_count; i++) {
                        MIRInstruction *candidate = &move->blocks[b].instructions[i];
                        if (candidate->has_collection_ownership_receipt || candidate->kind != MIR_INST_DEF) continue;
                        bool orphan_control = mir_validate(move_mir, NULL);
                        MIRInstruction saved_candidate = *candidate;
                        candidate->has_collection_ownership_receipt = true;
                        candidate->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_DROP;
                        candidate->collection_ownership_receiver_binding_id = dst->binding_syntax_id;
                        candidate->collection_ownership_source_binding_id = 0;
                        bool orphan_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        *candidate = saved_candidate;
                        TEST("MIR full validation rejects orphan DROP receipt on actual DEF");
                        EXPECT(orphan_control && orphan_rejected && mir_validate(move_mir, NULL));
                        checked_orphan = true; break;
                    }
                }
                TEST("MIR typed control contains an operation for orphan receipt fault");
                EXPECT(checked_orphan);
            }
            if (full_control && dropped) {
                size_t matched_drop = 0;
                for (size_t b = 0; b < move->block_count; b++) {
                    MIRBasicBlock *block = &move->blocks[b];
                    for (size_t i = 0; i < block->instruction_count; i++) {
                        ASTNode *call = block->instructions[i].expr0;
                        ASTNode *callee = call != NULL && call->type == AST_CALL ? ast_call_callee(call) : NULL;
                        if (callee == NULL || callee->type != AST_IDENTIFIER ||
                            strcmp(ast_identifier_name(callee), "ArrayDropOwnedStrings") != 0) continue;
                        matched_drop++;
                        size_t count = block->instruction_count;
                        MIRInstruction *original = block->instructions;
                        MIRInstruction *backup = malloc(count * sizeof(*backup));
                        MIRInstruction *duplicate = malloc((count + 1) * sizeof(*duplicate));
                        bool allocated = backup != NULL && duplicate != NULL;
                        TEST("MIR deep-drop mutation scratch storage exists");
                        EXPECT(allocated);
                        if (!allocated) { free(backup); free(duplicate); continue; }
                        memcpy(backup, original, count * sizeof(*backup));
                        for (unsigned receipt_fault = 0; receipt_fault < 4; receipt_fault++) {
                            bool receipt_control = mir_validate(move_mir, NULL);
                            MIRInstruction saved_drop = original[i];
                            if (receipt_fault == 0) original[i].has_collection_ownership_receipt = false;
                            if (receipt_fault == 1) original[i].collection_ownership_effect_kind = 99;
                            if (receipt_fault == 2) original[i].collection_ownership_receiver_binding_id = src->binding_syntax_id;
                            if (receipt_fault == 3) original[i].collection_ownership_source_binding_id = src->binding_syntax_id;
                            bool receipt_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                            free(full_error); full_error = NULL;
                            original[i] = saved_drop;
                            TEST("MIR full validation rejects missing or malformed deep-drop receipt");
                            if (!receipt_rejected) printf("\n  drop receipt fault=%u not refused\n", receipt_fault);
                            EXPECT(receipt_control && receipt_rejected && mir_validate(move_mir, NULL));
                        }
                        bool before_delete = mir_validate(move_mir, NULL);
                        memmove(&original[i], &original[i + 1], (count - i - 1) * sizeof(*original));
                        block->instruction_count = count - 1;
                        bool deletion_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        memcpy(original, backup, count * sizeof(*original));
                        block->instruction_count = count;
                        TEST("MIR full validation rejects deleted drop with retired final fact");
                        EXPECT(before_delete && deletion_rejected && mir_validate(move_mir, NULL));
                        bool before_duplicate = mir_validate(move_mir, NULL);
                        memcpy(duplicate, backup, (i + 1) * sizeof(*duplicate));
                        duplicate[i + 1] = backup[i];
                        memcpy(&duplicate[i + 2], &backup[i + 1], (count - i - 1) * sizeof(*duplicate));
                        block->instructions = duplicate; block->instruction_count = count + 1;
                        bool duplication_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        block->instructions = original; block->instruction_count = count;
                        TEST("MIR full validation rejects duplicated deep drop");
                        EXPECT(before_duplicate && duplication_rejected && mir_validate(move_mir, NULL));
                        free(duplicate); free(backup);
                    }
                }
                TEST("MIR typed binding-move has exactly one deep-drop operation");
                EXPECT(matched_drop == 1);
            }
            for (unsigned fault = 0; fault < 19; fault++) {
                MIRCollectionOwnershipFact saved_src = *src, saved_dst = *dst;
                MIRInstruction saved_def = *def;
                const char *saved_type = dst_type->type_name;
                size_t saved_count = move->collection_ownership_fact_count;
                switch (fault) {
                    case 0: dst->source_binding_syntax_id = 0; break;
                    case 1: dst->source_binding_syntax_id = UINT32_MAX; break;
                    case 2: dst->source_binding_syntax_id = dst->binding_syntax_id; break;
                    case 3: src->disposition = PGY_COLLECTION_DISPOSITION_LIVE; break;
                    case 4: dst->element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS; break;
                    case 5: dst->element_ownership = (PgyStringArrayOwnership)99; break;
                    case 6: dst->disposition = (PgyCollectionDisposition)99; break;
                    case 7: dst->function_syntax_id++; break;
                    case 8: dst->origin_syntax_id = 0; break;
                    case 9: dst->binding_syntax_id = 0; break;
                    case 10: dst->origin = (PgyCollectionOrigin)99; break;
                    case 11: *src = saved_dst; *dst = saved_src; break; /* source order */
                    case 12: *dst = saved_src; break; /* duplicate identity */
                    case 13: dst_type->type_name = "Array<Int>"; break;
                    case 14: def->kind = MIR_INST_STMT; break;
                    case 15: def->binding_syntax_id = 0; break;
                    case 16: move->collection_ownership_fact_count = 1; *src = saved_dst; break;
                    case 17: def->expr0 = NULL; break;
                    case 18: def->use_count = 0; break;
                }
                char *fault_error = NULL;
                bool refused = !mir_validate_collection_ownership_facts(move, &fault_error)
                    && fault_error != NULL;
                free(fault_error);
                *src = saved_src; *dst = saved_dst; *def = saved_def;
                dst_type->type_name = (char *)saved_type;
                move->collection_ownership_fact_count = saved_count;
                TEST("MIR binding-move malformed lineage refuses without lifetime rewrite");
                if (!refused) printf("\n  binding-move dropped=%u fault=%u not refused\n", dropped, fault);
                EXPECT(refused && mir_validate_collection_ownership_facts(move, NULL));
            }
        }
        mir_destroy(move_mir); rir_destroy(move_rir); hir_destroy(move_hir);
    }

    {
        const char *clone_source =
            "func Main() -> Void { let values: Array<String> = [\"borrowed\"];"
            " let cloned: Array<String> = Clone(values); ArrayDropOwnedStrings(cloned); }";
        HIRProgram *clone_hir = NULL;
        RIRProgram *clone_rir = NULL;
        MIRProgram *clone_mir = NULL;
        MIRRoutine *clone_routine = NULL;
        MIRSourceLocalType *clone_type = NULL;
        MIRCollectionOwnershipFact *clone_fact = NULL;
        bool clone_lowered = lower_mir_from_source(clone_source, &clone_hir, &clone_rir, &clone_mir);
        if (clone_lowered) clone_routine = find_mir_routine_mut(clone_mir, "Main", MIR_SCOPE_FUNCTION);
        if (clone_routine != NULL) {
            for (size_t i = 0; i < clone_routine->collection_ownership_fact_count; i++)
                if (clone_routine->collection_ownership_facts[i].origin == PGY_COLLECTION_ORIGIN_CLONE)
                    clone_fact = &clone_routine->collection_ownership_facts[i];
            for (size_t i = 0; clone_fact != NULL && i < clone_routine->source_local_type_count; i++)
                if (clone_routine->source_local_types[i].binding_syntax_id == clone_fact->binding_syntax_id)
                    clone_type = &clone_routine->source_local_types[i];
        }
        bool clone_control = clone_type != NULL && clone_fact != NULL
            && clone_type->initializer_call_syntax_id > 0
            && clone_type->initializer_call_syntax_id == clone_fact->origin_syntax_id
            && clone_type->initializer_collection_producer == 2
            && clone_type->initializer_call_value_type_name != NULL
            && strcmp(clone_type->initializer_call_value_type_name, "Array<String>") == 0
            && mir_validate(clone_mir, NULL);
        TEST("MIR full Clone deep-drop retains canonical actual result witness");
        EXPECT(clone_control);
        if (clone_control) for (unsigned fault = 0; fault < 3; fault++) {
            MIRSourceLocalType saved = *clone_type;
            bool before = mir_validate(clone_mir, NULL);
            if (fault == 0) clone_type->initializer_call_syntax_id = 0;
            if (fault == 1) clone_type->initializer_collection_producer = 0;
            if (fault == 2) clone_type->initializer_call_value_type_name = NULL;
            char *clone_error = NULL;
            bool rejected = !mir_validate(clone_mir, &clone_error) && clone_error != NULL;
            free(clone_error);
            *clone_type = saved;
            TEST("MIR full Clone rejects missing retained initializer witness");
            EXPECT(before && rejected && mir_validate(clone_mir, NULL));
        }
        mir_destroy(clone_mir); rir_destroy(clone_rir); hir_destroy(clone_hir);

        /* The missing canonical AST seal is rejected at HIR before MIR can
         * capture an owned birth. Do not mutate a retired AST flag and pretend
         * it is the native private MIR carrier's authority. */
        Lexer *clone_lexer = lexer_create(clone_source);
        Parser *clone_parser = parser_create(clone_lexer);
        ASTNode *clone_ast = parser_parse_program(clone_parser);
        SemanticResult *clone_sem = semantic_analyze(clone_ast);
        ASTNode *clone_call = NULL;
        if (!parser_has_error(clone_parser) && clone_sem != NULL && clone_sem->success) {
            ASTNode *main_decl = ast_program_statement(clone_sem->annotated_ast, 0);
            clone_call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 1));
        }
        HIRSemanticProjectionFailure clone_failure = HIR_SEMANTIC_PROJECTION_NONE;
        char *clone_error = NULL;
        HIRProgram *canonical_hir = clone_call != NULL
            ? hir_lower_with_semantic_facts(clone_sem, &clone_failure, &clone_error) : NULL;
        bool canonical_control = canonical_hir != NULL && clone_error == NULL
            && clone_failure == HIR_SEMANTIC_PROJECTION_NONE
            && ast_call_semantic_callee_is_stdlib(clone_call);
        hir_destroy(canonical_hir); free(clone_error); clone_error = NULL;
        bool canonical_refused = false;
        if (canonical_control) {
            bool saved_flag = clone_call->data.call.semantic_callee_is_stdlib;
            clone_call->data.call.semantic_callee_is_stdlib = false;
            canonical_hir = hir_lower_with_semantic_facts(clone_sem, &clone_failure, &clone_error);
            canonical_refused = canonical_hir == NULL && clone_error != NULL
                && clone_failure == HIR_SEMANTIC_PROJECTION_COLLECTION_OWNERSHIP;
            clone_call->data.call.semantic_callee_is_stdlib = saved_flag;
            hir_destroy(canonical_hir); free(clone_error);
        }
        TEST("HIR to MIR boundary refuses missing canonical Clone seal");
        EXPECT(canonical_control && canonical_refused);
        semantic_result_destroy(clone_sem); ast_destroy(clone_ast);
        parser_destroy(clone_parser); lexer_destroy(clone_lexer);
    }

    hir = NULL;
    rir = NULL;
    mir = NULL;
    bool formal_control = lower_mir_from_source(
        "struct Holder { rows: Array<String>; }"
        "func Inspect(own values: Holder, ref other: Holder) -> Void {"
        " let alias: Array<String> = values.rows; Log(ArrayLength(alias)); }"
        " func Main() -> Void {}", &hir, &rir, &mir);
    routine = formal_control
        ? find_mir_routine_mut(mir, "Inspect", MIR_SCOPE_FUNCTION) : NULL;
    bool duplicate_formal = false;
    if (routine != NULL && routine->param_count == 2) {
        formal_control &= routine->collection_ownership_fact_count == 1
            && mir_validate_collection_ownership_facts(routine, NULL);
        FuncParam *second = routine->params[1];
        routine->params[1] = routine->params[0];
        duplicate_formal = !mir_validate_collection_ownership_facts(routine,
            &error) && error != NULL;
        routine->params[1] = second;
        free(error);
    }
    TEST("MIR collection source type refuses duplicate formal identity");
    if (!formal_control || !duplicate_formal)
        printf("\n  formal identity fixture: control=%d rows=%zu params=%zu duplicate_refused=%d\n",
            (int)formal_control,
            routine != NULL ? routine->collection_ownership_fact_count : 0,
            routine != NULL ? routine->param_count : 0, (int)duplicate_formal);
    EXPECT(formal_control && duplicate_formal);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}
