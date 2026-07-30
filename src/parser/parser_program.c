/*
 * Copyright (c) 2025 Pergyra Language Project
 * Program assembly and implicit-entry ownership.
 */

#include "parser_internal.h"
#include "ast_constructors_internal.h"

/* A top-level statement belongs in implicit Main only when it is executable.
 * The conservative allowlist never pulls a declaration into the entrypoint. */
static bool
parser_is_top_level_executable(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    switch (stmt->type) {
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_RETURN:
    case AST_ASSIGNMENT:
    case AST_LET_DECL:
    case AST_LET_DESTRUCTURE:
    case AST_BLOCK:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_DEFER_STMT:
    case AST_BIND_STMT:
    case AST_WITH_STMT:
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_UNSAFE_BLOCK:
    case AST_TRANSACTION_BLOCK:
    case AST_SELECT_STMT:
    case AST_CALL:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_AWAIT_EXPR:
    case AST_SPAWN_EXPR:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
        return true;
    default:
        return false;
    }
}

static bool
program_has_func_named(ASTNode *program, const char *name)
{
    size_t count = ast_program_statement_count(program);
    for (size_t i = 0; i < count; i++) {
        ASTNode *statement = program->data.program.statements[i];
        if (statement != NULL && statement->type == AST_FUNC_DECL
            && statement->data.func_decl.name != NULL
            && strcmp(statement->data.func_decl.name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Move top-level executable statements, in source order, into one synthesized
 * Main. Declarations remain at program scope and moved nodes have one owner. */
static void
parser_synthesize_implicit_main(Parser *parser, ASTNode *program)
{
    ASTNode *implicit_body = NULL;
    ASTNode *implicit_main = NULL;
    ASTNode *void_type = NULL;
    size_t count;
    size_t write = 0;

    if (program == NULL || program->type != AST_PROGRAM)
        return;

    count = ast_program_statement_count(program);
    for (size_t i = 0; i < count; i++) {
        ASTNode *statement = program->data.program.statements[i];
        if (parser_is_top_level_executable(statement)) {
            if (implicit_body == NULL)
                implicit_body = ast_create_block();
            ast_add_statement(implicit_body, statement);
        } else if (statement != NULL) {
            program->data.program.statements[write++] = statement;
        }
    }
    program->data.program.count = write;

    if (implicit_body == NULL)
        return;
    if (program_has_func_named(program, "Main")) {
        parser_error(parser,
            "Top-level statements and an explicit 'func Main' are mutually "
            "exclusive: the top-level statements already form the program's "
            "Main.\n"
            "Fix: move the statements into 'func Main', or remove 'func Main' "
            "and keep them at top level.");
        ast_destroy(implicit_body);
        return;
    }

    implicit_main = ast_create_function("Main");
    void_type = implicit_main != NULL ? ast_create_type("Void") : NULL;
    if (implicit_main == NULL || void_type == NULL)
        goto implicit_main_oom;
    if (!ast_func_set_return_type(implicit_main, void_type))
        goto implicit_main_oom;
    void_type = NULL;
    if (!ast_func_attach_body(implicit_main, implicit_body))
        goto implicit_main_oom;
    implicit_body = NULL;
    if (!ast_program_append_statement(program, implicit_main))
        goto implicit_main_oom;
    return;

implicit_main_oom:
    parser_error(parser, "Out of memory while synthesizing implicit Main.");
    ast_destroy(void_type);
    ast_destroy(implicit_main);
    ast_destroy(implicit_body);
}

static ASTNode *
parser_parse_program_with_intent_finalization(Parser *parser,
                                              bool finalize_intent_roles)
{
    ASTNode *program = ast_create_program();

    while (!parser_is_at_end(parser)) {
        ASTNode *statement = parser_parse_statement(parser);
        if (statement != NULL) {
            if (statement->type == AST_BLOCK) {
                for (size_t i = 0; i < statement->data.block.count; i++) {
                    ASTNode *child = statement->data.block.statements[i];
                    if (child != NULL)
                        ast_add_statement(program, child);
                    statement->data.block.statements[i] = NULL;
                }
                ast_destroy(statement);
            } else {
                ast_add_statement(program, statement);
            }
        }
        if (parser->has_error)
            parser_synchronize(parser);
    }

    if (finalize_intent_roles
        && !parser_finalize_intent_parameter_roles(parser, program)) {
        ast_destroy(program);
        return NULL;
    }

    parser_synthesize_implicit_main(parser, program);
    if (!ast_assign_stable_ids(program)) {
        parser_error(parser, "Syntax node identity space exhausted.");
        ast_destroy(program);
        return NULL;
    }
    return program;
}

ASTNode *
parser_parse_program(Parser *parser)
{
    return parser_parse_program_with_intent_finalization(parser, true);
}

ASTNode *
parser_parse_program_for_module_composition(Parser *parser)
{
    return parser_parse_program_with_intent_finalization(parser, false);
}
