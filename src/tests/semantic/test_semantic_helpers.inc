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
#include "compiler/import_resolver.h"
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
static const char *g_current_test_name = NULL;

#define TEST(name) \
    do { g_current_test_name = (name); printf("  %-55s", name); } while(0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else       { \
            printf("✗  (%s @ line %d)\n", \
                   g_current_test_name != NULL ? g_current_test_name : "<unnamed test>", \
                   __LINE__); \
            g_fail++; \
        } \
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
