/* Intent compensation coverage (docs/173 INT-2): an intent that claims a full
 * rollback (it writes `rollback: full` or compensates some step) must give
 * every effectful step that a later failure can reach a `compensate:` or an
 * `irreversible: "reason";` marker. The marker contradicts a compensation and
 * means nothing on a step without an effect. */
#define COVERAGE_PRELUDE \
    "subject Wallet {\n" \
    "    let mut gold: Int;\n" \
    "    action Pay(self) -> Void { self.gold = self.gold - 1; }\n" \
    "    action Refund(self) -> Void { self.gold = self.gold + 1; }\n" \
    "    action Ship(self) -> Void { self.gold = self.gold; }\n" \
    "}\n" \
    "zone Shop {\n" \
    "    subject slot wallet: Wallet\n" \
    "}\n"
#define COVERAGE_MAIN \
    "func Main() -> Void { }\n"

static void
test_intent_compensation_coverage(void)
{
    static const struct {
        const char *name;
        const char *source;
        bool parse_error;
        const char *diagnostic;     /* NULL: accepted */
    } cases[] = {
        {"declared full rollback refuses an uncompensated effect before a later step",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    rollback: full;\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); }\n"
         "    step ship { where: Shop; who: wallet; on: wallet.Ship(); }\n"
         "}\n" COVERAGE_MAIN,
         false, "Intent step 'pay' in 'Buy' has an effect but no compensation"},
        {"a compensation elsewhere claims full rollback under the default policy",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    step ship { where: Shop; who: wallet; on: wallet.Ship(); }\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay();\n"
         "               compensate: wallet.Refund(); post: wallet.gold >= 0; }\n"
         "}\n" COVERAGE_MAIN,
         false, "Intent step 'ship' in 'Buy' has an effect but no compensation"},
        {"a post-action check of the step itself reaches its effect",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    rollback: full;\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); guard: wallet.gold > 0; }\n"
         "}\n" COVERAGE_MAIN,
         false, "a guard/expect/post/invariant check of this step can fail"},
        {"irreversible with a reason satisfies coverage",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    rollback: full;\n"
         "    step ship { where: Shop; who: wallet; on: wallet.Ship();\n"
         "                irreversible: \"a shipped parcel cannot be recalled\"; }\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); }\n"
         "}\n" COVERAGE_MAIN,
         false, NULL},
        {"a last step whose effect no failure can follow needs no marker",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    rollback: full;\n"
         "    step pay { where: Shop; who: wallet; pre: wallet.gold > 0; on: wallet.Pay(); post: true; }\n"
         "}\n" COVERAGE_MAIN,
         false, NULL},
        {"an intent that compensates nothing under the default policy claims no rollback",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); }\n"
         "    step ship { where: Shop; who: wallet; on: wallet.Ship(); }\n"
         "}\n" COVERAGE_MAIN,
         false, NULL},
        {"rollback current carries no full-coverage obligation",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    rollback: current;\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); compensate: wallet.Refund(); }\n"
         "    step ship { where: Shop; who: wallet; on: wallet.Ship(); }\n"
         "}\n" COVERAGE_MAIN,
         false, NULL},
        {"irreversible contradicts a compensation on the same step",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); compensate: wallet.Refund();\n"
         "               irreversible: \"paid\"; }\n"
         "}\n" COVERAGE_MAIN,
         false, "declares both 'compensate:' and 'irreversible:'"},
        {"irreversible on a step without an effect is refused",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    step check { where: Shop; who: wallet; pre: wallet.gold > 0; irreversible: \"nothing\"; }\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); }\n"
         "}\n" COVERAGE_MAIN,
         false, "is marked 'irreversible:' but performs no effect"},
        {"irreversible needs a non-empty reason string",
         COVERAGE_PRELUDE
         "intent Buy(wallet: Wallet) {\n"
         "    step pay { where: Shop; who: wallet; on: wallet.Pay(); irreversible: \"\"; }\n"
         "}\n" COVERAGE_MAIN,
         true, NULL},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST(cases[i].name);
        Lexer *lexer = lexer_create(cases[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);

        if (cases[i].parse_error) {
            EXPECT(parser_has_error(parser));
        } else {
            SemanticResult *result = semantic_analyze(program);
            bool accepted = result != NULL && result->error_count == 0;

            EXPECT(!parser_has_error(parser));
            if (cases[i].diagnostic == NULL) {
                EXPECT(accepted);
            } else {
                EXPECT(!accepted
                    && ctx_has_diagnostic_substring_from_result(result,
                        cases[i].diagnostic));
            }
            semantic_result_destroy(result);
        }
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

#undef COVERAGE_PRELUDE
#undef COVERAGE_MAIN
