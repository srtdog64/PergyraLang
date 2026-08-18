static void
test_mir_lowering_part_b_3(void)
{
    TEST("MIR owns typed intent transitions and rejects crosswired plans");
    {
        const char *src =
            "tobject ReceiptA { value: Int; }\n"
            "tobject ProblemA { code: Int; }\n"
            "tobject ReceiptB { value: Int; }\n"
            "tobject ProblemB { code: Int; }\n"
            "enum OutcomeA { GoodA(ReceiptA), BadA(ProblemA), }\n"
            "enum OutcomeB { GoodB(ReceiptB), BadB(ProblemB), }\n"
            "enum WorkflowOutcome {\n"
            "    Committed(ReceiptB),\n"
            "    FailedA(ProblemA),\n"
            "    FailedB(ProblemB),\n"
            "}\n"
            "ability Runnable { func CanRun() -> Bool; }\n"
            "subject Actor {\n"
            "    action ForwardA(self, succeed: Bool) -> OutcomeA\n"
            "        requires Runnable within WorkZone authorized by self\n"
            "    {\n"
            "        if succeed { return GoodA(ReceiptA(10)); }\n"
            "        return BadA(ProblemA(101));\n"
            "    }\n"
            "    action ForwardB(self, value: Int, succeed: Bool) -> OutcomeB\n"
            "        requires Runnable within WorkZone authorized by self\n"
            "    {\n"
            "        if succeed { return GoodB(ReceiptB(value)); }\n"
            "        return BadB(ProblemB(202));\n"
            "    }\n"
            "    action UndoA(self) -> Void { return; }\n"
            "    action UndoB(self) -> Void { return; }\n"
            "}\n"
            "role ActorRunnable for Actor {\n"
            "    impl ability Runnable {\n"
            "        func CanRun() -> Bool { return true; }\n"
            "    }\n"
            "}\n"
            "zone WorkZone {\n"
            "    subject slot actor: Actor\n"
            "    authority actor requires Runnable\n"
            "}\n"
            "intent RunWorkflow(\n"
            "    work: WorkZone, actor: Actor, first: Bool, second: Bool\n"
            ") -> WorkflowOutcome {\n"
            "    step A {\n"
            "        using: work;\n"
            "        on outcome_a: actor.ForwardA(first);\n"
            "        success: GoodA(receipt_a);\n"
            "        failure: BadA(problem_a);\n"
            "        compensate: actor.UndoA();\n"
            "    }\n"
            "    step B after A {\n"
            "        using: work;\n"
            "        on outcome_b: actor.ForwardB(receipt_a.value, second);\n"
            "        success: GoodB(receipt_b);\n"
            "        failure: BadB(problem_b);\n"
            "        compensate: actor.UndoB();\n"
            "    }\n"
            "    success B: Committed(receipt_b);\n"
            "    failure A: FailedA(problem_a);\n"
            "    failure B: FailedB(problem_b);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRIntentStepTransitionFact *root = NULL;
        MIRIntentStepTransitionFact *child = NULL;
        char *mir_error = NULL;
        bool exact_shape = false;
        bool rejected_variant = false;
        bool rejected_payload = false;
        bool rejected_payload_identity = false;
        bool rejected_action_identity = false;
        bool rejected_predecessor = false;
        bool rejected_completion = false;
        bool rejected_terminal_variant = false;
        bool final_valid = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok)
            routine = find_mir_routine_mut(
                mir, "RunWorkflow", MIR_SCOPE_INTENT);
        if (routine != NULL
            && routine->intent_step_transition_count == 2) {
            size_t completions = 0;
            size_t failure_completions = 0;
            size_t success_terminals = 0;
            size_t failure_terminals = 0;

            for (size_t i = 0;
                 i < routine->intent_step_transition_count; i++) {
                MIRIntentStepTransitionFact *row =
                    &routine->intent_step_transitions[i];
                if (row->has_predecessor)
                    child = row;
                else
                    root = row;
            }
            for (size_t b = 0; b < routine->block_count; b++) {
                for (size_t i = 0;
                     i < routine->blocks[b].instruction_count; i++) {
                    const MIRInstruction *inst =
                        &routine->blocks[b].instructions[i];
                    if (!mir_instruction_is_intent_stmt(
                            inst, "IntentStepCompleted")) {
                        continue;
                    }
                    completions++;
                    if ((root != NULL
                         && b == root->failure.successor_block_id)
                        || (child != NULL
                            && b == child->failure.successor_block_id)) {
                        failure_completions++;
                    }
                }
            }
            for (size_t i = 0;
                 i < routine->intent_terminal_transition_count; i++) {
                if (routine->intent_terminal_transitions[i].role
                    == MIR_INTENT_TERMINAL_SUCCESS) {
                    success_terminals++;
                } else if (routine->intent_terminal_transitions[i].role
                           == MIR_INTENT_TERMINAL_FAILURE) {
                    failure_terminals++;
                }
            }
            exact_shape = routine->intent_execution_plan_admitted
                && routine->has_signature
                && routine->return_type_name != NULL
                && strcmp(routine->return_type_name,
                          "WorkflowOutcome") == 0
                && routine->intent_terminal_transition_count == 3
                && root != NULL && child != NULL
                && child->predecessor_transition_id
                    == root->transition_id
                && child->predecessor_step_syntax_id
                    == root->step_syntax_id
                && child->predecessor_step_name != NULL
                && strcmp(child->predecessor_step_name,
                          root->step_name) == 0
                && completions == 2
                && failure_completions == 0
                && success_terminals == 1
                && failure_terminals == 2;
        }
        if (routine != NULL && root != NULL && child != NULL
            && routine->intent_terminal_transition_count == 3) {
            size_t saved_variant = root->success.variant_index;
            const char *saved_payload = root->success.payload_type_name;
            uint32_t saved_payload_identity =
                root->success.payload_decl_syntax_id;
            uint32_t saved_action_identity = root->action_syntax_id;
            uint32_t saved_predecessor =
                child->predecessor_transition_id;
            size_t saved_completion = child->completion_instruction_id;
            MIRIntentTerminalTransitionFact *terminal =
                &routine->intent_terminal_transitions[0];
            size_t saved_terminal_variant = terminal->result_variant_index;

            root->success.variant_index = root->failure.variant_index;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_variant = !mir_validate(mir, &mir_error);
            if (!rejected_variant) {
                fprintf(stderr,
                    "[mir-intent-execution-test] variant drift was accepted\n");
            }
            free(mir_error);
            mir_error = NULL;
            root->success.variant_index = saved_variant;

            root->success.payload_type_name =
                root->failure.payload_type_name;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_payload = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "payload tobject cross-seal") != NULL;
            if (!rejected_payload) {
                fprintf(stderr,
                    "[mir-intent-execution-test] payload drift diagnostic: %s\n",
                    mir_error != NULL ? mir_error : "(none)");
            }
            free(mir_error);
            mir_error = NULL;
            root->success.payload_type_name = saved_payload;

            root->success.payload_decl_syntax_id =
                root->failure.payload_decl_syntax_id;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_payload_identity = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "payload tobject cross-seal") != NULL;
            if (!rejected_payload_identity) {
                fprintf(stderr,
                    "[mir-intent-execution-test] payload identity diagnostic: %s\n",
                    mir_error != NULL ? mir_error : "(none)");
            }
            free(mir_error);
            mir_error = NULL;
            root->success.payload_decl_syntax_id = saved_payload_identity;

            root->action_syntax_id = UINT32_MAX;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_action_identity = !mir_validate(mir, &mir_error);
            if (!rejected_action_identity) {
                fprintf(stderr,
                    "[mir-intent-execution-test] action identity drift was accepted\n");
            }
            free(mir_error);
            mir_error = NULL;
            root->action_syntax_id = saved_action_identity;

            child->predecessor_transition_id = UINT32_MAX;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_predecessor = !mir_validate(mir, &mir_error);
            if (!rejected_predecessor) {
                fprintf(stderr,
                    "[mir-intent-execution-test] predecessor drift was accepted\n");
            }
            free(mir_error);
            mir_error = NULL;
            child->predecessor_transition_id = saved_predecessor;

            child->completion_instruction_id = SIZE_MAX;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_completion = !mir_validate(mir, &mir_error);
            if (!rejected_completion) {
                fprintf(stderr,
                    "[mir-intent-execution-test] completion drift was accepted\n");
            }
            free(mir_error);
            mir_error = NULL;
            child->completion_instruction_id = saved_completion;

            terminal->result_variant_index =
                (saved_terminal_variant + 1) % 3;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
            rejected_terminal_variant = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "payload tobject cross-seal") != NULL;
            if (!rejected_terminal_variant) {
                fprintf(stderr,
                    "[mir-intent-execution-test] terminal variant diagnostic: %s\n",
                    mir_error != NULL ? mir_error : "(none)");
            }
            free(mir_error);
            mir_error = NULL;
            terminal->result_variant_index = saved_terminal_variant;
            routine->intent_execution_plan_digest =
                mir_intent_execution_routine_digest(routine);
        }
        final_valid = mir_validate(mir, &mir_error);
        if (!final_valid && routine != NULL) {
            fprintf(stderr,
                "[mir-intent-execution-test] final diagnostic: %s\n",
                mir_error != NULL ? mir_error : "(none)");
            for (size_t i = 0;
                 i < routine->intent_step_transition_count; i++) {
                const MIRIntentStepTransitionFact *step =
                    &routine->intent_step_transitions[i];
                fprintf(stderr,
                    "[mir-intent-execution-test] step[%llu] zone=%s syntax=%u\n",
                    (unsigned long long)i,
                    step->where_zone_name != NULL
                        ? step->where_zone_name : "(none)",
                    step->where_zone_syntax_id);
            }
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                const MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->ast_type != AST_ZONE_DECL)
                    continue;
                fprintf(stderr,
                    "[mir-intent-execution-test] zone-header[%llu] name=%s syntax=%u\n",
                    (unsigned long long)i,
                    header->name != NULL ? header->name : "(none)",
                    header->source_syntax_id);
            }
        }
        if (!(ok && routine != NULL && exact_shape
              && rejected_variant && rejected_payload
              && rejected_payload_identity
              && rejected_action_identity
              && rejected_predecessor && rejected_completion
              && rejected_terminal_variant && final_valid)) {
            fprintf(stderr,
                "[mir-intent-execution-test] flags ok=%d routine=%d shape=%d "
                "variant=%d payload=%d payload-id=%d action=%d predecessor=%d "
                "completion=%d terminal=%d final=%d\n",
                ok, routine != NULL, exact_shape, rejected_variant,
                rejected_payload, rejected_payload_identity,
                rejected_action_identity, rejected_predecessor,
                rejected_completion, rejected_terminal_variant, final_valid);
        }
        EXPECT(ok && routine != NULL && exact_shape
               && rejected_variant && rejected_payload
               && rejected_payload_identity
               && rejected_action_identity
               && rejected_predecessor && rejected_completion
               && rejected_terminal_variant
               && final_valid);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
