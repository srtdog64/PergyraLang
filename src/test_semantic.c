/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic analyzer test suite
 * Build: make semantic_test
 * Run:   ./bin/semantic_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/string_compat.h"
#include "semantic/type_system.h"
#include "semantic/symbol_table.h"
#include "semantic/type_checker.h"
#include "semantic/semantic.h"

/* -----------------------------------------------------------------
 * Minimal AST node builder helpers (no full parser needed)
 * ----------------------------------------------------------------- */

static ASTNode *
make_number(double v, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type          = AST_NUMBER;
    n->line          = line;
    n->data.number.value = v;
    return n;
}

static ASTNode *
make_string(const char *s, uint32_t line)
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
make_boolean(bool v, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_BOOLEAN;
    n->line = line;
    n->data.boolean.value = v;
    return n;
}

static ASTNode *
make_member_access(ASTNode *object, const char *member, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MEMBER_ACCESS;
    n->line = line;
    n->data.member.object = object;
    n->data.member.name = pergyra_strdup(member);
    return n;
}

static ASTNode *
make_call_expr(ASTNode *callee, ASTNode **args, size_t arg_count, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_CALL;
    n->line = line;
    n->data.call.callee = callee;
    if (arg_count > 0) {
        n->data.call.arguments = calloc(arg_count, sizeof(ASTNode *));
        memcpy(n->data.call.arguments, args, arg_count * sizeof(ASTNode *));
    }
    n->data.call.arg_count = arg_count;
    return n;
}

static ASTNode *
make_call(const char *callee_name, ASTNode **args, size_t arg_count,
           uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_CALL;
    n->line = line;
    n->data.call.callee     = make_identifier(callee_name, line);
    if (arg_count > 0) {
        n->data.call.arguments = calloc(arg_count, sizeof(ASTNode *));
        memcpy(n->data.call.arguments, args, arg_count * sizeof(ASTNode *));
    }
    n->data.call.arg_count  = arg_count;
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

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-55s", name); } while(0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else       { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while(0)

static bool
ctx_has_diagnostic_substring(const SemanticContext *ctx, const char *needle)
{
    if (ctx == NULL || needle == NULL)
        return false;

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *diag = ctx->diagnostics[i];
        if (diag != NULL && diag->message != NULL
            && strstr(diag->message, needle) != NULL) {
            return true;
        }
    }

    return false;
}

/* -----------------------------------------------------------------
 * Test groups
 * ----------------------------------------------------------------- */

static void
test_type_system(void)
{
    printf("\n[type_system]\n");

    TEST("type_equals: Int == Int");
    EXPECT(type_equals(TYPE_INT, TYPE_INT));

    TEST("type_equals: Int != String");
    EXPECT(!type_equals(TYPE_INT, TYPE_STRING));

    TEST("type_is_assignable: Int -> Long widening");
    EXPECT(type_is_assignable(TYPE_INT, TYPE_LONG));

    TEST("type_is_assignable: String -> Int fails");
    EXPECT(!type_is_assignable(TYPE_STRING, TYPE_INT));

    TEST("type_create_slot: name is 'Slot<Int>'");
    Type *slot_int = type_create_slot(TYPE_INT, false);
    EXPECT(strcmp(slot_int->name, "Slot<Int>") == 0);

    TEST("type_create_slot secure: name is 'SecureSlot<Int>'");
    Type *sec_slot = type_create_slot(TYPE_INT, true);
    EXPECT(strcmp(sec_slot->name, "SecureSlot<Int>") == 0);

    TEST("type_equals: Slot<Int> == Slot<Int>");
    Type *slot_int2 = type_create_slot(TYPE_INT, false);
    EXPECT(type_equals(slot_int, slot_int2));

    TEST("type_equals: Slot<Int> != Slot<String>");
    Type *slot_str = type_create_slot(TYPE_STRING, false);
    EXPECT(!type_equals(slot_int, slot_str));

    TEST("type_infer_expression: identifier lookup returns bound type");
    {
        TypeEnv *env = type_env_create(NULL);
        type_env_add_variable(env, "count", TYPE_INT);
        ASTNode *id = make_identifier("count", 1);
        EXPECT(type_infer_expression(id, env) == TYPE_INT);
        ast_destroy(id);
        type_env_destroy(env);
    }

    TEST("type_infer_expression: array access returns element type");
    {
        Type *args[1] = { TYPE_FLOAT };
        Type *array_float = type_create_constructed(TYPE_ARRAY, args, 1);
        TypeEnv *env = type_env_create(NULL);
        type_env_add_variable(env, "items", array_float);

        ASTNode array_node; memset(&array_node, 0, sizeof(array_node));
        array_node.type = AST_IDENTIFIER;
        array_node.data.identifier.name = "items";

        ASTNode index_node; memset(&index_node, 0, sizeof(index_node));
        index_node.type = AST_NUMBER;
        index_node.data.number.value = 0;

        ASTNode access_node; memset(&access_node, 0, sizeof(access_node));
        access_node.type = AST_ARRAY_ACCESS;
        access_node.data.array_access.array = &array_node;
        access_node.data.array_access.index = &index_node;

        EXPECT(type_infer_expression(&access_node, env) == TYPE_FLOAT);

        free(array_float->data.constructed.args);
        free(array_float->name);
        free(array_float);
        type_env_destroy(env);
    }

    TEST("type_infer_expression: comparison returns Bool");
    {
        ASTNode left; memset(&left, 0, sizeof(left));
        left.type = AST_NUMBER;
        left.data.number.value = 1;

        ASTNode right; memset(&right, 0, sizeof(right));
        right.type = AST_NUMBER;
        right.data.number.value = 2;

        ASTNode expr; memset(&expr, 0, sizeof(expr));
        expr.type = AST_BINARY;
        expr.data.binary.left = &left;
        expr.data.binary.right = &right;
        expr.data.binary.op.type = TOKEN_LESS;

        EXPECT(type_infer_expression(&expr, NULL) == TYPE_BOOL);
    }
}

