/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Transpiler unit test suite
 * Build: make test-transpile
 * Run:   ./bin/test_transpile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/string_compat.h"
#include "codegen/transpiler.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/type_system.h"
#include "semantic/type_checker.h"

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

#define EXPECT_STR_CONTAINS(haystack, needle) \
    EXPECT(strstr((haystack), (needle)) != NULL)

/* -----------------------------------------------------------------
 * Minimal AST node builders (same helpers as test_semantic.c)
 * ----------------------------------------------------------------- */

static ASTNode *
make_number(double v, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type  = AST_NUMBER;
    n->line  = line;
    n->data.number.value = v;
    return n;
}

static ASTNode *
make_string_lit(const char *s, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_STRING;
    n->line = line;
    n->data.string.value = pergyra_strdup(s);
    return n;
}

static ASTNode *
make_identifier(const char *name, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_IDENTIFIER;
    n->line = line;
    n->data.identifier.name = pergyra_strdup(name);
    return n;
}

static ASTNode *
make_call(const char *callee_name, ASTNode **args, size_t arg_count,
           uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_CALL;
    n->line = line;
    n->data.call.callee    = make_identifier(callee_name, line);
    if (arg_count > 0) {
        n->data.call.arguments = calloc(arg_count, sizeof(ASTNode *));
        memcpy(n->data.call.arguments, args, arg_count * sizeof(ASTNode *));
    }
    n->data.call.arg_count = arg_count;
    return n;
}

static ASTNode *
make_type_node(const char *type_name)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_TYPE;
    n->data.type.name = pergyra_strdup(type_name);
    return n;
}

static ASTNode *
make_let(const char *name, ASTNode *ann, ASTNode *init, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_LET_DECL;
    n->line = line;
    n->data.let_decl.name        = pergyra_strdup(name);
    n->data.let_decl.type        = ann;
    n->data.let_decl.initializer = init;
    return n;
}

static ASTNode *
make_return(ASTNode *value, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_RETURN;
    n->line = line;
    n->data.return_stmt.value = value;
    return n;
}

static ASTNode *
make_block(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_BLOCK;
    if (count > 0) {
        n->data.block.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.block.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.block.count      = count;
    return n;
}

static ASTNode *
make_program(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_PROGRAM;
    if (count > 0) {
        n->data.program.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.program.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.program.count      = count;
    return n;
}

static HIRProgram *
lower_program(ASTNode *program)
{
    char *error = NULL;
    HIRProgram *hir = hir_lower(program, &error);
    if (hir == NULL) {
        fprintf(stderr, "HIR lowering failed in test: %s\n",
                error != NULL ? error : "out of memory");
    }
    free(error);
    return hir;
}

static ASTNode *
make_generic_type(const char *name, const char *arg_name)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_name);
    gp->constraint = ast_create_type(arg_name);
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_generic_type_from_node(const char *name, ASTNode *arg_type)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_type->data.type.name);
    gp->constraint = arg_type;
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_match_case(ASTNode *pattern, ASTNode *body)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_CASE;
    n->data.match_case.pattern = pattern;
    n->data.match_case.body = body;
    return n;
}

static ASTNode *
make_match(ASTNode *subject, ASTNode **cases, size_t case_count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_STMT;
    n->data.match_stmt.subject = subject;
    if (case_count > 0) {
        n->data.match_stmt.cases = calloc(case_count, sizeof(ASTNode *));
        memcpy(n->data.match_stmt.cases, cases, case_count * sizeof(ASTNode *));
    }
    n->data.match_stmt.case_count = case_count;
    return n;
}

static GenericParams *
make_generic_params1(const char *name)
{
    GenericParams *params = calloc(1, sizeof(GenericParams));
    GenericParam *param = calloc(1, sizeof(GenericParam));

    params->count = 1;
    params->params = calloc(1, sizeof(GenericParam *));
    param->name = pergyra_strdup(name);
    params->params[0] = param;
    return params;
}

/* -----------------------------------------------------------------
 * Helper: emit a single statement into a fresh context, return
 * the output string (caller does NOT free — points into ctx->out).
 * ----------------------------------------------------------------- */

static const char *
emit_stmt_to_str(ASTNode *node, TranspilerCtx **out_ctx)
{
    TranspilerCtx *ctx = transpiler_ctx_create();
    emit_statement(node, ctx);
    *out_ctx = ctx;
    return ctx->out->data;
}

/* -----------------------------------------------------------------
 * Tests: CodeBuf
 * ----------------------------------------------------------------- */

static void
test_codebuf(void)
{
    printf("\n[codebuf]\n");

    TEST("write simple string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "hello");
        EXPECT(strcmp(b->data, "hello") == 0 && b->len == 5);
        codebuf_destroy(b);
    }

    TEST("write formatted string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "int x = %d;", 42);
        EXPECT(strcmp(b->data, "int x = 42;") == 0);
        codebuf_destroy(b);
    }

    TEST("write triggers growth beyond initial capacity");
    {
        CodeBuf *b = codebuf_create();
        for (int i = 0; i < 1000; i++)
            codebuf_write(b, "a");
        EXPECT(b->len == 1000);
        codebuf_destroy(b);
    }

    TEST("multiple writes concatenate correctly");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "foo");
        codebuf_write(b, "bar");
        codebuf_write(b, "baz");
        EXPECT(strcmp(b->data, "foobarbaz") == 0);
        codebuf_destroy(b);
    }
}

/* -----------------------------------------------------------------
 * Tests: type mapping
 * ----------------------------------------------------------------- */

