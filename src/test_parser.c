/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser test driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "semantic/type_system.h"

typedef struct
{
    const char *name;
    const char *code;
    int expect_success;
} TestCase;

static int
run_parser_test(const TestCase *test)
{
    int failed = 0;
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;

    printf("\n=== Test: %s ===\n", test->name);
    printf("Code:\n%s\n", test->code);
    printf("---\n");

    lexer = lexer_create(test->code);
    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("Parse error: %s\n", parser_get_error(parser));
        if (test->expect_success) {
            printf("[FAIL] unexpected parse error\n");
            failed = 1;
        }
    } else {
        printf("Parsing successful!\n\n");
        printf("AST:\n");
        ast_print(ast, 0);
        if (!test->expect_success) {
            printf("[FAIL] expected parse failure but succeeded\n");
            failed = 1;
        }
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_doc_comment_attachment_test(void)
{
    const char *code =
        "/// @effects remote secure\n"
        "func RemoteOp() -> Void {\n"
        "    Log(1);\n"
        "}\n"
        "class Worker {\n"
        "    /// [Effects]: secure\n"
        "    func Run() -> Void {\n"
        "        Log(2);\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *func = NULL;
    ASTNode *klass = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Structured Comment Attachment ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 2) {
        printf("[FAIL] Expected program with at least two declarations\n");
        failed = 1;
        goto cleanup;
    }

    func = ast->data.program.statements[0];
    klass = ast->data.program.statements[1];
    if (func == NULL || func->type != AST_FUNC_DECL ||
        func->data.func_decl.doc_comment == NULL ||
        func->data.func_decl.doc_comment->tag_count != 1 ||
        func->data.func_decl.doc_comment->tags[0]->type != DOC_TAG_EFFECTS ||
        strcmp(func->data.func_decl.doc_comment->tags[0]->content, "remote secure") != 0) {
        printf("[FAIL] Top-level function doc comment was not attached correctly\n");
        failed = 1;
        goto cleanup;
    }

    if (klass == NULL || klass->type != AST_CLASS_DECL || klass->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected class with one method\n");
        failed = 1;
        goto cleanup;
    }

    method = klass->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL ||
        method->data.func_decl.doc_comment == NULL ||
        method->data.func_decl.doc_comment->tag_count != 1 ||
        method->data.func_decl.doc_comment->tags[0]->type != DOC_TAG_EFFECTS ||
        strcmp(method->data.func_decl.doc_comment->tags[0]->content, "secure") != 0) {
        printf("[FAIL] Class method doc comment was not attached correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Structured comments attached successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_signature_effect_clause_test(void)
{
    const char *code =
        "func RemoteOp() -> Void with effects remote, secure {\n"
        "    Log(1);\n"
        "}\n"
        "async func Tick() -> Int with effects local {\n"
        "    return 2;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *func = NULL;
    ASTNode *async_func = NULL;

    printf("\n=== Test: Signature Effect Clause ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 2) {
        printf("[FAIL] Expected program with two declarations\n");
        failed = 1;
        goto cleanup;
    }

    func = ast->data.program.statements[0];
    async_func = ast->data.program.statements[1];
    if (func == NULL || func->type != AST_FUNC_DECL
        || !func->data.func_decl.has_effects_clause
        || func->data.func_decl.declared_effects != (EFFECT_REMOTE | EFFECT_SECURE)) {
        printf("[FAIL] Regular function signature effects were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    if (async_func == NULL || async_func->type != AST_FUNC_DECL
        || !async_func->data.func_decl.has_effects_clause
        || async_func->data.func_decl.declared_effects != EFFECT_NONE
        || !async_func->is_async_decl) {
        printf("[FAIL] Async function signature effects were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Signature effect clauses parsed successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_subject_keyword_alias_test(void)
{
    const char *code =
        "subject Player {\n"
        "    let hp: Int;\n"
        "    action TakeDamage(amount: Int) -> Void\n"
        "        requires Damageable\n"
        "        within BattleZone\n"
        "        causes DamageEffect\n"
        "        authorized by self {\n"
        "        hp = hp - amount;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Subject Keyword Surface ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL) {
        printf("[FAIL] Expected 'subject' to parse as AST_CLASS_DECL\n");
        failed = 1;
        goto cleanup;
    }

    if (decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT
        || strcmp(decl->data.class_decl.name, "Player") != 0
        || decl->data.class_decl.field_count != 1
        || decl->data.class_decl.method_count != 1
        || !decl->data.class_decl.methods[0]->data.func_decl.is_action
        || decl->data.class_decl.methods[0]->data.func_decl.required_ability_count != 1
        || decl->data.class_decl.methods[0]->data.func_decl.within_zone == NULL
        || decl->data.class_decl.methods[0]->data.func_decl.causes_effect == NULL
        || decl->data.class_decl.methods[0]->data.func_decl.authorized_by_count != 1) {
        printf("[FAIL] Subject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Subject keyword parsed successfully as subject declaration!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_action_clause_reordering_test(void)
{
    const char *code =
        "subject Player {\n"
        "    action Attack(self, target: Player) -> Void\n"
        "        authorized by self, target\n"
        "        causes DamageEffect\n"
        "        within BattleZone\n"
        "        requires Combatable, Movable\n"
        "        with effects secure, remote\n"
        "        where T: Combatable {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Action Clause Reordering ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected subject with one action declaration\n");
        failed = 1;
        goto cleanup;
    }

    method = decl->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.where_clause == NULL
        || !method->data.func_decl.has_effects_clause
        || method->data.func_decl.declared_effects != (EFFECT_SECURE | EFFECT_REMOTE)
        || method->data.func_decl.required_ability_count != 2
        || method->data.func_decl.within_zone == NULL
        || strcmp(method->data.func_decl.within_zone, "BattleZone") != 0
        || method->data.func_decl.causes_effect == NULL
        || strcmp(method->data.func_decl.causes_effect, "DamageEffect") != 0
        || method->data.func_decl.authorized_by_count != 2) {
        printf("[FAIL] Reordered action clauses were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Action clauses parsed successfully regardless of order!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_duplicate_action_clause_diagnostic_test(void)
{
    const char *code =
        "subject Player {\n"
        "    action Attack(self) -> Void\n"
        "        requires Combatable\n"
        "        requires Movable {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Duplicate Action Clause Diagnostic ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected duplicate clause parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL || strstr(error, "Duplicate 'requires' clause") == NULL) {
        printf("[FAIL] Expected duplicate clause diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Duplicate action clause reports an explicit diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_vessel_keyword_alias_test(void)
{
    const char *code =
        "vessel HealthState {\n"
        "    current: Int;\n"
        "    max: Int;\n"
        "    func IsDead(self) -> Bool {\n"
        "        return current <= 0;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Vessel Keyword Surface ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
        printf("[FAIL] Expected 'vessel' to parse as vessel declaration\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "HealthState") != 0
        || decl->data.class_decl.field_count != 2
        || decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Vessel declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Vessel keyword parsed successfully as vessel declaration!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_lexical_zone_context_test(void)
{
    const char *code =
        "within BattleZone {\n"
        "    subject Hero {\n"
        "        let hp: Int;\n"
        "        action Guard(self) authorized by self {\n"
        "            return;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "zone BattleZone {\n"
        "    subject slot hero: Hero\n"
        "    authority hero\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *subject_decl = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Lexical Zone Context ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 2) {
        printf("[FAIL] Expected flattened program statements after lexical zone context\n");
        failed = 1;
        goto cleanup;
    }

    subject_decl = ast->data.program.statements[0];
    if (subject_decl == NULL || subject_decl->type != AST_CLASS_DECL
        || subject_decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected subject declaration with one action method\n");
        failed = 1;
        goto cleanup;
    }

    method = subject_decl->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.within_zone == NULL
        || strcmp(method->data.func_decl.within_zone, "BattleZone") != 0) {
        printf("[FAIL] Expected lexical zone context to inject within BattleZone\n");
        failed = 1;
        goto cleanup;
    }

    printf("Lexical zone context injected default within-clause successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_tobject_keyword_test(void)
{
    const char *code =
        "tobject PlayerDto {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: TObject Keyword ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT) {
        printf("[FAIL] Expected 'tobject' to parse as tobject declaration surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerDto") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] tobject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("TObject keyword parsed successfully as tobject declaration surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_tobject_keyword_alias_test(void)
{
    const char *code =
        "tobject PlayerPacket {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: TObject Keyword Surface ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT) {
        printf("[FAIL] Expected 'tobject' to parse as transfer object surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerPacket") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] TObject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("TObject keyword parsed successfully as transfer object surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_object_keyword_alias_test(void)
{
    const char *code =
        "object PlayerView {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Object Keyword Surface ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_OBJECT) {
        printf("[FAIL] Expected 'object' to parse as object declaration surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerView") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] Object declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Object keyword parsed successfully as object declaration surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

int
main(void)
{
    const TestCase tests[] = {
        {
            "Basic Let Declaration",
            "let x = 42;\n"
            "let name = \"Pergyra\";\n"
            "let flag = true;",
            1
        },
        {
            "Function Declaration",
            "func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}",
            1
        },
        {
            "Generic Function",
            "func Identity<T>(value: T) -> T {\n"
            "    return value;\n"
            "}",
            1
        },
        {
            "Nested Generic Type Arguments",
            "func Main() -> Void {\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "}",
            1
        },
        {
            "Function Typed Locals And Returns",
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func MakeAdder() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f: func(Int) -> Int = AddOne;\n"
            "    let g = MakeAdder();\n"
            "    Log(f(4));\n"
            "    Log(g(9));\n"
            "}",
            1
        },
        {
            "Escaped String Literal",
            "func Main() -> Void {\n"
            "    Log(\"{\\\"ok\\\":true}\\n\");\n"
            "}",
            1
        },
        {
            "Async Function With Ref Slot Param",
            "subject WorkerLedger {\n"
            "    let load: Int;\n"
            "}\n"
            "async func Worker(jobs: Channel<Int>, ref ledger: Slot<WorkerLedger>) -> Int {\n"
            "    return 1;\n"
            "}\n",
            1
        },
        {
            "Function with Where Clause",
            "func Sort<T>(items: Array<T>) -> Array<T>\n"
            "    where T: Comparable {\n"
            "    // Implementation\n"
            "    return items;\n"
            "}",
            1
        },
        {
            "Slot Operations",
            "let slot = ClaimSlot<Int>();\n"
            "Write(slot, 42);\n"
            "let value = Read(slot);\n"
            "Release(slot);",
            1
        },
        {
            "With Statement",
            "with slot<String> as s {\n"
            "    s.Write(\"Hello\");\n"
            "    Log(s.Read());\n"
            "}",
            1
        },
        {
            "Secure Slot",
            "with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {\n"
            "    hp.Write(100);\n"
            "}",
            1
        },
        {
            "Parallel Block",
            "let result = parallel {\n"
            "    ProcessA();\n"
            "    ProcessB();\n"
            "    ProcessC();\n"
            "};",
            1
        },
        {
            "For Loop",
            "for i in 1..10 {\n"
            "    Log(i);\n"
            "}",
            1
        },
        {
            "If Statement",
            "if x > 10 {\n"
            "    Log(\"Greater\");\n"
            "} else {\n"
            "    Log(\"Less or equal\");\n"
            "}",
            1
        },
        {
            "Class Declaration",
            "class Player<T> where T: Serializable {\n"
            "    private let _name: String;\n"
            "    public let Health: Int;\n"
            "    public func TakeDamage(amount: Int) {\n"
            "        Health = Health - amount;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Struct Declaration",
            "struct Vec3 {\n"
            "    x: Float;\n"
            "    y: Float;\n"
            "    z: Float;\n"
            "    func Length() -> Float {\n"
            "        return x;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Extern C Block",
            "extern \"C\" {\n"
            "    func SDL_Init(flags: Int) -> Int;\n"
            "    func SDL_Quit();\n"
            "}\n"
            "func Main() -> Int {\n"
            "    return SDL_Init(0);\n"
            "}",
            1
        },
        {
            "Complex Expression",
            "let result = (a + b * c) / (d - e) && flag || !other;",
            1
        },
        {
            "Method Chaining",
            "let result = entity.Method1().Method2(42).Property;",
            1
        },
        {
            "Using Alias Statement",
            "func Main() -> Void {\n"
            "    using entity.Route() as route;\n"
            "    Log(route);\n"
            "}",
            1
        },
        {
            "Array Access",
            "let value = array[index + 1];\n"
            "matrix[i][j] = value * 2;",
            1
        },
        {
            "Array Literal",
            "let values: Array<Int> = [1, 2, 3];\n"
            "Log(values[1]);",
            1
        },
        {
            "While Loop",
            "func Countdown(n: Int) -> Void {\n"
            "    let count: Int = n;\n"
            "    while count > 0 {\n"
            "        Log(count);\n"
            "        count = count - 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Match Statement",
            "func Classify(n: Int) -> Void {\n"
            "    match n {\n"
            "        case 0:\n"
            "            Log(\"zero\");\n"
            "        case 1:\n"
            "            Log(\"one\");\n"
            "        case 2 if n > 0:\n"
            "            Log(\"two positive\");\n"
            "        default:\n"
            "            Log(\"other\");\n"
            "    }\n"
            "}",
            1
        },
        {
            "Leading Dot Variant Shorthand",
            "enum OptionInt { Some(Int), None }\n"
            "func Wrap(n: Int) -> OptionInt {\n"
            "    if n > 0 {\n"
            "        return .Some(n);\n"
            "    }\n"
            "    return .None;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let value: OptionInt = .Some(7);\n"
            "    match value {\n"
            "        case .Some(v):\n"
            "            Log(v);\n"
            "        case .None:\n"
            "            Log(0);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Full Example",
            "// Fibonacci function\n"
            "func Fibonacci(n: Int) -> Int {\n"
            "    if n <= 1 {\n"
            "        return n;\n"
            "    }\n"
            "    with slot<Int> as prev {\n"
            "        prev.Write(0);\n"
            "        with slot<Int> as curr {\n"
            "            curr.Write(1);\n"
            "            for i in 2..n {\n"
            "                let next = prev.Read() + curr.Read();\n"
            "                prev.Write(curr.Read());\n"
            "                curr.Write(next);\n"
            "            }\n"
            "            return curr.Read();\n"
            "        }\n"
            "    }\n"
            "}",
            1
        },
        {
            "Ability Declaration",
            "ability Damageable {\n"
            "    require health: Int\n"
            "    func TakeDamage(amount: Int) -> Void {\n"
            "        Log(amount);\n"
            "    }\n"
            "    func GetHealth() -> Int;\n"
            "}",
            1
        },
        {
            "Role Declaration",
            "role PlayerDamageable for Player {\n"
            "    include role BuffableRole\n"
            "    impl ability Damageable {\n"
            "        func TakeDamage(amount: Int) -> Void {\n"
            "            Log(amount);\n"
            "        }\n"
            "    }\n"
            "    override func GetHealth() -> Int {\n"
            "        return 100;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Party Declaration",
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "    role slot healer: Healing\n"
            "    shared formation: String = \"standard\"\n"
            "    func Execute() -> Void {\n"
            "        Log(formation);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Roster Declaration",
            "roster CombatSystem {\n"
            "    party slot team1: DungeonTeam\n"
            "    party slot team2: DungeonTeam\n"
            "    shared round: Int = 0\n"
            "    func StartRound() -> Void {\n"
            "        Log(round);\n"
            "    }\n"
            "}",
            1
        },
        {
            "World Declaration",
            "world GameWorld {\n"
            "    roster combat: CombatSystem\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain battle\n"
            "    deactivate liveBattle\n"
            "    shared tick: Int = 0\n"
            "    func Update() -> Void {\n"
            "        Log(tick);\n"
            "    }\n"
            "}",
            1
        },
        {
            "World As Local Variable",
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let world = GameWorld();\n"
            "    Log(world);\n"
            "}",
            1
        },
        {
            "World Derived States",
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleReady: zone battle projection playerView\n"
            "    state battleLinked: zone battle layer poison\n"
            "    state battlePoisoned: zone battle state poisoned\n"
            "    state battleVisible: all battleReady, battleLinked\n"
            "    state battleInteresting: any battleVisible, battlePoisoned\n"
            "}",
            1
        },
        {
            "Intent Declaration",
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Payable { func Pay() -> Void; }\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Player, seller: Merchant) {\n"
            "    exclusive;\n"
            "    rollback: current;\n"
            "    priority: 10;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: buyer.pay();\n"
            "        on: buyer.pay();\n"
            "        compensate: buyer.pay();\n"
            "        pre: buyer.hp >= 0;\n"
            "        guard: buyer.hp >= 0;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        causes: PaymentEffect;\n"
            "        post: buyer.hp >= 0;\n"
            "        invariant: buyer.hp >= 0;\n"
            "        expect: buyer.hp >= 0;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}",
            1
        },
        {
            "Intent Step Subintent Clause",
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "intent Charge(buyer: Buyer) {\n"
            "    step verify {\n"
            "        expect: true;\n"
            "    }\n"
            "}\n"
            "intent Checkout(buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "}\n",
            1
        },
        {
            "Limited Domain Keywords As Local Variables",
            "func Main() -> Void {\n"
            "    let zone = 1;\n"
            "    let world = zone;\n"
            "    Log(world);\n"
            "}",
            1
        },
        {
            "Limited Domain Keywords As Parameters",
            "func Main(world: Int, zone: Int, participant: Int) -> Void {\n"
            "    Log(world + zone + participant);\n"
            "}",
            1
        },
        {
            "Reserved Domain Keyword As Local Variable Is Rejected",
            "func Main() -> Void {\n"
            "    let object = 1;\n"
            "}",
            0
        },
        {
            "Reserved Domain Keyword As Parameter Is Rejected",
            "func Main(subject: Int) -> Void {\n"
            "    Log(subject);\n"
            "}",
            0
        },
        {
            "Relation Declaration",
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    tobject slot packet: LinkDto\n"
            "    bind snapshot from source\n"
            "    bind packet from target\n"
            "    shared trust: Int = 100\n"
            "    func Refresh() -> Void {\n"
            "        Log(trust);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Effect Declaration",
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    tobject slot packet: StatusDto\n"
            "    bind view from bearer\n"
            "    bind packet from bearer\n"
            "    shared stacks: Int = 1\n"
            "    func Tick() -> Void {\n"
            "        Log(stacks);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Zone Declaration",
            "zone DungeonZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    object slot playerView: PlayerView = ToObject(PlayerView, player)\n"
            "    tobject slot playerDto: PlayerDto = ToTObject(PlayerDto, player)\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player requires Commandable, Damageable\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poison to player by player\n"
            "    apply poisoned by player\n"
            "    link trust between player, enemy by player\n"
            "    link allied by player\n"
            "    detach poison from enemy by player\n"
            "    detach poisoned by player\n"
            "    unlink trust between player, enemy by player\n"
            "    unlink allied by player\n"
            "    bind playerView from player by player\n"
            "    bind playerDto from player by player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
            "    shared level: Int = 3\n"
            "    func Update() -> Void {\n"
            "        Log(level);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Zone Bind Declaration",
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player by player\n"
            "}\n",
            4
        },
        {
            "Zone Effect Pool Declaration",
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "}\n",
            3
        },
        {
            "Zone Declaration With Vessel Slot",
            "vessel HabitatState {\n"
            "    current: Int;\n"
            "}\n"
            "zone MeadowZone {\n"
            "    vessel slot habitat: HabitatState = HabitatState(3)\n"
            "}\n",
            2
        },
        {
            "Subject Declaration",
            "subject Counter {\n"
            "    let count: Int;\n"
            "    func Increment() -> Void {\n"
            "        count = count + 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Subject Declaration Alias",
            "subject Counter {\n"
            "    let count: Int;\n"
            "    func Increment() -> Void {\n"
            "        count = count + 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Break Continue",
            "func Looping() -> Void {\n"
            "    while true {\n"
            "        if false { break; }\n"
            "        continue;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Enum Declaration",
            "enum Color { Red, Green, Blue }\n"
            "func Main() -> Void {\n"
            "    let c: Color = Red;\n"
            "    Log(1);\n"
            "}",
            1
        },
        {
            "Event Lambda Subscription",
            "event OnHit(damage: Int);\n"
            "func Main() -> Void {\n"
            "    OnHit += (d: Int) => { Log(d); };\n"
            "    OnHit(77);\n"
            "}",
            1
        },
        {
            "Exported Function Declaration",
            "export func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}",
            1
        },
        {
            "Namespace Export Declaration",
            "namespace Math {\n"
            "    export func Add(a: Int, b: Int) -> Int {\n"
            "        return a + b;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Unsafe Block",
            "func Main() -> Void {\n"
            "    unsafe {\n"
            "        Log(1);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Defer Statement",
            "func Main() -> Void {\n"
            "    defer {\n"
            "        Log(1);\n"
            "    };\n"
            "}",
            1
        },
        {
            "Bind Statement",
            "func Main() -> Void {\n"
            "    bind team.fighter = Warrior;\n"
            "}",
            1
        },
        {
            "Context Identifier Allowed",
            "struct StrategyContext {\n"
            "    let threat: Int;\n"
            "}\n"
            "func ReadThreat(context: StrategyContext) -> Int {\n"
            "    return context.threat;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let context = StrategyContext(7);\n"
            "    Log(ReadThreat(context));\n"
            "}",
            1
        },
        {
            "Function Type Parameter Syntax",
            "struct StrategyContext {\n"
            "    let morale: Int;\n"
            "}\n"
            "func Apply(base: Int, ctx: StrategyContext, policy: func(Int, StrategyContext) -> Int) -> Int {\n"
            "    return policy(base, ctx);\n"
            "}\n",
            1
        },
        {
            "Else If Chain",
            "func Main() -> Void {\n"
            "    if true {\n"
            "        Log(1);\n"
            "    } else if false {\n"
            "        Log(2);\n"
            "    } else {\n"
            "        Log(3);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Type Alias Declaration",
            "type UserId = Int;",
            1
        },
        {
            "Multiline multiline-string literal",
            "func Main() -> Void {\n"
            "    Log(\"\"\"\n"
            "line1\n"
            "line2\n"
            "\"\"\");\n"
            "}",
            1
        },
        {
            "Multiline literal should not interpolate ${...}",
            "func Main() -> Void {\n"
            "    Log(\"\"\"\n"
            "${not-a-template}\n"
            "\"\"\");\n"
            "}",
            1
        }
    };

    int failures = 0;
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    printf("=== Pergyra Parser Test ===\n");

    for (size_t i = 0; i < num_tests; i++) {
        failures += run_parser_test(&tests[i]);
        printf("\n");
    }

    failures += run_doc_comment_attachment_test();
    printf("\n");
    failures += run_signature_effect_clause_test();
    printf("\n");
    failures += run_action_clause_reordering_test();
    printf("\n");
    failures += run_duplicate_action_clause_diagnostic_test();
    printf("\n");
    failures += run_subject_keyword_alias_test();
    printf("\n");
    failures += run_tobject_keyword_alias_test();
    printf("\n");
    failures += run_tobject_keyword_test();
    printf("\n");
    failures += run_object_keyword_alias_test();
    printf("\n");
    failures += run_vessel_keyword_alias_test();
    printf("\n");
    failures += run_lexical_zone_context_test();
    printf("\n");

    printf("\n=== All tests completed ===\n");
    if (failures > 0) {
        printf("Parser test failures: %d\n", failures);
        return 1;
    }

    return 0;
}
