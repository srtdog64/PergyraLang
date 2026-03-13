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
    n->data.string.value = (char *)s;
    return n;
}

static ASTNode *
make_identifier(const char *name, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_IDENTIFIER;
    n->line = line;
    n->data.identifier.name = (char *)name;
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
    n->data.call.arguments  = args;
    n->data.call.arg_count  = arg_count;
    return n;
}

static ASTNode *
make_program(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_PROGRAM;
    n->data.program.statements = stmts;
    n->data.program.count      = count;
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

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