static void
test_type_mapping(void)
{
    printf("\n[type_mapping]\n");

    TEST("Int → int32_t");
    EXPECT(strcmp(pergyra_type_to_c("Int"), "int32_t") == 0);

    TEST("Long → int64_t");
    EXPECT(strcmp(pergyra_type_to_c("Long"), "int64_t") == 0);

    TEST("Float → float");
    EXPECT(strcmp(pergyra_type_to_c("Float"), "float") == 0);

    TEST("Bool → bool");
    EXPECT(strcmp(pergyra_type_to_c("Bool"), "bool") == 0);

    TEST("String → char*");
    EXPECT(strcmp(pergyra_type_to_c("String"), "char*") == 0);

    TEST("Void → void");
    EXPECT(strcmp(pergyra_type_to_c("Void"), "void") == 0);

    TEST("Slot<Int> → PgySlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Int>"), "PgySlot_Int") == 0);

    TEST("Slot<String> → PgySlot_String");
    EXPECT(strcmp(pergyra_type_to_c("Slot<String>"), "PgySlot_String") == 0);

    TEST("Slot<Vec2> → PgySlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Vec2>"), "PgySlot_Vec2") == 0);

    TEST("SecureSlot<Int> → PgySecureSlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Int>"), "PgySecureSlot_Int") == 0);

    TEST("SecureSlot<Vec2> → PgySecureSlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Vec2>"), "PgySecureSlot_Vec2") == 0);

    TEST("Array<Vertex> → PgyArray_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Array<Vertex>"), "PgyArray_Vertex") == 0);

    TEST("Slice<Vertex> → PgySlice_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Slice<Vertex>"), "PgySlice_Vertex") == 0);

    TEST("Rc<Int> → PgyRc_Int");
    EXPECT(strcmp(pergyra_type_to_c("Rc<Int>"), "PgyRc_Int") == 0);

    TEST("Weak<Int> → PgyWeak_Int");
    EXPECT(strcmp(pergyra_type_to_c("Weak<Int>"), "PgyWeak_Int") == 0);

    TEST("Allocator → PgyAllocator");
    EXPECT(strcmp(pergyra_type_to_c("Allocator"), "PgyAllocator") == 0);

    TEST("Box<Array<Int>> → PgyBoxArray_Int");
    EXPECT(strcmp(pergyra_type_to_c("Box<Array<Int>>"), "PgyBoxArray_Int") == 0);

    TEST("slot_inner_type_name(Slot<Float>) → Float");
    EXPECT(strcmp(slot_inner_type_name("Slot<Float>"), "Float") == 0);

    TEST("slot_inner_type_name(SecureSlot<Long>) → Long");
    EXPECT(strcmp(slot_inner_type_name("SecureSlot<Long>"), "Long") == 0);
}

/* -----------------------------------------------------------------
 * Tests: expression emitters
 * ----------------------------------------------------------------- */

