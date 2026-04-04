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
#include "lexer/lexer.h"
#include "parser/parser.h"
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

static FuncParam *
make_func_param(const char *name, ASTNode *type)
{
    FuncParam *param = calloc(1, sizeof(FuncParam));
    param->name = pergyra_strdup(name);
    param->type = type;
    return param;
}

static StructuredComment *
make_effect_doc_comment(const char *content)
{
    StructuredComment *comment = calloc(1, sizeof(StructuredComment));
    comment->tag_count = 1;
    comment->tags = calloc(1, sizeof(DocTag *));
    comment->tags[0] = calloc(1, sizeof(DocTag));
    comment->tags[0]->type = DOC_TAG_EFFECTS;
    comment->tags[0]->content = pergyra_strdup(content);
    return comment;
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

static bool
ctx_has_diagnostic_substring_from_result(const SemanticResult *result,
                                         const char *needle)
{
    if (result == NULL || needle == NULL)
        return false;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *diag = result->diagnostics[i];
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

    TEST("ReadView<T> reads but does not own");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("rv", 2) };
        ASTNode *read_call = make_call("Read", read_args, 1, 2);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(read_call);
    }

    TEST("Write through ReadView<T> → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *write_args[2] = { make_identifier("rv", 2), make_number(42, 2) };
        ASTNode *write_call = make_call("Write", write_args, 2, 2);
        type_check_write_slot(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Cannot write through ReadView"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(write_call);
    }

    TEST("WriteView<T> writes but cannot be read");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("wv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("WriteView", "Int");
        decl->data.let_decl.initializer = make_call("ViewWrite", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *write_args[2] = { make_identifier("wv", 2), make_number(7, 2) };
        ASTNode *write_call = make_call("Write", write_args, 2, 2);
        type_check_write_slot(write_call, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("wv", 3) };
        ASTNode *read_call = make_call("Read", read_args, 1, 3);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(ctx->has_error
            && t == TYPE_UNKNOWN
            && ctx_has_diagnostic_substring(ctx, "Cannot read through WriteView"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(write_call);
        ast_destroy(read_call);
    }

    TEST("Release(ReadView<T>) → error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *release_args[1] = { make_identifier("rv", 2) };
        ASTNode *release_call = make_call("Release", release_args, 1, 2);
        type_check_release_slot(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "views are non-owning"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_call);
    }

    TEST("ReadView on SecureSlot<T> reads with implicit capability");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "ss_token", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("srv");
        ASTNode *view_args[1] = { make_identifier("ss", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("srv", 2) };
        ASTNode *read_call = make_call("Read", read_args, 1, 2);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(read_call);
    }

    TEST("MoveToken<T> materializes into a new owning Slot<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *move_decl = ast_create_let_declaration("mt");
        ASTNode *move_args[1] = { make_identifier("s", 1) };
        move_decl->data.let_decl.type = make_generic_type("MoveToken", "Int");
        move_decl->data.let_decl.initializer = make_call("Move", move_args, 1, 1);
        type_check_let_decl(move_decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *slot_decl = ast_create_let_declaration("dst");
        slot_decl->data.let_decl.type = make_generic_type("Slot", "Int");
        slot_decl->data.let_decl.initializer = make_identifier("mt", 2);
        type_check_let_decl(slot_decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_src_args[1] = { make_identifier("s", 3) };
        ASTNode *read_src = make_call("Read", read_src_args, 1, 3);
        type_check_read_slot(read_src, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released slot"));

        semantic_context_destroy(ctx);
        ast_destroy(move_decl);
        ast_destroy(slot_decl);
        ast_destroy(read_src);
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

    TEST("ClaimDeviceSlot infers DeviceSlot<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *t = type_check_expression(make_call("ClaimDeviceSlot", NULL, 0, 1), ctx);
        EXPECT(!ctx->has_error
            && t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.constructor == TYPE_DEVICE_SLOT
            && t->data.constructed.arg_count == 1
            && type_equals(t->data.constructed.args[0], TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("DeviceRead returns inner type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("dev", 1) };
        Type *t = type_check_expression(make_call("DeviceRead", call_args, 1, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("SubmitDeviceRead returns RemoteFuture<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("dev", 1) };
        Type *t = type_check_expression(make_call("SubmitDeviceRead", call_args, 1, 1), ctx);
        EXPECT(!ctx->has_error
            && t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.constructor == TYPE_REMOTE_FUTURE
            && t->data.constructed.arg_count == 1
            && type_equals(t->data.constructed.args[0], TYPE_INT));
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

    TEST("Slot<Int> parameter types remain rejected");
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

    TEST("Slot<subject> parameter types are accepted with ref qualifier");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> parameter types are accepted with own qualifier");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("Option<T> match destructuring binds case variable");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *opt_type = type_create_constructed(TYPE_OPTION, args, 1);
        scope_declare(ctx->scope, symbol_create_variable("opt", opt_type, 1, 1));

        ASTNode *match = ast_create_match_statement();
        match->data.match_stmt.subject = ast_create_identifier("opt");

        ASTNode *some_case = ast_create_match_case();
        ASTNode *some_args[1] = { ast_create_identifier("value") };
        some_case->data.match_case.pattern = make_call("Some", some_args, 1, 2);
        some_case->data.match_case.body = ast_create_block();
        ASTNode *log_args[1] = { ast_create_identifier("value") };
        ast_add_statement(some_case->data.match_case.body,
            make_call("Log", log_args, 1, 3));

        ASTNode *none_case = ast_create_match_case();
        none_case->data.match_case.pattern = make_call("None", NULL, 0, 4);
        none_case->data.match_case.body = ast_create_block();

        match->data.match_stmt.cases = calloc(2, sizeof(ASTNode *));
        match->data.match_stmt.cases[0] = some_case;
        match->data.match_stmt.cases[1] = none_case;
        match->data.match_stmt.case_count = 2;

        type_check_match_stmt(match, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(match);
    }

    TEST("Result<T> match destructuring binds Ok/Err variables");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *res_type = type_create_constructed(TYPE_RESULT, args, 1);
        scope_declare(ctx->scope, symbol_create_variable("result", res_type, 1, 1));

        ASTNode *match = ast_create_match_statement();
        match->data.match_stmt.subject = ast_create_identifier("result");

        ASTNode *ok_case = ast_create_match_case();
        ASTNode *ok_args[1] = { ast_create_identifier("value") };
        ok_case->data.match_case.pattern = make_call("Ok", ok_args, 1, 2);
        ok_case->data.match_case.body = ast_create_block();
        ASTNode *log_ok_args[1] = { ast_create_identifier("value") };
        ast_add_statement(ok_case->data.match_case.body,
            make_call("Log", log_ok_args, 1, 3));

        ASTNode *err_case = ast_create_match_case();
        ASTNode *err_args[1] = { ast_create_identifier("error") };
        err_case->data.match_case.pattern = make_call("Err", err_args, 1, 4);
        err_case->data.match_case.body = ast_create_block();
        ASTNode *log_err_args[1] = { ast_create_identifier("error") };
        ast_add_statement(err_case->data.match_case.body,
            make_call("Log", log_err_args, 1, 5));

        match->data.match_stmt.cases = calloc(2, sizeof(ASTNode *));
        match->data.match_stmt.cases[0] = ok_case;
        match->data.match_stmt.cases[1] = err_case;
        match->data.match_stmt.case_count = 2;

        type_check_match_stmt(match, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(match);
    }

    TEST("enum payload match destructuring binds case variables");
    {
        const char *source =
            "enum Shape {\n"
            "    Circle(Int),\n"
            "    Rect(Int, Int),\n"
            "    None\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let shape: Shape = Circle(7);\n"
            "    match shape {\n"
            "        case .Circle(r):\n"
            "            Log(r);\n"
            "        case .Rect(w, h):\n"
            "            Log(w);\n"
            "            Log(h);\n"
            "        case .None:\n"
            "            Log(0);\n"
            "    }\n"
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

    TEST("Option<T> match without None is non-exhaustive");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let opt: Option<Int> = Some(1);\n"
            "    match opt {\n"
            "        case .Some(v):\n"
            "            Log(v);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Non-exhaustive match"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing cases: None"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("guarded variant does not satisfy exhaustiveness");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let result: Result<Int> = Ok(3);\n"
            "    match result {\n"
            "        case .Ok(v) if v > 0:\n"
            "            Log(v);\n"
            "        case .Err(e):\n"
            "            Log(e);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing cases: Ok"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("duplicate variant case produces warning");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let opt: Option<Int> = Some(1);\n"
            "    match opt {\n"
            "        case .Some(v):\n"
            "            Log(v);\n"
            "        case .Some(x):\n"
            "            Log(x);\n"
            "        case .None:\n"
            "            Log(0);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Redundant match case for 'Some'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("redundant default after full variant coverage produces warning");
    {
        const char *source =
            "enum Color { Red, Green }\n"
            "func Main() -> Void {\n"
            "    let c: Color = Red;\n"
            "    match c {\n"
            "        case .Red:\n"
            "            Log(1);\n"
            "        case .Green:\n"
            "            Log(2);\n"
            "        default:\n"
            "            Log(3);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Redundant default case"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("role bound to non-subject declaration is rejected");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "}\n"
            "role ValueRole for Vec2 {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must be bound to a subject or primitive domain"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("valid party with subject-backed role slot and shared field passes");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Damageable { func Hit() -> Void; }\n"
            "role Tank for Player {\n"
            "    impl ability Damageable { func Hit() -> Void { Log(1); } }\n"
            "}\n"
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "    shared round: Int = 1\n"
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

    TEST("party role slot rejects abilities without subject-bound role impl");
    {
        const char *source =
            "ability Damageable { func Hit() -> Void; }\n"
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "no subject-bound role implements it"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("BoxGet returns inner class object type");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = Box(Vec2(3, 7));\n"
            "    let value: Vec2 = BoxGet(handle);\n"
            "    Log(value.x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("BoxSet requires assignable inner value");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxSet",
            (ASTNode *[]){ make_identifier("boxed", 2), make_number(42, 2) }, 2, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_VOID && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("BoxIsValid returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_STRING };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxIsValid",
            (ASTNode *[]){ make_identifier("boxed", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_BOOL && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Box<class> can be returned and passed explicitly");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func SumX(cell: Box<Vec2>) -> Int {\n"
            "    return BoxGet(cell).x;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(SumX(handle));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    printf("\n[domain_layer]\n");
    fflush(stdout);

    TEST("relation/effect/zone declarations pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *relation = ast_create_relation_declaration("TrustedLink");
        relation->line = 1; relation->column = 1;
        ASTNode *effect = ast_create_effect_declaration("Poisoned");
        effect->line = 2; effect->column = 1;
        ASTNode *zone = ast_create_zone_declaration("DungeonZone");
        zone->line = 3; zone->column = 1;

        type_check_relation_decl(relation, ctx);
        type_check_effect_decl(effect, ctx);
        type_check_zone_decl(zone, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(relation);
        ast_destroy(effect);
        ast_destroy(zone);
    }

    TEST("relation/effect/zone/world minimal composition parses and subject slots require subject type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    shared trust: Int = 100\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    shared stacks: Int = 1\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    object slot playerView: PlayerView\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    apply poison to player\n"
            "    link trust between player, enemy\n"
            "    detach poison from enemy\n"
            "    unlink trust between player, enemy\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
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

    TEST("relation/effect warn when no subject endpoint or target is declared");
    {
        const char *source =
            "relation LooseLink {\n"
            "    shared trust: Int = 100\n"
            "}\n"
            "effect AmbientFog {\n"
            "    shared density: Int = 1\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone/world references warn when relation/effect/zone types are missing");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    relation slot trust: MissingRelation\n"
            "    effect slot poison: MissingEffect\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: MissingZone\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone object/dto slot initializer can project from subject slot without direct-projection warning");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "dto PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView = ToObject(PlayerView, player)\n"
            "    dto slot snapshot: PlayerDto = ToDto(PlayerDto, player)\n"
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

    TEST("zone refresh projects object slot from subject slot");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player\n"
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

    TEST("zone dto slot and publish project dto from subject slot");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "dto PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    dto slot snapshot: PlayerDto = ToDto(PlayerDto, player)\n"
            "    publish snapshot from player\n"
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

    TEST("zone publish requires dto slot target");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot view: PlayerView\n"
            "    publish view from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh rejects wrong slot kinds");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh player from player\n"
            "    refresh playerView from playerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone slot initializer enforces declared slot type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot bad: Int = ToObject(PlayerView, player)\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain validates relation/effect contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle statements can reference declared state aliases");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poisoned by player\n"
            "    link allied by player\n"
            "    detach poisoned by player\n"
            "    unlink allied by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle state aliases reject wrong verb kind");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply allied by player\n"
            "    link poisoned by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority requires declared subject slots and explicit by actors");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    authority playerView\n"
            "    refresh playerView from player by playerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority can require abilities implemented by subject roles");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Damageable { func Hit() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "    impl ability Damageable { func Hit() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Damageable\n"
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

    TEST("zone authority ability requirements reject missing role impls");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Healable { func Heal() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Healable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone warns when authority exists but mutable rules omit by actor");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    apply poison to player\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain warns on duplicate and conflicting lifecycle rules");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain poison on player by player\n"
            "    detach poison from player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain trust between player, enemy by player\n"
            "    unlink trust between player, enemy by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply rejects unknown effect or target slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    apply missing to player\n"
            "    apply poison to missingTarget\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link rejects unknown relation or endpoint slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    link missing between player, enemy\n"
            "    link trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone detach and unlink reject unknown slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    detach missing from player\n"
            "    detach poison from missingEnemy\n"
            "    unlink missing between player, enemy\n"
            "    unlink trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link enforces relation endpoint types and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Monster\n"
            "    relation slot trust: TrustedLink\n"
            "    relation slot crowd: CrowdLink\n"
            "    link trust between player, enemy\n"
            "    link crowd between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "relation CrowdLink for a: Player, b: Player, c: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply enforces effect target type and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot curse: Cursed\n"
            "    effect slot split: SplitMind\n"
            "    apply curse to player\n"
            "    apply split to player\n"
            "}\n"
            "effect Cursed for bearer: Monster { }\n"
            "effect SplitMind for a: Player, b: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone warns when subject count exceeds recommended shape");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BusyZone {\n"
            "    subject slot a: Player\n"
            "    subject slot b: Player\n"
            "    subject slot c: Player\n"
            "    subject slot d: Player\n"
            "    subject slot e: Player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect/zone reject non-subject type in subject slot");
    {
        const char *source =
            "struct PlayerView { hp: Int; }\n"
            "relation BrokenLink {\n"
            "    subject slot source: PlayerView\n"
            "}\n"
            "effect BrokenEffect {\n"
            "    subject slot bearer: PlayerView\n"
            "}\n"
            "zone BrokenZone {\n"
            "    subject slot player: PlayerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect can refresh and publish projection slots from subject slots");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "dto PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    dto slot packet: PlayerDto\n"
            "    refresh snapshot from source\n"
            "    publish packet from target\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    dto slot packet: PlayerDto\n"
            "    refresh view from bearer\n"
            "    publish packet from bearer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect constructors are type-checked as nominal overlays");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(Player(7, \"src\"), Player(9, \"dst\"));\n"
            "    let poison = Poisoned(Player(5, \"bear\"));\n"
            "    Log(1);\n"
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

    TEST("relation/effect constructors reject mismatched positional field types");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(1, Player(9, \"dst\"));\n"
            "    let poison = Poisoned(2);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }


    TEST("relation/effect/zone reject legacy class in subject slot");
    {
        const char *source =
            "class PassivePlayer { let hp: Int; }\n"
            "relation BrokenLink {\n"
            "    subject slot source: PassivePlayer\n"
            "}\n"
            "effect BrokenEffect {\n"
            "    subject slot bearer: PassivePlayer\n"
            "}\n"
            "zone BrokenZone {\n"
            "    subject slot player: PassivePlayer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect/zone accept actor types in subject slot");
    {
        const char *source =
            "actor Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "relation ActiveLink {\n"
            "    subject slot source: Bot\n"
            "}\n"
            "effect ActiveEffect {\n"
            "    subject slot bearer: Bot\n"
            "}\n"
            "zone ActiveZone {\n"
            "    subject slot player: Bot\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("role can bind to actor as subject profile");
    {
        const char *source =
            "actor Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "ability Tickable { func Tick() -> Void; }\n"
            "role CounterRole for Counter {\n"
            "    impl ability Tickable { func Tick() -> Void { Log(1); } }\n"
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

    TEST("await rejects Future<Slot<T>> anchored handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_slot", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_slot", 1));
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("await on RemoteFuture<QubitSlot> yields Result<QubitSlot>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_qubit", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_qubit", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_QUBIT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_await_expression(make_identifier("pending_qubit", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }

    TEST("await movable resource rejects inline use without binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_qubit", 1));
        await->line = 1; await->column = 1;
        ASTNode *state_args[1] = { await };
        ASTNode *state = make_call("QubitState", state_args, 1, 1);

        type_check_expression(state, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bound to a named variable"));

        semantic_context_destroy(ctx);
        ast_destroy(state);
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

    TEST("select statement rejects non-channel case");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sel = ast_create_select_statement();
        ASTNode *bad = ast_create_block();
        ast_add_statement(bad, make_call("Log", NULL, 0, 1));
        sel->data.select_stmt.case_count = 1;
        sel->data.select_stmt.cases = calloc(1, sizeof(ASTNode *));
        sel->data.select_stmt.cases[0] = bad;

        type_check_select_stmt(sel, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "select case must begin"));

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

    TEST("Cancel(Future<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects non-future values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "Cancel requires Future<T> or RemoteFuture<T>"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsCancelled returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call = make_call("IsCancelled", NULL, 0, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
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

    TEST("TryRecv(Channel<Int>) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout(Channel<Int>, Int) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(1000, 1) };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelCapacity(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelCapacity", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelSpace(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelSpace", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelFull(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelFull", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClosed(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelClosed", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySendStatus(Channel<Int>, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(7, 1) };
        ASTNode *call = make_call("TrySendStatus", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("SendTimeoutStatus(Channel<Int>, Int, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[3] = {
            make_identifier("ch", 1),
            make_number(7, 1),
            make_number(1000, 1)
        };
        ASTNode *call = make_call("SendTimeoutStatus", call_args, 3, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength rejects non-channel values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend(Channel<Int>, Int) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(42, 1) };
        ASTNode *call = make_call("TrySend", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend rejects movable resource channel payloads");
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

        ASTNode *call_args[2] = { make_identifier("ch", 2), make_identifier("q", 2) };
        ASTNode *call = make_call("TrySend", call_args, 2, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "TrySend does not support movable resource sends yet"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(call);
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

    TEST("channel recv may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_channel_recv(make_identifier("ch", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }
}

static void
test_effect_inference(void)
{
    printf("\n[effect_inference]\n");

    TEST("Measure infers nondeterministic + collapse on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Observe");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        ret->data.return_stmt.value = make_call("Measure", measure_args, 1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Observe");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("effectful calls propagate function effects to callers");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *observe = ast_create_function("Observe");
        observe->data.func_decl.return_type = ast_create_type("Int");
        observe->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(observe->data.func_decl.body, decl);

        ASTNode *measure_ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        measure_ret->data.return_stmt.value =
            make_call("Measure", measure_args, 1, 3);
        ast_add_statement(observe->data.func_decl.body, measure_ret);
        type_check_func_decl(observe, ctx);

        ASTNode *wrapper = ast_create_function("WrapObserve");
        wrapper->data.func_decl.return_type = ast_create_type("Int");
        wrapper->data.func_decl.body = ast_create_block();

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_call("Observe", NULL, 0, 6);
        ast_add_statement(wrapper->data.func_decl.body, ret);
        type_check_func_decl(wrapper, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "WrapObserve");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(observe);
        ast_destroy(wrapper);
    }

    TEST("SecureSlot declarations infer secure effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchSecure");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("secret");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_number(7, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchSecure");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot parameter types are rejected as anchored handles");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseDevice");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("dev", make_generic_type("DeviceSlot", "Int"));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored resource handle parameters"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot copy into new binding is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *dev_args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, dev_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("dev", 2);

        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("parallel slot analyzer rejects write-write conflict");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            Write(s, 1);\n"
            "            Write(s, 2);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);

        bool found = false;
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message, "Parallel slot conflict") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel slot analyzer warns on read-write race risk");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            let a = Read(s);\n"
            "            Write(s, 2);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        bool found = false;
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message, "Parallel slot race risk") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("DeviceSlot use-after-release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_args[1] = { make_identifier("dev", 2) };
        ASTNode *read_args[1] = { make_identifier("dev", 3) };
        ASTNode *release = make_call("ReleaseDeviceSlot", release_args, 1, 2);
        ASTNode *read = make_call("DeviceRead", read_args, 1, 3);

        type_check_expression(release, ctx);
        ctx->has_error = false;
        type_check_expression(read, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released DeviceSlot"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release);
        ast_destroy(read);
    }

    TEST("DeviceSlot double release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_a_args[1] = { make_identifier("dev", 2) };
        ASTNode *release_b_args[1] = { make_identifier("dev", 3) };
        ASTNode *release_a = make_call("ReleaseDeviceSlot", release_a_args, 1, 2);
        ASTNode *release_b = make_call("ReleaseDeviceSlot", release_b_args, 1, 3);

        type_check_expression(release_a, ctx);
        ctx->has_error = false;
        type_check_expression(release_b, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "already been released"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_a);
        ast_destroy(release_b);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("struct value copy into new binding is allowed");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("class value copy into new binding is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("standalone subject parameter by value is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Use(v: Vec2) -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subject parameters are not supported as plain value parameters yet"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class return by value is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
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

    TEST("subject return by value is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Returning subjects by value is not supported yet"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("actor copy into new binding is rejected as subject profile");
    {
        const char *source =
            "actor Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject actor profile syntax is rejected by subject copy rules");
    {
        const char *source =
            "subject Counter actor {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field names resolve in class scope");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor positional arguments are type-checked");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, \"bad\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "argument 2 initializes field 'y'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor rejects too many positional arguments");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(1, 2, 3);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "accepts at most 2 positional field argument"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> is accepted as a local object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<actor> is accepted as a local object-cell anchor");
    {
        const char *source =
            "actor Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9));\n"
            "    Release(s);\n"
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

    TEST("SecureSlot<subject> is accepted as a secure object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<actor> is accepted as a secure object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("spawn and channel send infer remote effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Dispatch");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("ch", make_generic_type("Channel", "Int"));

        ASTNode *pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 2));
        ast_add_statement(func->data.func_decl.body, pending);

        ASTNode *send = ast_create_channel_send(make_identifier("ch", 3),
                                                make_number(7, 3));
        ast_add_statement(func->data.func_decl.body, send);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 4);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Dispatch");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("structured comment @effects merges declared effect into function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("PlanRemote");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.doc_comment =
            make_effect_doc_comment("remote secure");

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("source-level /// @effects flows from parser into semantic effects");
    {
        const char *source =
            "/// @effects remote secure\n"
            "func PlanRemote() -> Int {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("source-level with effects clause flows from parser into semantic effects");
    {
        const char *source =
            "func PlanRemote() -> Int with effects remote, secure {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("declared effects must cover inferred body effects");
    {
        const char *source =
            "/// @effects local\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects must cover inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects local {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects may exactly match inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects remote {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

    TEST("declared effects may exactly match inferred body effects");
    {
        const char *source =
            "/// @effects remote\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

static void
test_misc_grammar_edges(void)
{
    printf("\n[misc_grammar]\n");

    TEST("unsafe block type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        unsafe_block->line = 1; unsafe_block->column = 1;

        type_check_statement(unsafe_block, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(unsafe_block);
    }

    TEST("defer statement type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        defer_stmt->line = 1; defer_stmt->column = 1;

        type_check_statement(defer_stmt, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(defer_stmt);
    }

    TEST("bind statement is accepted by semantic pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        bind->line = 1; bind->column = 1;

        type_check_statement(bind, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(bind);
    }

    TEST("else if chain type-checks nested branch structure");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_args[1] = { make_number(1, 1) };
        ast_add_statement(then_block, make_call("Log", then_args, 1, 1));

        ASTNode *else_then = ast_create_block();
        ASTNode *else_then_args[1] = { make_number(2, 1) };
        ast_add_statement(else_then, make_call("Log", else_then_args, 1, 1));

        ASTNode *else_final = ast_create_block();
        ASTNode *else_final_args[1] = { make_number(3, 1) };
        ast_add_statement(else_final, make_call("Log", else_final_args, 1, 1));

        ASTNode *nested_if = ast_create_if_statement();
        nested_if->data.if_stmt.condition = ast_create_boolean(false);
        nested_if->data.if_stmt.then_branch = else_then;
        nested_if->data.if_stmt.else_branch = else_final;

        ASTNode *outer_if = ast_create_if_statement();
        outer_if->data.if_stmt.condition = ast_create_boolean(true);
        outer_if->data.if_stmt.then_branch = then_block;
        outer_if->data.if_stmt.else_branch = nested_if;
        outer_if->line = 1; outer_if->column = 1;

        type_check_statement(outer_if, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer_if);
    }

    /* ---- RemoteFuture design: await returns Result, Claim/Read/Write/Release rejected ---- */

    TEST("await on local Future<Int> returns Int (not Result)");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("local_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("local_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await on RemoteFuture<Int> returns Result<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("Read on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *read_args[1] = { make_identifier("rf", 1) };
        ASTNode *read_call = make_call("Read", read_args, 1, 1);
        type_check_expression(read_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Read"));

        semantic_context_destroy(ctx);
        ast_destroy(read_call);
    }

    TEST("Write on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *write_args[2] = { make_identifier("rf", 1), make_number(42, 1) };
        ASTNode *write_call = make_call("Write", write_args, 2, 1);
        type_check_expression(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Write"));

        semantic_context_destroy(ctx);
        ast_destroy(write_call);
    }

    TEST("Release on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *release_args[1] = { make_identifier("rf", 1) };
        ASTNode *release_call = make_call("Release", release_args, 1, 1);
        type_check_expression(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Release"));

        semantic_context_destroy(ctx);
        ast_destroy(release_call);
    }

    TEST("Option<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *opt = make_generic_type("Option", "Int");
        Type *t = resolve_type_node(opt, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(opt);
    }

    TEST("Some(42) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Some", args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsSome requires Option<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IsSome", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "IsSome requires Option<T>"));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ToDto returns dto projection type but warns outside domain context");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToDto accepts actor source as subject profile");
    {
        const char *source =
            "dto BotDto { hp: Int; }\n"
            "actor Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bot: Bot = Bot();\n"
            "    let snapshot: BotDto = ToDto(BotDto, bot);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToDto rejects missing fields, non-dto targets, and unnamed sources");
    {
        const char *source =
            "dto PlayerDto { hp: Int; name: String; }\n"
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let missing: PlayerDto = ToDto(PlayerDto, player);\n"
            "    let wrong = ToDto(Player, player);\n"
            "    let wrongView = ToDto(PlayerView, player);\n"
            "    let anon = ToDto(PlayerDto, Player());\n"
            "    let legacy = ToDto(PlayerDto, passive);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 5);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject returns object projection type but warns outside domain context");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("object and dto declarations reject methods because they are passive projection forms");
    {
        const char *source =
            "object PlayerView {\n"
            "    hp: Int;\n"
            "    func Mutate() -> Void {\n"
            "        Log(hp);\n"
            "    }\n"
            "}\n"
            "dto PlayerDto {\n"
            "    hp: Int;\n"
            "    func Export() -> Int {\n"
            "        return hp;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject rejects non-object targets and non-subject sources");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "dto PlayerDto { hp: Int; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let wrongDto = ToObject(PlayerDto, player);\n"
            "    let wrongSubject = ToObject(Player, player);\n"
            "    let legacy = ToObject(PlayerView, passive);\n"
            "    let anon = ToObject(PlayerView, Player());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState works inside zone methods for declared states");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        if HasState(poisoned) || HasState(\"allied\") || HasState(poisoned, player) || HasState(allied, player, enemy) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasState validates state slot arity and endpoint matching");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        let a = HasState(poisoned, enemy);\n"
            "        let b = HasState(allied, player);\n"
            "        let c = HasState(allied, enemy, player);\n"
            "        Log(a || b || c);\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState rejects unknown states and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    state poisoned: effect poison on player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasState(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let active = HasState(\"poisoned\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZone works inside world methods for declared zone states");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain battle\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(liveBattle) || HasZone(\"battle\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasZone rejects unknown names and use outside world");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        let active = HasZone(missing);\n"
            "        Log(active);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasZone(\"battle\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    /* ---- Generic class semantic tests ---- */

    TEST("generic class declaration passes semantic check");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
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

    TEST("generic class with method passes semantic check");
    {
        const char *source =
            "class Wrapper<T> {\n"
            "    let data: T;\n"
            "    func GetData(self) -> T {\n"
            "        return self.data;\n"
            "    }\n"
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

    TEST("generic class with concrete usage passes semantic check");
    {
        const char *source =
            "class Pair<T> {\n"
            "    let first: T;\n"
            "    let second: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Pair<Int> = Pair(3, 7);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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
    test_effect_inference();
    test_misc_grammar_edges();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
