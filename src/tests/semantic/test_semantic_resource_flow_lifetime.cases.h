#include "semantic/type_checker_flow_resources.h"
#include "semantic/type_checker_flow_universe.h"

static void
test_resource_flow_symbol_lifetime(void)
{
    TEST("resource-flow facts outlive inner-block Symbol storage");
    {
        const char *source =
            "func Track() -> Void {\n"
            "    if true {\n"
            "        let slot: Slot<Int> = ClaimSlot<Int>();\n"
            "        if true {\n"
            "            Write(slot, 1);\n"
            "        }\n"
            "        Release(slot);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = NULL;
        bool found_slot = false;

        if (program != NULL && ast_assign_stable_ids(program))
            result = semantic_analyze(program);
        if (result != NULL) {
            for (size_t i = 0; i < result->resource_flow_fact_count; i++) {
                const PgyResourceFlowFact *fact =
                    &result->resource_flow_facts[i];
                if (fact->name != NULL && strcmp(fact->name, "slot") == 0) {
                    found_slot = !fact->is_parameter
                        && fact->declaration_syntax_id != 0;
                    break;
                }
            }
        }

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(found_slot);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

static void
test_resource_flow_value_admission(void)
{
    TEST("resource-flow admits values, not nominal or generic type declarations");
    Type generic = {0};
    generic.kind = TYPE_KIND_GENERIC;
    generic.name = "T";
    Symbol nominal = {0}, formal = {0}, value = {0};
    nominal.name = "Nominal";
    nominal.kind = SYMBOL_CLASS;
    nominal.type = &generic;
    nominal.decl_syntax_id = 101;
    formal.name = "T";
    formal.kind = SYMBOL_TYPE_PARAM;
    formal.type = &generic;
    formal.decl_syntax_id = 102;
    value.name = "item";
    value.kind = SYMBOL_VARIABLE;
    value.type = &generic;
    value.decl_syntax_id = 103;
    value.is_parameter = true;
    Symbol *symbols[] = {&nominal, &formal, &value};
    Scope scope = {0};
    scope.symbols = symbols;
    scope.symbol_count = 3;
    scope.kind = SCOPE_FUNCTION;
    ASTNode function = {0};
    function.type = AST_FUNC_DECL;
    SemanticContext ctx = {0};
    ctx.scope = &scope;
    ctx.current_function_decl = &function;
    resource_flow_universe_begin(&ctx);

    EXPECT(!resource_flow_universe_is_value_binding(NULL));
    for (int kind = SYMBOL_VARIABLE; kind <= SYMBOL_ENUM_CONSTRUCTOR; kind++) {
        Symbol kind_probe = {0};
        kind_probe.kind = (SymbolKind)kind;
        EXPECT(resource_flow_universe_is_value_binding(&kind_probe)
            == (kind != SYMBOL_CLASS && kind != SYMBOL_TYPE_PARAM));
    }
    EXPECT(resource_flow_universe_bind(&ctx, &nominal) == RESOURCE_FLOW_INDEX_NONE);
    EXPECT(resource_flow_universe_bind(&ctx, &formal) == RESOURCE_FLOW_INDEX_NONE);
    EXPECT(resource_flow_universe_bind(&ctx, &value) == 0);
    ResourceConsumeSnapshot snapshot = snapshot_resource_states(&ctx);
    EXPECT(snapshot.valid && snapshot.count == 1);
    EXPECT(snapshot.valid && snapshot.count == 1
        && snapshot.symbols[0] == &value && snapshot.symbol_indices[0] == 0);
    destroy_resource_snapshot(&snapshot);
    EXPECT(resource_flow_universe_capture_function_facts(&ctx, 99));
    EXPECT(ctx.resource_flow_fact_count == 1);
    if (ctx.resource_flow_fact_count == 1) {
        const PgyResourceFlowFact *fact = &ctx.resource_flow_facts[0];
        EXPECT(fact->symbol_kind == SYMBOL_VARIABLE);
        EXPECT(fact->declaration_syntax_id == 103);
        EXPECT(fact->is_parameter && fact->parameter_index == 0);
    }
    resource_flow_universe_end(&ctx);
    snapshot = snapshot_resource_states(&ctx);
    EXPECT(!snapshot.valid); /* A value still requires its identity owner. */
    destroy_resource_snapshot(&snapshot);
    pgy_resource_flow_facts_destroy(ctx.resource_flow_facts, ctx.resource_flow_fact_count);

    /* Nested declarations and seal-only capture cannot depend on a prior
     * branch snapshot having excluded type aliases. Future values survive. */
    Type constructor = {0}, future = {0};
    constructor.name = "Future";
    constructor.kind = TYPE_KIND_CLASS;
    future.kind = TYPE_KIND_CONSTRUCTED;
    future.data.constructed.constructor = &constructor;
    nominal.type = formal.type = value.type = &future;
    for (int nested = 0; nested < 2; nested++) {
        memset(&ctx, 0, sizeof(ctx));
        ctx.scope = &scope;
        ctx.current_function_decl = &function;
        resource_flow_universe_begin(&ctx);
        if (nested) {
            EXPECT(resource_flow_universe_record_declaration(&ctx, &nominal));
            EXPECT(resource_flow_universe_record_declaration(&ctx, &formal));
            EXPECT(resource_flow_universe_record_declaration(&ctx, &value));
        }
        EXPECT(resource_flow_universe_capture_function_facts(&ctx, 100));
        EXPECT(ctx.resource_flow_fact_count == 1);
        if (ctx.resource_flow_fact_count == 1) {
            EXPECT(ctx.resource_flow_facts[0].symbol_kind == SYMBOL_VARIABLE);
            EXPECT(ctx.resource_flow_facts[0].stable_index == 0);
            EXPECT(ctx.resource_flow_facts[0].function_syntax_id == 100);
        }
        resource_flow_universe_end(&ctx);
        pgy_resource_flow_facts_destroy(ctx.resource_flow_facts, ctx.resource_flow_fact_count);
    }
}

static void
test_resource_flow_generation_reentry(void)
{
    TEST("resource-flow validates retained epoch/index hints against current declaration");
    /* The memo is retained deliberately: ordinary re-entry, a new context,
     * and epoch wrap must all resolve the same current declaration owner. */
    for (int reuse = 0; reuse < 3; reuse++) {
        Type generic = {0};
        generic.kind = TYPE_KIND_GENERIC;
        generic.name = "T";
        Symbol retained = {0}, first = {0};
        retained.name = "retained_value";
        retained.kind = SYMBOL_VARIABLE;
        retained.type = &generic;
        retained.decl_syntax_id = 201;
        first.name = "new_generation_first";
        first.kind = SYMBOL_VARIABLE;
        first.type = &generic;
        first.decl_syntax_id = 202;
        Symbol *symbols[] = {&retained, &first};
        Scope scope = {0};
        scope.symbols = symbols;
        scope.symbol_count = 2;
        scope.kind = SCOPE_FUNCTION;
        ASTNode function = {0};
        function.type = AST_FUNC_DECL;
        SemanticContext initial = {0}, fresh = {0};
        initial.scope = fresh.scope = &scope;
        initial.current_function_decl = fresh.current_function_decl = &function;
        resource_flow_universe_begin(&initial);
        EXPECT(resource_flow_universe_bind(&initial, &retained) == 0);
        resource_flow_universe_end(&initial);
        SemanticContext *ctx = reuse == 1 ? &fresh : &initial;
        if (reuse == 2)
            ctx->resource_flow_epoch = SIZE_MAX;
        resource_flow_universe_begin(ctx);
        EXPECT(resource_flow_universe_bind(ctx, &first) == 0);
        EXPECT(resource_flow_universe_bind(ctx, &retained) == 1);
        EXPECT(resource_flow_universe_symbol(ctx, 1) == &retained);
        ResourceConsumeSnapshot snapshot = snapshot_resource_states(ctx);
        EXPECT(snapshot.valid && snapshot.count == 2);
        if (snapshot.valid && snapshot.count == 2) {
            EXPECT(snapshot.symbols[0] == &retained && snapshot.symbol_indices[0] == 1);
            EXPECT(snapshot.symbols[1] == &first && snapshot.symbol_indices[1] == 0);
        }
        destroy_resource_snapshot(&snapshot);
        EXPECT(resource_flow_universe_capture_function_facts(ctx, 301));
        EXPECT(ctx->resource_flow_fact_count == 2);
        if (ctx->resource_flow_fact_count == 2) {
            EXPECT(ctx->resource_flow_facts[0].stable_index == 0
                && ctx->resource_flow_facts[0].declaration_syntax_id == 202);
            EXPECT(ctx->resource_flow_facts[1].stable_index == 1
                && ctx->resource_flow_facts[1].declaration_syntax_id == 201);
        }
        resource_flow_universe_end(ctx);
        pgy_resource_flow_facts_destroy(ctx->resource_flow_facts, ctx->resource_flow_fact_count);
    }
}

static void
test_resource_flow_source_admission(void)
{
    TEST("resource-flow source keeps typed/generic parameters and nested Slot only");
    const char *source =
        "struct UnusedRows { let items: Array<Int>; }\n"
        "subject UnusedSubject { let name: String; }\n"
        "func Leaf() -> Int {\n"
        "    let n: Int = 0; while n < 2 { n = n + 1; } return n;\n"
        "}\n"
        "func Inspect(ref row: UnusedRows) -> Int {\n"
        "    if true { Log(ArrayLength(row.items)); }\n"
        "    return ArrayLength(row.items);\n"
        "}\n"
        "func Generic<T>(item: T) -> Int {\n"
        "    if true { let n: Int = 0; } return 1;\n"
        "}\n"
        "func Nested() -> Void {\n"
        "    if true { let cell: Slot<Int> = ClaimSlot<Int>();\n"
        "        if true { Write(cell, 1); } Release(cell); }\n"
        "}\n"
        "func Main() -> Void { Log(Leaf()); Nested(); }\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *result = NULL;
    if (program != NULL && ast_assign_stable_ids(program))
        result = semantic_analyze(program);
    EXPECT(!parser_has_error(parser));
    EXPECT(result != NULL && result->error_count == 0);
    EXPECT(result != NULL && result->resource_flow_fact_count == 3);
    bool found_row = false, found_item = false, found_cell = false;
    if (result != NULL) {
        for (size_t i = 0; i < result->resource_flow_fact_count; i++) {
            const PgyResourceFlowFact *fact = &result->resource_flow_facts[i];
            EXPECT(fact->symbol_kind != SYMBOL_CLASS && fact->symbol_kind != SYMBOL_TYPE_PARAM);
            EXPECT(fact->stable_index == 0 && fact->declaration_syntax_id != 0);
            if (fact->name != NULL && strcmp(fact->name, "row") == 0)
                found_row = fact->is_parameter && fact->parameter_index == 0;
            if (fact->name != NULL && strcmp(fact->name, "item") == 0)
                found_item = fact->is_parameter && fact->parameter_index == 0;
            if (fact->name != NULL && strcmp(fact->name, "cell") == 0)
                found_cell = !fact->is_parameter && fact->symbol_kind == SYMBOL_SLOT;
        }
    }
    EXPECT(found_row && found_item && found_cell);
    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
}