static void
test_symbol_table(void)
{
    printf("\n[symbol_table]\n");

    Scope *root = scope_create(NULL, SCOPE_GLOBAL);

    TEST("scope_declare and lookup in same scope");
    Symbol *sym = symbol_create_variable("x", TYPE_INT, 1, 1);
    scope_declare(root, sym);
    EXPECT(scope_lookup(root, "x") == sym);

    TEST("scope_lookup returns NULL for unknown symbol");
    EXPECT(scope_lookup(root, "y") == NULL);

    TEST("scope_declare: duplicate in same scope returns false");
    Symbol *dup = symbol_create_variable("x", TYPE_STRING, 2, 1);
    EXPECT(!scope_declare(root, dup));
    symbol_destroy(dup);

    TEST("child scope can see parent symbols");
    Scope *child = scope_create(root, SCOPE_BLOCK);
    EXPECT(scope_lookup(child, "x") == sym);

    TEST("shadowing: child declares same name, lookup returns child sym");
    Symbol *shadow = symbol_create_variable("x", TYPE_STRING, 3, 1);
    scope_declare(child, shadow);
    EXPECT(scope_lookup(child, "x") == shadow);
    EXPECT(scope_lookup(root, "x")  == sym); /* Parent unaffected */

    TEST("scope_release_slot: marks slot as RELEASED");
    Symbol *slot = symbol_create_slot("mySlot", type_create_slot(TYPE_INT, false),
                                       false, NULL, 5, 1);
    scope_declare(child, slot);
    EXPECT(slot->slot_info.state == SLOT_STATE_CLAIMED);
    scope_release_slot(child, "mySlot");
    EXPECT(slot->slot_info.state == SLOT_STATE_RELEASED);

    TEST("scope_auto_release_slots: releases all owned slots");
    Scope *with_scope = scope_create(child, SCOPE_WITH);
    Symbol *auto_slot = symbol_create_slot("autoSlot",
                            type_create_slot(TYPE_INT, false),
                            false, NULL, 6, 1);
    scope_declare(with_scope, auto_slot);
    scope_register_slot(with_scope, auto_slot);
    EXPECT(auto_slot->slot_info.state == SLOT_STATE_CLAIMED);
    scope_auto_release_slots(with_scope);
    EXPECT(auto_slot->slot_info.state == SLOT_STATE_RELEASED);

    scope_destroy(with_scope);
    scope_destroy(child);
    scope_destroy(root);
}