static void
test_expression_emit(void)
{
    printf("\n[expression_emit]\n");

    TranspilerCtx *ctx;
    char *result;

    TEST("integer literal → correct C literal");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_number(42, 1), ctx);
        EXPECT(strcmp(result, "42") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("string literal → quoted C string");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_string_lit("hello", 1), ctx);
        EXPECT(strcmp(result, "\"hello\"") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(42) → Some_Int(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Some_Int(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("None() → None_Int()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("None", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "None_Int()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("identifier → same name");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_identifier("myVar", 1), ctx);
        EXPECT(strcmp(result, "myVar") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("user function call → funcName(arg0, arg1)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[2] = { make_number(1, 1), make_number(2, 1) };
        result = emit_expression(make_call("Add", args, 2, 1), ctx);
        EXPECT(strcmp(result, "Add(1, 2)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(42) → pgy_log(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Read(s) → pgy_read_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Read", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_read_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Release(s) → pgy_release_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Release", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_release_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewRead(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewRead", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewWrite(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewWrite", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Move(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("Move", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("array access → values[0]");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(
            ast_create_array_access(make_identifier("values", 1),
                                    make_number(0, 1)),
            ctx);
        EXPECT(strcmp(result, "values[0]") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RcClone(shared) → pgy_rc_clone_Int(shared)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("shared",
                     make_generic_type("Rc", "Int"),
                     make_call("RcNew", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("shared", 1) };
        result = emit_expression(make_call("RcClone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_rc_clone_Int(shared)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxGet(boxed) → pgy_box_get_Int(boxed)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("boxed", 1) };
        result = emit_expression(make_call("BoxGet", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_get_Int(boxed)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet(boxed, 42) → pgy_box_set_Int(&boxed, 42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[2] = { make_identifier("boxed", 1), make_number(42, 1) };
        result = emit_expression(make_call("BoxSet", args, 2, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_set_Int(&boxed, 42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ToDto(PlayerDto, player) → dto projection literal");
    {
        const char *source =
            "dto PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToDto(PlayerDto, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "PlayerDto snapshot = (PlayerDto){ .hp = player.hp, .name = player.name };");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject(PlayerView, player) → object projection literal");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "PlayerView view = (PlayerView){ .hp = player.hp, .name = player.name };");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested vessel-backed projection lowers through subject field paths");
    {
        const char *source =
            "vessel Cycle { age: Int; fatigue: Int; }\n"
            "vessel Traits { metabolism: Int; }\n"
            "subject Creature { vessel cycle: Cycle; vessel traits: Traits; }\n"
            "object CreatureView { age: Int; fatigue: Int; metabolism: Int; }\n"
            "dto CreaturePacket { age: Int; metabolism: Int; }\n"
            "func Main() -> Void {\n"
            "    let creature: Creature = Creature();\n"
            "    let view: CreatureView = ToObject(CreatureView, creature);\n"
            "    let packet: CreaturePacket = ToDto(CreaturePacket, creature);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "CreatureView view = (CreatureView){ .age = creature.cycle.age, .fatigue = creature.cycle.fatigue, .metabolism = creature.traits.metabolism };");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "CreaturePacket packet = (CreaturePacket){ .age = creature.cycle.age, .metabolism = creature.traits.metabolism };");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("AllocatorTracing() → pgy_allocator_tracing()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("AllocatorTracing", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "pgy_allocator_tracing()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(poisoned) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poisoned", 1) };
        result = emit_expression(make_call("HasState", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false /* HasState: zone-semantic query only */") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(allied, player, enemy) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[3] = {
            make_identifier("allied", 1),
            make_identifier("player", 1),
            make_identifier("enemy", 1)
        };
        result = emit_expression(make_call("HasState", args, 3, 1), ctx);
        EXPECT(strcmp(result, "false /* HasState: zone-semantic query only */") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasZone(battle) → world semantic placeholder outside world context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("battle", 1) };
        result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false /* HasZone: world-semantic query only */") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasLayer(poison) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poison", 1) };
        result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false /* HasLayer: zone-semantic query only */") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection(snapshot) → domain semantic placeholder outside domain context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("snapshot", 1) };
        result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false /* HasProjection: domain-semantic query only */") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection lowers to relation/effect/zone runtime projection flag inside domain context");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "dto PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    bind playerView from left\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    dto slot snapshot: PlayerDto\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    dto slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);

        ctx = transpiler_ctx_create();
        ctx->hir = hir;

        ctx->current_relation_name = "TrustedLink";
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }
        ctx->current_relation_name = NULL;

        ctx->current_effect_name = "Poisoned";
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        ctx->current_effect_name = NULL;

        ctx->current_zone_name = "BattleZone";
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("LLVM domain layouts include projection-ready flags for relation/effect/zone");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "dto PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    dto slot snapshot: PlayerDto\n"
            "    bind playerView from left\n"
            "    bind snapshot from right\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    dto slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    dto slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);

        ctx = transpiler_ctx_create();
        ctx->hir = hir;
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_snapshot;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasLayer lowers to zone runtime layer field inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);

        ctx = transpiler_ctx_create();
        ctx->hir = hir;
        ctx->current_zone_name = "BattleZone";

        {
            ASTNode *args[1] = { make_identifier("poison", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__layer_active_poison") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("trust", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__layer_active_trust") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState lowers to zone runtime state field inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);

        ctx = transpiler_ctx_create();
        ctx->hir = hir;
        ctx->current_zone_name = "BattleZone";

        {
            ASTNode *args[1] = { make_identifier("poisoned", 1) };
            result = emit_expression(make_call("HasState", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__state_poisoned") == 0);
            free(result);
        }

        {
            ASTNode *args[3] = {
                make_identifier("allied", 1),
                make_identifier("player", 1),
                make_identifier("enemy", 1)
            };
            result = emit_expression(make_call("HasState", args, 3, 1), ctx);
            EXPECT(strcmp(result, "self->__state_allied") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZone lowers to world runtime zone fields inside world context");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);

        ctx = transpiler_ctx_create();
        ctx->hir = hir;
        ctx->current_world_name = "GameWorld";

        {
            ASTNode *args[1] = { make_identifier("liveBattle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_state_liveBattle") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("battle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_active_battle") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Tests: statement emitters
 * ----------------------------------------------------------------- */

static void
test_statement_emit(void)
{
    printf("\n[statement_emit]\n");

    TranspilerCtx *ctx;

    TEST("let x: Int = 42 → int32_t x = 42;");
    {
        ASTNode *node = make_let("x", make_type_node("Int"), make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "int32_t x = 42;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let s: String = \"hi\" → char* s = \"hi\";");
    {
        ASTNode *node = make_let("s", make_type_node("String"),
                                  make_string_lit("hi", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "char* s = \"hi\";");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let maybe: Option<Int> = Some(42) → PgyOption_Int maybe = Some_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("maybe", make_generic_type("Option", "Int"),
                                 make_call("Some", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int maybe = Some_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let none: Option<Int> = None() → PgyOption_Int none = None_Int();");
    {
        ASTNode *node = make_let("none", make_generic_type("Option", "Int"),
                                 make_call("None", NULL, 0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int none = None_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("match Option<Int> destructures Some/None");
    {
        ctx = transpiler_ctx_create();

        ASTNode *some_args[1] = { make_number(42, 1) };
        emit_statement(
            make_let("maybe", make_generic_type("Option", "Int"),
                     make_call("Some", some_args, 1, 1), 1),
            ctx);

        ASTNode *bind_args[1] = { make_identifier("v", 2) };
        ASTNode *case_some_body_stmts[1] = {
            make_return(make_identifier("v", 2), 2)
        };
        ASTNode *case_none_body_stmts[1] = {
            make_return(make_number(0, 3), 3)
        };
        ASTNode *cases[2] = {
            make_match_case(
                make_call("Some", bind_args, 1, 2),
                make_block(case_some_body_stmts, 1)),
            make_match_case(
                make_call("None", NULL, 0, 3),
                make_block(case_none_body_stmts, 1))
        };

        emit_statement(make_match(make_identifier("maybe", 2), cases, 2), ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionSome");
        EXPECT_STR_CONTAINS(ctx->out->data, "__typeof__(__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, "v = __match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".value;");
        EXPECT_STR_CONTAINS(ctx->out->data, "else if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionNone");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let slot: Slot<Int> = ClaimSlot<Int>() → PgySlot_Int slot = pgy_claim_Int();");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSlot", args, 0, 1);
        ASTNode *node = make_let("slot", make_type_node("Slot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int slot = pgy_claim_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let ss: SecureSlot<Int> = ClaimSecureSlot() → PgySecureSlot_Int + token");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSecureSlot", args, 0, 1);
        ASTNode *node = make_let("ss", make_type_node("SecureSlot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyToken_Int ss_token;");
        EXPECT_STR_CONTAINS(out, "PgySecureSlot_Int ss = pgy_claim_secure_Int(");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let rv: ReadView<Int> = ViewRead(slot) → PgySlot_Int rv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("rv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int rv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let wv: WriteView<Int> = ViewWrite(slot) → PgySlot_Int wv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("wv", make_type_node("WriteView<Int>"),
                                 make_call("ViewWrite", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int wv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let srv: ReadView<Int> = ViewRead(ss) on SecureSlot emits token alias");
    {
        ctx = transpiler_ctx_create();
        ASTNode *claim = make_call("ClaimSecureSlot", NULL, 0, 1);
        ASTNode *secure = make_let("ss", make_type_node("SecureSlot<Int>"), claim, 1);
        emit_statement(secure, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int ss_token;");

        ASTNode *args[1] = { make_identifier("ss", 2) };
        ASTNode *node = make_let("srv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 2), 2);
        emit_statement(node, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "srv = ss;");
        EXPECT_STR_CONTAINS(ctx->out->data, "srv_token = ss_token;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let dst: Slot<Int> = mt materializes moved slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *move_args[1] = { make_identifier("slot", 1) };
        ASTNode *mt = make_let("mt", make_type_node("MoveToken<Int>"),
                               make_call("Move", move_args, 1, 1), 1);
        emit_statement(mt, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int mt = slot;");

        ASTNode *dst = make_let("dst", make_type_node("Slot<Int>"),
                                make_identifier("mt", 2), 2);
        emit_statement(dst, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int dst = mt;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("return 0 → return 0;");
    {
        ASTNode *node = make_return(make_number(0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "return 0;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let vertices: Array<Vertex> = meshData → PgyArray_Vertex vertices = meshData;");
    {
        ASTNode *node = make_let("vertices",
                                 make_generic_type("Array", "Vertex"),
                                 make_identifier("meshData", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Vertex vertices = meshData;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<Int> = RcNew(42) → PgyRc_Int shared = pgy_rc_new_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("shared",
                                 make_generic_type("Rc", "Int"),
                                 make_call("RcNew", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_Int shared = pgy_rc_new_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let alloc: Allocator = AllocatorPool(1024) → PgyAllocator alloc = pgy_allocator_pool(1024);");
    {
        ASTNode *args[1] = { make_number(1024, 1) };
        ASTNode *node = make_let("alloc",
                                 make_type_node("Allocator"),
                                 make_call("AllocatorPool", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyAllocator alloc = pgy_allocator_pool(1024);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let storage: Box<Array<Int>> = BoxArray(128) → fused BoxArray allocation");
    {
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *args[1] = { make_number(128, 1) };
        ASTNode *node = make_let("storage", boxed_array,
                                 make_call("BoxArray", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyBoxArray_Int storage = pgy_box_array_new_Int(128, NULL);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Write(slot, 42) → pgy_write_Int(&slot, 42);");
    {
        ASTNode *args[2] = { make_identifier("slot", 1), make_number(42, 1) };
        ASTNode *call    = make_call("Write", args, 2, 1);
        const char *out  = emit_stmt_to_str(call, &ctx);
        EXPECT_STR_CONTAINS(out, "pgy_write_Int(&slot, 42)");
        transpiler_ctx_destroy(ctx);
    }

    TEST("unsafe block emits body directly");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        const char *out = emit_stmt_to_str(unsafe_block, &ctx);
        EXPECT_STR_CONTAINS(out, "/* unsafe */");
        EXPECT_STR_CONTAINS(out, "pgy_log");
        transpiler_ctx_destroy(ctx);
    }

    TEST("defer statement emits cleanup sentinel");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        const char *out = emit_stmt_to_str(defer_stmt, &ctx);
        EXPECT_STR_CONTAINS(out, "__attribute__((cleanup(_pgy_defer_");
        EXPECT_STR_CONTAINS(ctx->helpers->data, "static void _pgy_defer_");
        transpiler_ctx_destroy(ctx);
    }

    TEST("bind statement emits party-role rebinding call");
    {
        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        const char *out = emit_stmt_to_str(bind, &ctx);
        EXPECT_STR_CONTAINS(out, "UnknownParty_bind_fighter(&team, NULL, &Warrior_fighter_vtable_instance)");
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Tests: full program output
 * ----------------------------------------------------------------- */

static void
test_program_emit(void)
{
    printf("\n[program_emit]\n");

    TEST("program header contains #include \"pgy_runtime.h\"");
    {
        ASTNode *stmts[0];
        ASTNode *prog = make_program(stmts, 0);
        HIRProgram *hir = lower_program(prog);
        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "#include \"pgy_runtime.h\"");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("function emitted at top level with correct signature");
    {
        /* func Add(a: Int, b: Int) -> Int { return a + b } */
        FuncParam pa, pb;
        memset(&pa, 0, sizeof(pa)); pa.name = "a"; pa.type = make_type_node("Int");
        memset(&pb, 0, sizeof(pb)); pb.name = "b"; pb.type = make_type_node("Int");
        FuncParam *params[2] = { &pa, &pb };

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name        = "Add";
        fn->data.func_decl.params      = params;
        fn->data.func_decl.param_count = 2;
        fn->data.func_decl.return_type = make_type_node("Int");
        fn->data.func_decl.body        = NULL;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t\nAdd(int32_t a, int32_t b)");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("generic function call emits concrete specialization in C");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("T");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "Identity";
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("T");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);
        identity->data.func_decl.generic_params = make_generic_params1("T");

        ASTNode *sum = make_let("sum", make_type_node("Int"), make_number(7, 1), 1);
        ASTNode *identity_args[1] = { make_identifier("sum", 1) };
        ASTNode *echoed = make_let("echoed", make_type_node("Int"),
                                   make_call("Identity", identity_args, 1, 1), 1);
        ASTNode *log_args[1] = { make_identifier("echoed", 1) };
        ASTNode *main_stmts[3] = { sum, echoed, make_call("Log", log_args, 1, 1) };

        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = make_block(main_stmts, 3);

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Identity_Int(int32_t x);");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t\nIdentity_Int(int32_t x)");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t echoed = Identity_Int(sum);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("spawn of generic function call uses concrete specialization");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("T");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "Identity";
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("T");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);
        identity->data.func_decl.generic_params = make_generic_params1("T");

        ASTNode *spawn_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Identity", spawn_args, 1, 1);
        ASTNode *spawn = calloc(1, sizeof(ASTNode));
        spawn->type = AST_SPAWN_EXPR;
        spawn->data.spawn_expr.function = call;

        ASTNode *task = make_let("task", NULL, spawn, 1);
        ASTNode *main_stmts[1] = { task };
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = make_block(main_stmts, 1);

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Identity_Int(int32_t x);");
        EXPECT_STR_CONTAINS(ctx->out->data, "*result = Identity_Int(args->arg0);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("parallel block emits pgy_spawn / pgy_await per task");
    {
        ASTNode *tasks[2] = {
            make_call("A", NULL, 0, 1),
            make_call("B", NULL, 0, 1)
        };
        ASTNode *par = calloc(1, sizeof(ASTNode));
        par->type = AST_PARALLEL_BLOCK;
        par->data.parallel.tasks      = tasks;
        par->data.parallel.task_count = 2;

        ASTNode *stmts[1] = { par };
        ASTNode *prog     = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_spawn");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_await");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_par_");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("struct emits typedef struct and method function");
    {
        ASTNode *st = calloc(1, sizeof(ASTNode));
        st->type = AST_CLASS_DECL;
        st->data.class_decl.name = "Vec3";
        st->data.class_decl.is_struct = true;

        ClassField fx, fy, fz;
        memset(&fx, 0, sizeof(fx));
        memset(&fy, 0, sizeof(fy));
        memset(&fz, 0, sizeof(fz));
        fx.name = "x"; fx.type = make_type_node("Float");
        fy.name = "y"; fy.type = make_type_node("Float");
        fz.name = "z"; fz.type = make_type_node("Float");
        ClassField *fields[3] = { &fx, &fy, &fz };
        st->data.class_decl.fields = fields;
        st->data.class_decl.field_count = 3;

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Length";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Float");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        st->data.class_decl.methods = methods;
        st->data.class_decl.method_count = 1;

        ASTNode *stmts[1] = { st };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Vec3");
        EXPECT_STR_CONTAINS(ctx->out->data, "float x;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float y;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float z;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float\nVec3_Length(Vec3 self)");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("subject method call lowers to self-cell call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(&v)");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 *self");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->x");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_BOX_DEFINE(Vec2, Vec2)");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method call lowers by value");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(v)");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 self");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.x;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Box<class> handle lowers through explicit Box helpers");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(BoxGet(handle).x);\n"
            "    BoxSet(handle, Vec2(3, 4));\n"
            "    BoxDrop(handle);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PgyBox_Vec2 MakeVec(");
        EXPECT_STR_CONTAINS(ctx->out->data, "return pgy_box_new_Vec2((Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(pgy_box_get_Vec2(handle).x);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_set_Vec2(&handle, (Vec2){ .x = 3, .y = 4 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_drop_Vec2(&handle);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field access lowers through value self");
    {
        const char *source =
            "class Counter {\n"
            "    let count: Int;\n"
            "    func Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self.count = (self.count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.count;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action bare field access lowers through self cell");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "    action Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->count = (self->count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->count;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone methods lower bare shared fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "    func Next(self) -> Int {\n"
            "        round = round + 1;\n"
            "        return round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Next() + round;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->round = (self->round + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (BattleZone_Next(self) + self->round);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world methods lower bare shared fields zone fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    shared storm: Int = 1\n"
            "    func Pulse(self) -> Int {\n"
            "        storm = storm + 1;\n"
            "        return storm + battle.round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Pulse() + storm;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->storm = (self->storm + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (self->storm + self->battle.round);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (GameWorld_Pulse(self) + self->storm);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("vessel methods lower like passive pointer-self receivers");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT(strstr(ctx->out->data, "HealthState_IsDead(HealthState *self)") != NULL
            || strstr(ctx->helpers->data, "HealthState_IsDead(HealthState *self)") != NULL);
        EXPECT(strstr(ctx->out->data, "return (self->current <= 0);") != NULL
            || strstr(ctx->helpers->data, "return (self->current <= 0);") != NULL);

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor positional arguments lower to field initialization");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, 7);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 v = { .x = 3, .y = 7 };");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> lowers through generated object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2));\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Vec2 s = pgy_claim_Vec2();");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 3, .y = 7 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_release_Vec2(&s);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Vec2 s_token;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySecureSlot_Vec2 s = pgy_claim_secure_Vec2(&s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2(&s, (Vec2){ .x = 3, .y = 7 }, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2(&s, (Vec2){ .x = 1, .y = 2 }, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2(&s, &s_token);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<actor> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "actor Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Bot, Bot)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Bot s_token;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySecureSlot_Bot s = pgy_claim_secure_Bot(&s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Bot(&s, (Bot){ .hp = 7 }, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Bot(&s, (Bot){ .hp = 9 }, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Bot(&s, &s_token);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter lowers as slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Touch(s);\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void Touch(PgySlot_Vec2 *s)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "Touch(&s);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own SecureSlot<subject> parameter lowers as secure slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Consume(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void Consume(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2(s, (Vec2){ .x = 1, .y = 2 }, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2(s, &s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "Consume(&s, s_token);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure boundary forwarding preserves paired token through helper call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeInner(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeOuter(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeInner(s, s_token);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("extern block emits C prototypes");
    {
        ASTNode ext; memset(&ext, 0, sizeof(ext));
        ext.type = AST_EXTERN_BLOCK;
        ext.data.extern_block.abi = "C";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "flags";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode fn1; memset(&fn1, 0, sizeof(fn1));
        fn1.type = AST_FUNC_DECL;
        fn1.data.func_decl.name = "SDL_Init";
        fn1.data.func_decl.params = params;
        fn1.data.func_decl.param_count = 1;
        fn1.data.func_decl.return_type = make_type_node("Int");
        fn1.data.func_decl.body = NULL;

        ASTNode fn2; memset(&fn2, 0, sizeof(fn2));
        fn2.type = AST_FUNC_DECL;
        fn2.data.func_decl.name = "SDL_Quit";
        fn2.data.func_decl.params = NULL;
        fn2.data.func_decl.param_count = 0;
        fn2.data.func_decl.return_type = make_type_node("Void");
        fn2.data.func_decl.body = NULL;

        ASTNode *decls[2] = { &fn1, &fn2 };
        ext.data.extern_block.declarations = decls;
        ext.data.extern_block.count = 2;

        ASTNode *stmts[1] = { &ext };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "extern \"C\"");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t SDL_Init(int32_t flags);");
        EXPECT_STR_CONTAINS(ctx->out->data, "void SDL_Quit();");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("event declaration stays at file scope");
    {
        ASTNode event_node;
        ASTNode param_node;
        ASTNode *params[1] = { &param_node };
        ASTNode *stmts[2];
        ASTNode *prog;
        HIRProgram *hir;
        TranspilerCtx *ctx;
        const char *event_pos;
        const char *main_pos;

        memset(&event_node, 0, sizeof(event_node));
        memset(&param_node, 0, sizeof(param_node));

        event_node.type = AST_EVENT_DECL;
        event_node.data.event_decl.name = "OnHit";
        event_node.data.event_decl.params = params;
        event_node.data.event_decl.param_count = 1;
        event_node.data.event_decl.return_type = make_type_node("Void");

        param_node.type = AST_LET_DECL;
        param_node.data.let_decl.name = "damage";
        param_node.data.let_decl.type = make_type_node("Int");

        stmts[0] = &event_node;
        stmts[1] = make_let("boot", make_type_node("Int"), make_number(1, 1), 1);
        prog = make_program(stmts, 2);
        hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        event_pos = strstr(ctx->out->data, "typedef void (*OnHit_Handler)");
        main_pos = strstr(ctx->out->data, "\nint\nmain(void)\n");
        EXPECT(event_pos != NULL && main_pos != NULL && event_pos < main_pos);

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }
}

/* -----------------------------------------------------------------
 * Ability / Role codegen
 * ----------------------------------------------------------------- */

static void
test_ability_role_emit(void)
{
    printf("\n[ability_role_emit]\n");

    TEST("ability emits vtable typedef");
    {
        /* Build ability with one method manually (no ast_destroy — manual free) */
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "Damageable";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "amount";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "TakeDamage";
        method.data.func_decl.params = params;
        method.data.func_decl.param_count = 1;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        ability_node.data.ability_decl.methods = methods;
        ability_node.data.ability_decl.method_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_ability_decl(&ability_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "(*TakeDamage)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *self");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role emits static method and vtable instance");
    {
        ASTNode role_node; memset(&role_node, 0, sizeof(role_node));
        role_node.type = AST_ROLE_DECL;
        role_node.data.role_decl.name = "PlayerHeal";

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Heal";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *impl_methods[1] = { &method };
        ASTNode impl_node; memset(&impl_node, 0, sizeof(impl_node));
        impl_node.type = AST_IMPL_ABILITY;
        impl_node.data.impl_ability.ability_name = "Healable";
        impl_node.data.impl_ability.methods = impl_methods;
        impl_node.data.impl_ability.method_count = 1;

        ASTNode *impls[1] = { &impl_node };
        role_node.data.role_decl.impl_abilities = impls;
        role_node.data.role_decl.impl_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerHeal_Heal");
        EXPECT_STR_CONTAINS(ctx->out->data, "Healable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "vtable_instance");
        EXPECT_STR_CONTAINS(ctx->out->data, ".Heal = PlayerHeal_Heal");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role include copies inherited impls into current role");
    {
        ASTNode base_method; memset(&base_method, 0, sizeof(base_method));
        base_method.type = AST_FUNC_DECL;
        base_method.data.func_decl.name = "Tick";
        base_method.data.func_decl.return_type = make_type_node("Void");

        ASTNode *base_methods[1] = { &base_method };
        ASTNode base_impl; memset(&base_impl, 0, sizeof(base_impl));
        base_impl.type = AST_IMPL_ABILITY;
        base_impl.data.impl_ability.ability_name = "Updatable";
        base_impl.data.impl_ability.methods = base_methods;
        base_impl.data.impl_ability.method_count = 1;

        ASTNode *base_impls[1] = { &base_impl };
        ASTNode base_role; memset(&base_role, 0, sizeof(base_role));
        base_role.type = AST_ROLE_DECL;
        base_role.data.role_decl.name = "BaseRole";
        base_role.data.role_decl.impl_abilities = base_impls;
        base_role.data.role_decl.impl_count = 1;

        ASTNode include_stmt; memset(&include_stmt, 0, sizeof(include_stmt));
        include_stmt.type = AST_INCLUDE_STMT;
        include_stmt.data.include_stmt.role_name = "BaseRole";

        ASTNode *includes[1] = { &include_stmt };
        ASTNode derived_role; memset(&derived_role, 0, sizeof(derived_role));
        derived_role.type = AST_ROLE_DECL;
        derived_role.data.role_decl.name = "DerivedRole";
        derived_role.data.role_decl.includes = includes;
        derived_role.data.role_decl.include_count = 1;

        ASTNode *roles[2] = { &base_role, &derived_role };
        HIRProgram hir; memset(&hir, 0, sizeof(hir));
        hir.roles = roles;
        hir.role_count = 2;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = &hir;
        emit_role_decl(&derived_role, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Tick");
        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Updatable_vtable_instance");

        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Party codegen
 * ----------------------------------------------------------------- */

static void
test_party_emit(void)
{
    printf("\n[party_emit]\n");

    TEST("party emits struct with role slot and shared field");
    {
        ASTNode party_node; memset(&party_node, 0, sizeof(party_node));
        party_node.type = AST_PARTY_DECL;
        party_node.data.party_decl.name = "DungeonTeam";

        /* Role slot */
        ASTNode rs; memset(&rs, 0, sizeof(rs));
        rs.type = AST_ROLE_SLOT;
        rs.data.role_slot.slot_name = "tank";
        ASTNode ab_type; memset(&ab_type, 0, sizeof(ab_type));
        ab_type.type = AST_TYPE;
        ab_type.data.type.name = "Damageable";
        ASTNode *abilities[1] = { &ab_type };
        rs.data.role_slot.required_abilities = abilities;
        rs.data.role_slot.ability_count = 1;

        ASTNode *role_slots[1] = { &rs };
        party_node.data.party_decl.role_slots = role_slots;
        party_node.data.party_decl.role_count = 1;

        /* Shared field */
        ASTNode shared; memset(&shared, 0, sizeof(shared));
        shared.type = AST_PARTY_SHARED;
        shared.data.party_shared.name = "formation";
        shared.data.party_shared.type = make_type_node("String");

        ASTNode *shared_fields[1] = { &shared };
        party_node.data.party_decl.shared_fields = shared_fields;
        party_node.data.party_decl.shared_count = 1;

        party_node.data.party_decl.methods = NULL;
        party_node.data.party_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_party_decl(&party_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct DungeonTeam");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *tank");
        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "formation");
        EXPECT_STR_CONTAINS(ctx->out->data, "} DungeonTeam;");

        transpiler_ctx_destroy(ctx);
    }

    TEST("party instance emits C compound literal");
    {
        ASTNode value; memset(&value, 0, sizeof(value));
        value.type = AST_IDENTIFIER;
        value.data.identifier.name = "tankRole";

        ASTNode instance; memset(&instance, 0, sizeof(instance));
        instance.type = AST_PARTY_INSTANCE;
        instance.data.party_instance.party_type = "DungeonTeam";
        instance.data.party_instance.assignments = calloc(1, sizeof(*instance.data.party_instance.assignments));
        instance.data.party_instance.assignment_count = 1;
        instance.data.party_instance.assignments[0].slot_name = "tank";
        instance.data.party_instance.assignments[0].value = &value;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&instance, ctx);

        EXPECT_STR_CONTAINS(result, "(DungeonTeam){");
        EXPECT_STR_CONTAINS(result, ".tank = tankRole");

        free(result);
        free(instance.data.party_instance.assignments);
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Systemic / World codegen
 * ----------------------------------------------------------------- */

#include "tests/transpile/test_transpile_domain_async.inc"

static void
test_slot_sugar(void)
{
    printf("\n[slot_sugar]\n");

    TranspilerCtx *ctx;

    TEST("let x: Slot<Int> = 42 → claim + write");
    {
        ASTNode *node = make_let("x", make_type_node("Slot<Int>"),
                                  make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(strstr(out, "pgy_claim_Int()") != NULL);
        EXPECT(strstr(out, "pgy_write_Int(&x, 42)") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: identifier auto-read via Log");
    {
        /* Build: func Main() -> Void { let x: Slot<Int> = 42; Log(x); } */
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *x_ident = calloc(1, sizeof(ASTNode));
        x_ident->type = AST_IDENTIFIER; x_ident->line = 2;
        x_ident->data.identifier.name = pergyra_strdup("x");

        ASTNode *log_call = calloc(1, sizeof(ASTNode));
        log_call->type = AST_CALL; log_call->line = 2;
        ASTNode *log_id = calloc(1, sizeof(ASTNode));
        log_id->type = AST_IDENTIFIER; log_id->data.identifier.name = pergyra_strdup("Log");
        log_call->data.call.callee = log_id;
        log_call->data.call.arguments = malloc(sizeof(ASTNode*));
        log_call->data.call.arguments[0] = x_ident;
        log_call->data.call.arg_count = 1;

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body, let_node);
        ast_add_statement(fn_body, log_call);

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT(strstr(ctx->out->data, "pgy_read_Int(&x)") != NULL);
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("slot sugar: x = 5 auto-write");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *assign = calloc(1, sizeof(ASTNode));
        assign->type = AST_ASSIGNMENT; assign->line = 2;
        ASTNode *tgt = calloc(1, sizeof(ASTNode));
        tgt->type = AST_IDENTIFIER; tgt->data.identifier.name = pergyra_strdup("x");
        assign->data.assignment.target = tgt;
        assign->data.assignment.value  = make_number(5, 2);

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body, let_node);
        ast_add_statement(fn_body, assign);

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT(strstr(ctx->out->data, "pgy_write_Int(&x, 5)") != NULL);
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("explicit Release prevents double release");
    {
        /* let a: Slot<Int> = ClaimSlot<Int>(); Write(a,10); Release(a); */
        ASTNode *args0[0];
        ASTNode *init = make_call("ClaimSlot", args0, 0, 1);
        ASTNode *let_node = make_let("a", make_type_node("Slot<Int>"), init, 1);

        ASTNode *a_id = calloc(1, sizeof(ASTNode));
        a_id->type = AST_IDENTIFIER; a_id->data.identifier.name = pergyra_strdup("a");
        ASTNode *w_args[] = { a_id, make_number(10, 2) };
        ASTNode *write_call = make_call("Write", w_args, 2, 2);

        ASTNode *a_id2 = calloc(1, sizeof(ASTNode));
        a_id2->type = AST_IDENTIFIER; a_id2->data.identifier.name = pergyra_strdup("a");
        ASTNode *r_args[] = { a_id2 };
        ASTNode *rel_call = make_call("Release", r_args, 1, 3);

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body, let_node);
        ast_add_statement(fn_body, write_call);
        ast_add_statement(fn_body, rel_call);

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        int count = 0;
        const char *p = ctx->out->data;
        while ((p = strstr(p, "pgy_release_Int")) != NULL) { count++; p++; }
        EXPECT(count == 1);
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }
}

static void
test_stdlib_and_enum_emit(void)
{
    printf("\n[stdlib_enum]\n");

    TranspilerCtx *ctx;

    TEST("array literal let emits PgyArray_Int builder");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        ASTNode *node = make_let("values", make_generic_type("Array", "Int"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Int values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_push_Int");
        transpiler_ctx_destroy(ctx);
    }

    TEST("String built-ins map to runtime helpers");
    {
        ASTNode *args[2] = { make_string_lit("a", 1), make_string_lit("b", 1) };
        ASTNode *call = make_call("Concat", args, 2, 1);
        ASTNode *node = make_let("s", make_type_node("String"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringConcat(\"a\", \"b\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("enum variant identifier emits qualified C enum constant");
    {
        ASTNode *enum_decl = calloc(1, sizeof(ASTNode));
        enum_decl->type = AST_ENUM_DECL;
        enum_decl->data.enum_decl.name = pergyra_strdup("Color");
        enum_decl->data.enum_decl.variant_count = 2;
        enum_decl->data.enum_decl.variants = calloc(2, sizeof(char *));
        enum_decl->data.enum_decl.variants[0] = pergyra_strdup("Red");
        enum_decl->data.enum_decl.variants[1] = pergyra_strdup("Blue");

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body,
            make_let("c", make_type_node("Color"),
                make_identifier("Red", 2), 2));

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[2] = { enum_decl, fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Color_Red");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("operator overload dispatch uses operator_add_Type");
    {
        FuncParam opa, opb, maina, mainb;
        memset(&opa, 0, sizeof(opa)); opa.name = "a"; opa.type = make_type_node("Vec2");
        memset(&opb, 0, sizeof(opb)); opb.name = "b"; opb.type = make_type_node("Vec2");
        memset(&maina, 0, sizeof(maina)); maina.name = "a"; maina.type = make_type_node("Vec2");
        memset(&mainb, 0, sizeof(mainb)); mainb.name = "b"; mainb.type = make_type_node("Vec2");
        FuncParam *op_params[2] = { &opa, &opb };
        FuncParam *main_params[2] = { &maina, &mainb };

        ASTNode *op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = "operator_add_Vec2";
        op_fn->data.func_decl.params = op_params;
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *op_body = ast_create_block();
        ast_add_statement(op_body, make_return(make_identifier("a", 1), 1));
        op_fn->data.func_decl.body = op_body;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = main_params;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { op_fn, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "return operator_add_Vec2(a, b);");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }

    TEST("role ability Add emits operator_add_Type alias");
    {
        FuncParam rhs_param, a_param, b_param;
        memset(&rhs_param, 0, sizeof(rhs_param));
        memset(&a_param, 0, sizeof(a_param));
        memset(&b_param, 0, sizeof(b_param));
        rhs_param.name = "other";
        rhs_param.type = make_type_node("Int");
        a_param.name = "a";
        a_param.type = make_type_node("Int");
        b_param.name = "b";
        b_param.type = make_type_node("Int");

        ASTNode *role_method = calloc(1, sizeof(ASTNode));
        role_method->type = AST_FUNC_DECL;
        role_method->data.func_decl.name = "Add";
        role_method->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        role_method->data.func_decl.params[0] = &rhs_param;
        role_method->data.func_decl.param_count = 1;
        role_method->data.func_decl.return_type = make_type_node("Int");
        ASTNode *role_body = ast_create_block();
        ast_add_statement(role_body, make_return(make_number(123, 1), 1));
        role_method->data.func_decl.body = role_body;

        ASTNode *impl = calloc(1, sizeof(ASTNode));
        impl->type = AST_IMPL_ABILITY;
        impl->data.impl_ability.ability_name = "Arithmetic";
        impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode *));
        impl->data.impl_ability.methods[0] = role_method;
        impl->data.impl_ability.method_count = 1;

        ASTNode *role = calloc(1, sizeof(ASTNode));
        role->type = AST_ROLE_DECL;
        role->data.role_decl.name = "IntMath";
        role->data.role_decl.for_type = make_type_node("Int");
        role->data.role_decl.impl_abilities = calloc(1, sizeof(ASTNode *));
        role->data.role_decl.impl_abilities[0] = impl;
        role->data.role_decl.impl_count = 1;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        main_fn->data.func_decl.params[0] = &a_param;
        main_fn->data.func_decl.params[1] = &b_param;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Int");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { role, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = lower_program(prog);
        ctx = transpiler_ctx_create();
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Int(int32_t lhs, int32_t other)");
        EXPECT_STR_CONTAINS(ctx->out->data, "return operator_add_Int(a, b);");
        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */

int
main(void)
{
    printf("=== Pergyra C Transpiler Test Suite ===\n");

    type_system_init();

    test_codebuf();
    test_type_mapping();
    test_expression_emit();
    test_statement_emit();
    test_program_emit();
    test_ability_role_emit();
    test_party_emit();
    test_systemic_world_emit();
    test_async_emit();
    test_slot_sugar();
    test_stdlib_and_enum_emit();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
