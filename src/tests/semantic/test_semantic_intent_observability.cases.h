static void
test_intent_observability_semantics(void)
{
    printf("\n[intent_observability]\n");

    TEST("intent observability builtins accept structured handle and step queries");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let current: Int = IntentCurrentHandle();\n"
            "    let recent: Int = IntentRecentHandle(0);\n"
            "    let trace: Int = IntentRecentTraceId(0);\n"
            "    let steps: Int = IntentActiveStepCount(current);\n"
            "    let name: String = IntentActiveStepName(recent, 0);\n"
            "    let zone: String = IntentActiveStepZone(recent, 0);\n"
            "    let phase: String = IntentActiveStepPhase(recent, 0);\n"
            "    let participant: String = IntentActiveStepParticipant(recent, 0);\n"
            "    let slot: String = IntentActiveStepSlot(recent, 0);\n"
            "    let from_zone: String = IntentActiveStepFromZone(recent, 0);\n"
            "    let from_slot: String = IntentActiveStepFromSlot(recent, 0);\n"
            "    let to_zone: String = IntentActiveStepToZone(recent, 0);\n"
            "    let to_slot: String = IntentActiveStepToSlot(recent, 0);\n"
            "    let ok: Bool = IntentActiveStepOk(recent, 0);\n"
            "    let failure: String = IntentActiveStepFailure(recent, 0);\n"
            "    Log(ToString(current + recent + trace + steps));\n"
            "    Log(name + zone + phase + participant + slot + from_zone + from_slot + to_zone + to_slot + failure);\n"
            "    Log(ok);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