static void
test_type_checker_slot_rules(void)
{
    printf("\n[type_checker — slot rules]\n");

    /* --- R1: Write type mismatch --- */
    TEST("R1: Write String to Slot<Int> → error");
    {
        SemanticContext *ctx = semantic_context_create();

        /* Register a Slot<Int> named 's' */
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        /* Write(s, "hello") */
        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_string("hello", 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("R1: Write Int to Slot<Int> → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- R2: SecureSlot requires token --- */
    TEST("R2: Write to SecureSlot without token → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("ss", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("R2: Write to SecureSlot with correct token → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        Symbol *tok_sym = symbol_create_token("tok", "ss", 1, 2);
        scope_declare(ctx->scope, tok_sym);

        ASTNode *args[3] = {
            make_identifier("ss", 2),
            make_number(42, 2),
            make_identifier("tok", 2)
        };
        ASTNode *call = make_call("Write", args, 3, 2);

        type_check_write_slot(call, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- R3: wrong token --- */
    TEST("R3: Write to SecureSlot with wrong token → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tokA", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[3] = {
            make_identifier("ss", 2),
            make_number(42, 2),
            make_identifier("tokB", 2)  /* wrong token */
        };
        ASTNode *call = make_call("Write", args, 3, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- R4: write to released slot --- */
    TEST("R4: Write to released slot → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        slot_sym->slot_info.state = SLOT_STATE_RELEASED;
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- Read from released slot --- */
    TEST("Read from released slot → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        slot_sym->slot_info.state = SLOT_STATE_RELEASED;
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[1] = { make_identifier("s", 3) };
        ASTNode *call    = make_call("Read", args, 1, 3);

        Type *t = type_check_read_slot(call, ctx);
        EXPECT(ctx->has_error);
        (void)t;
        semantic_context_destroy(ctx);
    }

    /* --- Release twice --- */
    TEST("Release already-released slot → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[1] = { make_identifier("s", 4) };
        ASTNode *call    = make_call("Release", args, 1, 4);

        /* First release: OK */
        type_check_release_slot(call, ctx);
        EXPECT(!ctx->has_error);

        /* Second release: error */
        type_check_release_slot(call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
    }
}

static void
test_undefined_symbol(void)
{
    printf("\n[type_checker — symbol resolution]\n");

    TEST("Undefined identifier → error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *id = make_identifier("nope", 1);
        type_check_expression(id, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("Defined identifier → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *sym = symbol_create_variable("x", TYPE_INT, 1, 1);
        scope_declare(ctx->scope, sym);
        ASTNode *id = make_identifier("x", 2);
        Type *t = type_check_expression(id, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("Private namespace function access inside Log → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *param_types[2] = { TYPE_INT, TYPE_INT };
        Type *math_add = type_create_function(param_types, 2, TYPE_INT);
        scope_declare(ctx->scope,
            symbol_create_function("Math_Add", math_add, 1, 1));

        ASTNode *hidden_args[2] = {
            make_number(2, 2),
            make_number(5, 2)
        };
        ASTNode *hidden_call = make_call_expr(
            make_member_access(make_identifier("Math", 2), "HiddenAdd", 2),
            hidden_args, 2, 2);
        ASTNode *log_args[1] = { hidden_call };
        ASTNode *log_call = make_call("Log", log_args, 1, 2);

        type_check_expression(log_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "export visibility"));

        semantic_context_destroy(ctx);
        ast_destroy(log_call);
    }
}

static void
test_while_loop(void)
{
    printf("\n[type_checker — while loop]\n");

    TEST("While loop with Bool condition → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        /* Build: while true { } */
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        ASTNode *cond = calloc(1, sizeof(ASTNode));
        cond->type = AST_BOOLEAN;
        cond->data.boolean.value = true;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        wh->data.while_loop.condition = cond;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        free(wh); free(cond); free(body);
    }

    TEST("While loop with Int condition → error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        ASTNode *cond = calloc(1, sizeof(ASTNode));
        cond->type = AST_NUMBER;
        cond->data.number.value = 42;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        wh->data.while_loop.condition = cond;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        free(wh); free(cond); free(body);
    }

    TEST("break outside loop → error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 1;
        type_check_statement(br, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        free(br);
    }

    TEST("continue inside while loop → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        wh->data.while_loop.condition = make_boolean(true, 1);
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.count = 1;
        body->data.block.statements = calloc(1, sizeof(ASTNode *));
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 2;
        body->data.block.statements[0] = cont;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(wh->data.while_loop.condition);
        free(body->data.block.statements);
        free(cont);
        free(body);
        free(wh);
    }

    TEST("break skips unreachable QubitSlot move in while loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 2;
        wh->data.while_loop.condition = make_boolean(true, 2);
        ASTNode *body = ast_create_block();
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 3;
        ast_add_statement(body, br);
        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(body, moved);
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        Type *t = type_check_expression(state_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(wh);
        ast_destroy(state_call);
    }

    TEST("continue skips unreachable QubitSlot move in while loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 2;
        wh->data.while_loop.condition = make_boolean(true, 2);
        ASTNode *body = ast_create_block();
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 3;
        ast_add_statement(body, cont);
        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(body, moved);
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        Type *t = type_check_expression(state_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(wh);
        ast_destroy(state_call);
    }
}

static void
test_arrays_and_enums(void)
{
    printf("\n[arrays_enums]\n");

    TEST("array literal infers Array<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        Type *t = type_check_expression(arr, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Array<Int>") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(arr);
    }

    TEST("mixed array literal elements → error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 2;
        arr->data.array_literal.elements = calloc(2, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_string("oops", 1);

        type_check_expression(arr, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(arr);
    }

    TEST("enum variants are visible as enum-typed identifiers");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *enum_decl = calloc(1, sizeof(ASTNode));
        enum_decl->type = AST_ENUM_DECL;
        enum_decl->line = 1;
        enum_decl->data.enum_decl.name = pergyra_strdup("Color");
        enum_decl->data.enum_decl.variant_count = 2;
        enum_decl->data.enum_decl.variants = calloc(2, sizeof(char *));
        enum_decl->data.enum_decl.variants[0] = pergyra_strdup("Red");
        enum_decl->data.enum_decl.variants[1] = pergyra_strdup("Blue");
        ASTNode *stmts[1] = { enum_decl };
        ASTNode *prog = make_program(stmts, 1);

        type_check_program(prog, ctx);
        Type *t = type_check_expression(make_identifier("Red", 2), ctx);
        EXPECT(!ctx->has_error && t != NULL && strcmp(t->name, "Color") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(prog);
    }
}

static void
test_stdlib_and_io(void)
{
    printf("\n[stdlib_io]\n");

    TEST("StringContains returns Bool with valid args");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[2] = { make_string("hello", 1), make_string("ell", 1) };
        Type *t = type_check_expression(make_call("StringContains", args, 2, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));
        semantic_context_destroy(ctx);
    }

    TEST("ArrayLength requires Array<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(42, 1) };
        type_check_expression(make_call("ArrayLength", args, 1, 1), ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("ReadFile requires String path");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(1, 1) };
        type_check_expression(make_call("ReadFile", args, 1, 1), ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("WriteFile accepts String path and data");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[2] = { make_string("out.txt", 1), make_string("data", 1) };
        Type *t = type_check_expression(make_call("WriteFile", args, 2, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));
        semantic_context_destroy(ctx);
    }

    TEST("slot sugar let declaration registers Slot symbol");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("s");
        decl->data.let_decl.type = make_generic_type("Slot", "Int");
        decl->data.let_decl.initializer = make_number(42, 1);
        type_check_let_decl(decl, ctx);
        Symbol *sym = scope_lookup(ctx->scope, "s");
        EXPECT(!ctx->has_error && sym != NULL && sym->kind == SYMBOL_SLOT);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("slot handle alias in let declaration is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("t");
        decl->data.let_decl.type = make_generic_type("Slot", "Int");
        decl->data.let_decl.initializer = make_identifier("s", 2);
        type_check_let_decl(decl, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("secure slot handle alias in let declaration is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("ss", 2);
        type_check_let_decl(decl, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("slot sugar assignment from inner value remains valid");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *assign = ast_create_assignment(make_identifier("s", 2), make_number(7, 2));
        Type *t = type_check_expression(assign, ctx);

        EXPECT(!ctx->has_error && t != NULL && t->kind == TYPE_KIND_SLOT);
        semantic_context_destroy(ctx);
        ast_destroy(assign);
    }

    TEST("slot sugar binary expression auto-reads inner value");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *expr = ast_create_binary(make_identifier("s", 2),
            (Token){ .type = TOKEN_PLUS }, make_number(1, 2));
        Type *t = type_check_expression(expr, ctx);

        EXPECT(!ctx->has_error && t != NULL && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
        ast_destroy(expr);
    }

    TEST("slot handle assignment from another slot is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_a = symbol_create_slot(
            "a", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        Symbol *slot_b = symbol_create_slot(
            "b", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_a);
        scope_register_slot(ctx->scope, slot_a);
        scope_declare(ctx->scope, slot_b);
        scope_register_slot(ctx->scope, slot_b);

        ASTNode *assign = ast_create_assignment(make_identifier("a", 2), make_identifier("b", 2));
        type_check_expression(assign, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "cannot be copied or rebound with '='"));
        semantic_context_destroy(ctx);
        ast_destroy(assign);
    }
}

static void
test_qubit_slot_semantics(void)
{
    printf("\n[qubit_slot]\n");

    TEST("ClaimQubit infers QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *call = make_call("ClaimQubit", NULL, 0, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_QUBIT));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("QubitSlot let declaration from ClaimQubit passes");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("QubitSlot move in let declaration consumes source");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        ASTNode *args[1] = { make_identifier("q", 3) };
        ASTNode *call = make_call("QubitState", args, 1, 3);
        type_check_expression(call, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(call);
    }

    TEST("Measure requires QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(1, 1) };
        ASTNode *call = make_call("Measure", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("if branches move QubitSlot independently without cross-branch false positive");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *if_stmt = ast_create_if_statement();
        if_stmt->data.if_stmt.condition = make_boolean(true, 2);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_decl = ast_create_let_declaration("a");
        then_decl->data.let_decl.type = ast_create_type("QubitSlot");
        then_decl->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(then_block, then_decl);
        ASTNode *then_release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(then_block, make_call("ReleaseQubit", then_release_args, 1, 4));
        if_stmt->data.if_stmt.then_branch = then_block;

        ASTNode *else_block = ast_create_block();
        ASTNode *else_decl = ast_create_let_declaration("b");
        else_decl->data.let_decl.type = ast_create_type("QubitSlot");
        else_decl->data.let_decl.initializer = make_identifier("q", 5);
        ast_add_statement(else_block, else_decl);
        ASTNode *else_release_args[1] = { make_identifier("b", 6) };
        ast_add_statement(else_block, make_call("ReleaseQubit", else_release_args, 1, 6));
        if_stmt->data.if_stmt.else_branch = else_block;

        type_check_if_stmt(if_stmt, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 7) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 7);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(if_stmt);
        ast_destroy(state_call);
    }

    TEST("match cases move QubitSlot independently without cross-case false positive");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *match = ast_create_match_statement();
        match->data.match_stmt.subject = make_number(0, 2);

        ASTNode *case0 = ast_create_match_case();
        case0->data.match_case.pattern = make_number(0, 3);
        case0->data.match_case.body = ast_create_block();
        ASTNode *case0_decl = ast_create_let_declaration("a");
        case0_decl->data.let_decl.type = ast_create_type("QubitSlot");
        case0_decl->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(case0->data.match_case.body, case0_decl);
        ASTNode *case0_release_args[1] = { make_identifier("a", 5) };
        ast_add_statement(case0->data.match_case.body,
            make_call("ReleaseQubit", case0_release_args, 1, 5));

        ASTNode *case1 = ast_create_match_case();
        case1->data.match_case.pattern = make_number(1, 6);
        case1->data.match_case.body = ast_create_block();
        ASTNode *case1_decl = ast_create_let_declaration("b");
        case1_decl->data.let_decl.type = ast_create_type("QubitSlot");
        case1_decl->data.let_decl.initializer = make_identifier("q", 7);
        ast_add_statement(case1->data.match_case.body, case1_decl);
        ASTNode *case1_release_args[1] = { make_identifier("b", 8) };
        ast_add_statement(case1->data.match_case.body,
            make_call("ReleaseQubit", case1_release_args, 1, 8));

        match->data.match_stmt.cases = calloc(2, sizeof(ASTNode *));
        match->data.match_stmt.cases[0] = case0;
        match->data.match_stmt.cases[1] = case1;
        match->data.match_stmt.case_count = 2;

        type_check_match_stmt(match, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 9) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 9);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(match);
        ast_destroy(state_call);
    }

    TEST("while loop body move with break consumes QubitSlot after loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.while_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 5;
        ast_add_statement(loop->data.while_loop.body, br);

        type_check_while_loop(loop, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 6) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 6);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
        ast_destroy(state_call);
    }

    TEST("while loop continue path reuses moved QubitSlot on next iteration");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 4;
        ast_add_statement(loop->data.while_loop.body, cont);

        type_check_while_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("while loop fallthrough reuses moved QubitSlot on next iteration");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.while_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));

        type_check_while_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("for loop body move conservatively consumes QubitSlot after loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_for_loop();
        loop->data.for_loop.variable = pergyra_strdup("i");
        loop->data.for_loop.range_start = make_number(0, 2);
        loop->data.for_loop.range_end = make_number(1, 2);
        loop->data.for_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.for_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.for_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));

        type_check_for_loop(loop, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
        ast_destroy(state_call);
    }

    TEST("for loop repeated iterations reuse moved QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_for_loop();
        loop->data.for_loop.variable = pergyra_strdup("i");
        loop->data.for_loop.range_start = make_number(0, 2);
        loop->data.for_loop.range_end = make_number(2, 2);
        loop->data.for_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.for_loop.body, moved);

        type_check_for_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("ReleaseQubit after move reports one diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        size_t before = ctx->diagnostic_count;
        ASTNode *release_args[1] = { make_identifier("q", 3) };
        ASTNode *release = make_call("ReleaseQubit", release_args, 1, 3);
        type_check_expression(release, ctx);

        EXPECT(ctx->has_error && ctx->diagnostic_count == before + 1);

        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(release);
    }

    TEST("moving consumed QubitSlot into let reports one diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        size_t before = ctx->diagnostic_count;
        ASTNode *decl3 = ast_create_let_declaration("q3");
        decl3->data.let_decl.type = ast_create_type("QubitSlot");
        decl3->data.let_decl.initializer = make_identifier("q", 3);
        type_check_let_decl(decl3, ctx);

        EXPECT(ctx->has_error && ctx->diagnostic_count == before + 1);

        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(decl3);
    }

    TEST("return skips unreachable QubitSlot move in function body");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Pass");
        func->data.func_decl.return_type = ast_create_type("QubitSlot");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_identifier("q", 2);
        ast_add_statement(func->data.func_decl.body, ret);

        ASTNode *unreachable = ast_create_let_declaration("q2");
        unreachable->data.let_decl.type = ast_create_type("QubitSlot");
        unreachable->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(func->data.func_decl.body, unreachable);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("if-return skips unreachable QubitSlot move in branch");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("BranchPass");
        func->data.func_decl.return_type = ast_create_type("QubitSlot");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *if_stmt = ast_create_if_statement();
        if_stmt->data.if_stmt.condition = make_boolean(true, 2);
        ASTNode *then_block = ast_create_block();
        ASTNode *then_ret = ast_create_return_statement();
        then_ret->data.return_stmt.value = make_identifier("q", 3);
        ast_add_statement(then_block, then_ret);
        ASTNode *unreachable = ast_create_let_declaration("q2");
        unreachable->data.let_decl.type = ast_create_type("QubitSlot");
        unreachable->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(then_block, unreachable);
        if_stmt->data.if_stmt.then_branch = then_block;
        ASTNode *else_block = ast_create_block();
        ASTNode *else_ret = ast_create_return_statement();
        else_ret->data.return_stmt.value = make_call("ClaimQubit", NULL, 0, 5);
        ast_add_statement(else_block, else_ret);
        if_stmt->data.if_stmt.else_branch = else_block;
        ast_add_statement(func->data.func_decl.body, if_stmt);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("QubitSlot function argument moves from named variable");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseQubit");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("q");
        param->type = ast_create_type("QubitSlot");
        func->data.func_decl.params[0] = param;
        type_check_func_decl(func, ctx);

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        type_check_let_decl(decl, ctx);

        ASTNode *call_args[1] = { make_identifier("q", 3) };
        ASTNode *call = make_call("UseQubit", call_args, 1, 3);
        type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 4) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 4);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(decl);
        ast_destroy(call);
        ast_destroy(state_call);
    }

    TEST("QubitSlot function argument rejects anonymous temporary");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseQubit");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("q");
        param->type = ast_create_type("QubitSlot");
        func->data.func_decl.params[0] = param;
        type_check_func_decl(func, ctx);

        ASTNode *temp_args[1] = { make_call("ClaimQubit", NULL, 0, 2) };
        ASTNode *call = make_call("UseQubit", temp_args, 1, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bind the value first"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(call);
    }

    TEST("Slot parameter types are rejected for now");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseSlot");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("s");
        param->type = make_generic_type("Slot", "Int");
        func->data.func_decl.params[0] = param;

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("Slot return types are rejected for now");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("MakeSlot");
        func->data.func_decl.return_type = make_generic_type("Slot", "Int");
        func->data.func_decl.body = ast_create_block();

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }
}

static void
test_quantum_extensions(void)
{
    printf("\n[quantum_extensions]\n");

    TEST("IntoClassical on COLLAPSED qubit returns Bool");
    {
        /* func F() -> Void { let q = ClaimQubit(); Measure(q); IntoClassical(q); } */
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("F");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *marg = ast_create_identifier("q");
        ASTNode *meas = make_call("Measure", &marg, 1, 2);
        ast_add_statement(func->data.func_decl.body, meas);

        ASTNode *carg = ast_create_identifier("q");
        ASTNode *into = make_call("IntoClassical", &carg, 1, 3);
        ASTNode *let_c = ast_create_let_declaration("c");
        let_c->data.let_decl.type = ast_create_type("Bool");
        let_c->data.let_decl.initializer = into;
        ast_add_statement(func->data.func_decl.body, let_c);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("IntoClassical on unmeasured qubit triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("F");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        /* IntoClassical without Measure first */
        ASTNode *carg = ast_create_identifier("q");
        ASTNode *into = make_call("IntoClassical", &carg, 1, 2);
        ast_add_statement(func->data.func_decl.body, into);

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("IntoClassical consumes qubit — further use triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("F");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *marg = ast_create_identifier("q");
        ast_add_statement(func->data.func_decl.body,
            make_call("Measure", &marg, 1, 2));

        ASTNode *carg = ast_create_identifier("q");
        ast_add_statement(func->data.func_decl.body,
            make_call("IntoClassical", &carg, 1, 3));

        /* Use after IntoClassical should fail */
        ASTNode *qarg = ast_create_identifier("q");
        ast_add_statement(func->data.func_decl.body,
            make_call("QubitState", &qarg, 1, 4));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("Entangle after Measure triggers error (COLLAPSED state)");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("F");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        ASTNode *da = ast_create_let_declaration("a");
        da->data.let_decl.type = ast_create_type("QubitSlot");
        da->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, da);

        ASTNode *db = ast_create_let_declaration("b");
        db->data.let_decl.type = ast_create_type("QubitSlot");
        db->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(func->data.func_decl.body, db);

        /* Measure a first */
        ASTNode *marg = ast_create_identifier("a");
        ast_add_statement(func->data.func_decl.body,
            make_call("Measure", &marg, 1, 3));

        /* Entangle(a, b) should fail — a is COLLAPSED */
        ASTNode *eargs[2] = { ast_create_identifier("a"), ast_create_identifier("b") };
        ast_add_statement(func->data.func_decl.body,
            make_call("Entangle", eargs, 2, 4));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("H() builtin resolves without error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("F");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *harg = ast_create_identifier("q");
        ast_add_statement(func->data.func_decl.body,
            make_call("H", &harg, 1, 2));

        ASTNode *rel_arg = ast_create_identifier("q");
        ASTNode *rel = make_call("ReleaseQubit", &rel_arg, 1, 3);
        ast_add_statement(func->data.func_decl.body, rel);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(func);
    }
}

static void
test_match_stmt(void)
{
    printf("\n[type_checker — match statement]\n");

    TEST("Match with compatible Int cases → no error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *match = calloc(1, sizeof(ASTNode));
        match->type = AST_MATCH_STMT;
        match->line = 1;
        match->data.match_stmt.subject = make_number(42, 1);
        match->data.match_stmt.default_body = NULL;

        ASTNode *mc = calloc(1, sizeof(ASTNode));
        mc->type = AST_MATCH_CASE;
        mc->line = 2;
        mc->data.match_case.pattern = make_number(0, 2);
        mc->data.match_case.guard = NULL;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        mc->data.match_case.body = body;

        match->data.match_stmt.cases = malloc(sizeof(ASTNode*));
        match->data.match_stmt.cases[0] = mc;
        match->data.match_stmt.case_count = 1;

        type_check_match_stmt(match, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        free(match->data.match_stmt.cases);
        free(match); free(mc); free(body);
    }

    TEST("Match with String guard (not Bool) → error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *match = calloc(1, sizeof(ASTNode));
        match->type = AST_MATCH_STMT;
        match->line = 1;
        match->data.match_stmt.subject = make_number(1, 1);
        match->data.match_stmt.default_body = NULL;

        ASTNode *mc = calloc(1, sizeof(ASTNode));
        mc->type = AST_MATCH_CASE;
        mc->line = 2;
        mc->data.match_case.pattern = make_number(0, 2);
        mc->data.match_case.guard = make_string("bad", 2);
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        mc->data.match_case.body = body;

        match->data.match_stmt.cases = malloc(sizeof(ASTNode*));
        match->data.match_stmt.cases[0] = mc;
        match->data.match_stmt.case_count = 1;

        type_check_match_stmt(match, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        free(match->data.match_stmt.cases);
        free(match); free(mc); free(body);
    }
}

/* -----------------------------------------------------------------
 * Ability / Role declarations
 * ----------------------------------------------------------------- */

static void
test_ability_decl(void)
{
    printf("\n[ability_decl]\n");

    TEST("valid ability with require field passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability = ast_create_ability_declaration("Damageable");
        ability->line = 1; ability->column = 1;

        ASTNode *req = ast_create_require_field("health");
        req->data.require_field.type = ast_create_type("Int");
        req->line = 2; req->column = 1;
        ability->data.ability_decl.require_count = 1;
        ability->data.ability_decl.require_fields = malloc(sizeof(ASTNode*));
        ability->data.ability_decl.require_fields[0] = req;

        type_check_ability_decl(ability, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability);
    }

    TEST("duplicate ability declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability1 = ast_create_ability_declaration("Foo");
        ability1->line = 1; ability1->column = 1;
        type_check_ability_decl(ability1, ctx);

        ASTNode *ability2 = ast_create_ability_declaration("Foo");
        ability2->line = 2; ability2->column = 1;
        type_check_ability_decl(ability2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability1);
        ast_destroy(ability2);
    }
}

static void
test_role_decl(void)
{
    printf("\n[role_decl]\n");

    TEST("valid role with impl ability passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability = ast_create_ability_declaration("Healable");
        ability->line = 1; ability->column = 1;
        type_check_ability_decl(ability, ctx);

        ASTNode *role = ast_create_role_declaration("HealerRole");
        role->line = 3; role->column = 1;

        ASTNode *impl = ast_create_impl_ability("Healable");
        impl->line = 4; impl->column = 1;
        role->data.role_decl.impl_count = 1;
        role->data.role_decl.impl_abilities = malloc(sizeof(ASTNode*));
        role->data.role_decl.impl_abilities[0] = impl;

        type_check_role_decl(role, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability);
        ast_destroy(role);
    }

    TEST("role with unknown ability produces warning");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *role = ast_create_role_declaration("BadRole");
        role->line = 1; role->column = 1;

        ASTNode *impl = ast_create_impl_ability("NonExistent");
        impl->line = 2; impl->column = 1;
        role->data.role_decl.impl_count = 1;
        role->data.role_decl.impl_abilities = malloc(sizeof(ASTNode*));
        role->data.role_decl.impl_abilities[0] = impl;

        type_check_role_decl(role, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(ctx->diagnostic_count > 0);

        semantic_context_destroy(ctx);
        ast_destroy(role);
    }

    TEST("role ability Add enables operator overload on target type");
    {
        SemanticContext *ctx = semantic_context_create();

        FuncParam *rhs = calloc(1, sizeof(FuncParam));
        rhs->name = pergyra_strdup("other");
        rhs->type = ast_create_type("Int");

        ASTNode *method = calloc(1, sizeof(ASTNode));
        method->type = AST_FUNC_DECL;
        method->data.func_decl.name = pergyra_strdup("Add");
        method->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        method->data.func_decl.params[0] = rhs;
        method->data.func_decl.param_count = 1;
        method->data.func_decl.return_type = ast_create_type("Int");
        ASTNode *method_ret = calloc(1, sizeof(ASTNode));
        method_ret->type = AST_RETURN;
        method_ret->data.return_stmt.value = make_number(123, 3);
        ASTNode *method_body = calloc(1, sizeof(ASTNode));
        method_body->type = AST_BLOCK;
        method_body->data.block.statements = calloc(1, sizeof(ASTNode *));
        method_body->data.block.statements[0] = method_ret;
        method_body->data.block.count = 1;
        method->data.func_decl.body = method_body;

        ASTNode *impl = ast_create_impl_ability("Arithmetic");
        impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode *));
        impl->data.impl_ability.methods[0] = method;
        impl->data.impl_ability.method_count = 1;

        ASTNode *role = ast_create_role_declaration("IntMath");
        role->data.role_decl.for_type = ast_create_type("Int");
        role->data.role_decl.impl_abilities = calloc(1, sizeof(ASTNode *));
        role->data.role_decl.impl_abilities[0] = impl;
        role->data.role_decl.impl_count = 1;

        FuncParam *a = calloc(1, sizeof(FuncParam));
        FuncParam *b = calloc(1, sizeof(FuncParam));
        a->name = pergyra_strdup("a");
        b->name = pergyra_strdup("b");
        a->type = ast_create_type("Int");
        b->type = ast_create_type("Int");

        ASTNode *binary = ast_create_binary(make_identifier("a", 8),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 8));
        ASTNode *ret = calloc(1, sizeof(ASTNode));
        ret->type = AST_RETURN;
        ret->data.return_stmt.value = binary;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = calloc(1, sizeof(ASTNode *));
        body->data.block.statements[0] = ret;
        body->data.block.count = 1;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = pergyra_strdup("Main");
        main_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        main_fn->data.func_decl.params[0] = a;
        main_fn->data.func_decl.params[1] = b;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = ast_create_type("Int");
        main_fn->data.func_decl.body = body;

        ASTNode *stmts[2] = { role, main_fn };
        ASTNode *program = make_program(stmts, 2);

        EXPECT(type_check_program(program, ctx));
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(program);
    }
}

/* -----------------------------------------------------------------
 * Party declarations
 * ----------------------------------------------------------------- */

static void
test_party_decl(void)
{
    printf("\n[party_decl]\n");

    TEST("valid party with role slot and shared field passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        /* Register an ability first */
        ASTNode *ability = ast_create_ability_declaration("Damageable");
        ability->line = 1; ability->column = 1;
        type_check_ability_decl(ability, ctx);

        /* Create party */
        ASTNode *party = ast_create_party_declaration("DungeonTeam");
        party->line = 3; party->column = 1;

        /* Add role slot */
        ASTNode *rs = ast_create_role_slot("tank");
        rs->line = 4; rs->column = 1;
        ASTNode *ab_type = ast_create_type("Damageable");
        rs->data.role_slot.ability_count = 1;
        rs->data.role_slot.required_abilities = malloc(sizeof(ASTNode*));
        rs->data.role_slot.required_abilities[0] = ab_type;
        party->data.party_decl.role_count = 1;
        party->data.party_decl.role_slots = malloc(sizeof(ASTNode*));
        party->data.party_decl.role_slots[0] = rs;

        type_check_party_decl(party, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability);
        ast_destroy(party);
    }

    TEST("duplicate party declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *p1 = ast_create_party_declaration("Team");
        p1->line = 1; p1->column = 1;
        type_check_party_decl(p1, ctx);

        ASTNode *p2 = ast_create_party_declaration("Team");
        p2->line = 2; p2->column = 1;
        type_check_party_decl(p2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(p1);
        ast_destroy(p2);
    }
}

/* -----------------------------------------------------------------
 * Systemic / World declarations
 * ----------------------------------------------------------------- */

static void
test_systemic_world_decl(void)
{
    printf("\n[systemic_world_decl]\n");

    TEST("valid systemic declaration passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sys = ast_create_systemic_declaration("CombatSystem");
        sys->line = 1; sys->column = 1;
        type_check_systemic_decl(sys, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sys);
    }

    TEST("valid world with systemic ref passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        /* Register systemic first */
        ASTNode *sys = ast_create_systemic_declaration("Combat");
        sys->line = 1; sys->column = 1;
        type_check_systemic_decl(sys, ctx);

        /* Create world referencing it */
        ASTNode *world = ast_create_world_declaration("GameWorld");
        world->line = 3; world->column = 1;
        ASTNode *ws = ast_create_world_systemic("combat", "Combat");
        ws->line = 4; ws->column = 1;
        world->data.world_decl.systemic_count = 1;
        world->data.world_decl.systemics = malloc(sizeof(ASTNode*));
        world->data.world_decl.systemics[0] = ws;

        type_check_world_decl(world, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sys);
        ast_destroy(world);
    }
}

static void
test_extern_block(void)
{
    printf("\n[extern_block]\n");

    TEST("extern C function is visible to later call");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *ext = ast_create_extern_block("C");
        ext->line = 1; ext->column = 1;

        ASTNode *fn = ast_create_function("SDL_Init");
        fn->line = 2; fn->column = 5;
        fn->data.func_decl.return_type = ast_create_type("Int");
        fn->data.func_decl.param_count = 1;
        fn->data.func_decl.params = calloc(1, sizeof(FuncParam*));

        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("flags");
        param->type = ast_create_type("Int");
        fn->data.func_decl.params[0] = param;

        ast_add_statement(ext, fn);

        ASTNode **call_args = calloc(1, sizeof(ASTNode*));
        call_args[0] = make_number(0, 4);
        ASTNode *call = make_call("SDL_Init", call_args, 1, 4);
        ASTNode *decl = ast_create_let_declaration("result");
        decl->line = 4; decl->column = 1;
        decl->data.let_decl.initializer = call;

        ASTNode *stmts[2] = { ext, decl };
        ASTNode *program = make_program(stmts, 2);

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ext);
        ast_destroy(decl);
        free(program);
    }
}

static void
test_engine_collections(void)
{
    printf("\n[engine_collections]\n");

    TEST("Array<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *array_type = make_generic_type("Array", "Int");
        Type *resolved = resolve_type_node(array_type, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Array<Int>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(array_type);
    }

    TEST("Slice<Float>.Length resolves to Int");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_FLOAT };
        Type *slice_type = type_create_constructed(TYPE_SLICE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("view", slice_type, 1, 1));

        ASTNode *length = ast_create_member_access(
            make_identifier("view", 2), "Length");
        Type *resolved = type_check_expression(length, ctx);

        EXPECT(resolved == TYPE_INT && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(length);
    }

    TEST("Array<Int> indexing returns element type");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("values", array_type, 1, 1));

        ASTNode *access = ast_create_array_access(
            make_identifier("values", 2), make_number(0, 2));
        Type *resolved = type_check_expression(access, ctx);

        EXPECT(resolved == TYPE_INT && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(access);
    }
}

static void
test_shared_memory_features(void)
{
    printf("\n[shared_memory]\n");

    TEST("Rc<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *rc_type = make_generic_type("Rc", "Int");
        Type *resolved = resolve_type_node(rc_type, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Rc<Int>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(rc_type);
    }

    TEST("RcClone returns same Rc<T> type");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcClone", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(type_equals(resolved, rc_type) && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RcDowngrade returns Weak<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcDowngrade", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Weak<Int>") == 0
               && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("AllocatorPool returns Allocator type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *call = make_call("AllocatorPool", (ASTNode *[]){ make_number(1024, 1) }, 1, 1);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_ALLOCATOR && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Box<Array<Int>> let declaration accepts BoxArray initializer");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *call = make_call("BoxArray", (ASTNode *[]){ make_number(64, 1) }, 1, 1);
        ASTNode *decl = ast_create_let_declaration("storage");
        decl->data.let_decl.type = boxed_array;
        decl->data.let_decl.initializer = call;

        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }
}

/* -----------------------------------------------------------------
 * Async system tests
 * ----------------------------------------------------------------- */

static void
test_async_system(void)
{
    printf("\n[async_system]\n");

    TEST("actor declaration registers SYMBOL_ACTOR");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *actor = ast_create_actor("Counter");
        actor->line = 1; actor->column = 1;
        type_check_actor_decl(actor, ctx);
        EXPECT(!ctx->has_error);

        /* Verify symbol was registered */
        Symbol *sym = scope_lookup(ctx->scope, "Counter");
        EXPECT(sym != NULL && sym->kind == SYMBOL_ACTOR);

        semantic_context_destroy(ctx);
        ast_destroy(actor);
    }

    TEST("duplicate actor declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *a1 = ast_create_actor("Counter");
        a1->line = 1; a1->column = 1;
        type_check_actor_decl(a1, ctx);

        ASTNode *a2 = ast_create_actor("Counter");
        a2->line = 3; a2->column = 1;
        type_check_actor_decl(a2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(a1);
        ast_destroy(a2);
    }

    TEST("await outside async context triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = false;

        ASTNode *num = make_number(42, 1);
        ASTNode *await = ast_create_await_expression(num);
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("await inside async context passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending", 1));
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("select statement with empty cases passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sel = ast_create_select_statement();
        sel->line = 1; sel->column = 1;

        type_check_select_stmt(sel, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sel);
    }

    TEST("spawn expression returns Future<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *num = make_number(42, 1);
        ASTNode *spawn = ast_create_spawn_expression(num);
        spawn->line = 1; spawn->column = 1;

        Type *t = type_check_spawn_expr(spawn, ctx);
        EXPECT(t != NULL);
        EXPECT(t->kind == TYPE_KIND_CONSTRUCTED);
        EXPECT(type_equals(t->data.constructed.constructor, TYPE_FUTURE));
        EXPECT(type_equals(t->data.constructed.args[0], TYPE_INT));
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(spawn);
    }

    TEST("channel send accepts plain value payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_number(42, 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel send rejects anchored Slot handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_slot("slot", slot_type, false, NULL, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("slot", 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel send moves QubitSlot from named binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 2), make_identifier("q", 2));
        send->line = 2; send->column = 1;
        type_check_expression(send, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 3) };
        ASTNode *state = make_call("QubitState", state_args, 1, 3);
        type_check_expression(state, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(send);
        ast_destroy(state);
    }

    TEST("channel send rejects anonymous movable resource payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_call("ClaimQubit", NULL, 0, 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bind the value first"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */

int
main(void)
{
    printf("=== Pergyra Semantic Analyzer Test Suite ===\n");

    type_system_init();

    test_type_system();
    test_symbol_table();
    test_type_checker_slot_rules();
    test_undefined_symbol();
    test_while_loop();
    test_arrays_and_enums();
    test_stdlib_and_io();
    test_qubit_slot_semantics();
    test_quantum_extensions();
    test_match_stmt();
    test_ability_decl();
    test_role_decl();
    test_party_decl();
    test_systemic_world_decl();
    test_extern_block();
    test_engine_collections();
    test_shared_memory_features();
    test_async_system();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
