/*
 * Copyright (c) 2025 Pergyra Language Project
 *
 * for-in non-identifier iterable desugar (post-parse, pre-semantic).
 *
 * Problem: `for x in <EXPR>` only lowered cleanly when <EXPR> was a bare
 * identifier. The LLVM MIR for-in lowering hard-requires an identifier
 * iterable (llvm_mir_for_in_control.c), and the C transpiler re-emits the
 * iterable expression on every loop step, which would re-evaluate calls.
 *
 * Fix (desugar): rewrite
 *     for VAR in EXPR { BODY }        // EXPR is non-NULL, type != identifier
 * into the semantically-equivalent
 *     { let __pgy_forin_N = EXPR; for VAR in __pgy_forin_N { BODY } }
 * The hoisted local is evaluated exactly once before the loop, and after
 * the rewrite the loop iterable is an identifier, so the existing
 * identifier path serves BOTH backends with one code path.
 *
 * This runs in the compile path only (driver_app.c, just before
 * semantic_analyze). It does NOT run in the parser nor in the `--ast`
 * dump, so parser-parity is unaffected. Range loops (range_start/range_end)
 * and identifier/variable iterables are left byte-for-byte unchanged.
 */

#include "forin_desugar.h"

#include "../parser/ast_api.h"
#include "../parser/ast.h"

#include <stddef.h>
#include <stdio.h>

/* Reserved synthetic-name prefix; cannot collide with user identifiers
 * because `__pgy_forin_` is not a legal user-chosen convention and the
 * per-program counter makes every site unique. */
#define PGY_FORIN_PREFIX "__pgy_forin_"

static void desugar_node(ASTNode *node, unsigned *counter);

/* Build `{ let __pgy_forin_N = <iterable>; for VAR in __pgy_forin_N { BODY } }`
 * by mutating `loop` in place and wrapping it in a fresh block. Ownership of
 * the original iterable node is transferred to the synthetic let (re-parented,
 * not copied). Returns the wrapping block, or `loop` unchanged on allocation
 * failure (so the caller keeps a valid tree). */
static ASTNode *
desugar_for_loop_slot(ASTNode *loop, unsigned *counter)
{
    ASTNode *iterable = ast_for_iterable(loop);
    if (iterable == NULL || iterable->type == AST_IDENTIFIER)
        return loop;

    char name[64];
    unsigned n = counter != NULL ? (*counter)++ : 0;
    snprintf(name, sizeof(name), PGY_FORIN_PREFIX "%u", n);

    ASTNode *let = ast_create_let_declaration(name);
    ASTNode *ident = ast_create_identifier(name);
    ASTNode *block = ast_create_block();
    if (let == NULL || ident == NULL || block == NULL)
        return loop;

    /* Provenance: inherit the loop's source position. */
    let->line = loop->line;
    let->column = loop->column;
    ident->line = loop->line;
    ident->column = loop->column;
    block->line = loop->line;
    block->column = loop->column;

    /* Re-parent the existing iterable node into the let initializer. */
    iterable = ast_for_detach_iterable(loop);
    if (iterable == NULL)
        return loop;
    if (!ast_let_attach_initializer(let, iterable)) {
        (void)ast_for_attach_iterable(loop, iterable);
        return loop;
    }

    /* Rewrite the loop to iterate the hoisted local (now an identifier). */
    if (!ast_for_attach_iterable(loop, ident)) {
        (void)ast_for_attach_iterable(loop, iterable);
        return loop;
    }

    /* Assemble the block: [ let, loop ]. */
    ast_add_statement(block, let);
    ast_add_statement(block, loop);
    return block;
}

/* Rewrite for-loop statements that appear directly in a statement list. The
 * slot is replaced with the wrapping block so the hoisted `let` becomes a
 * sibling statement evaluated once before the loop. Recurses into each
 * statement first so nested loops are handled. */
static void
desugar_statement_list(ASTNode **statements, size_t count, unsigned *counter)
{
    if (statements == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        if (stmt == NULL)
            continue;
        desugar_node(stmt, counter);
        if (stmt->type == AST_FOR_LOOP
            && ast_for_iterable(stmt) != NULL
            && ast_for_iterable(stmt)->type != AST_IDENTIFIER) {
            statements[i] = desugar_for_loop_slot(stmt, counter);
        }
    }
}

/* Recurse into every child that can transitively contain a statement list.
 * Statement-list owners get their slots rewritten in desugar_statement_list;
 * everything else just forwards into its body/branch nodes. The body slots
 * (func/loop/if/with/match) are AST_BLOCK nodes, so visiting them reaches
 * their statement lists. */
static void
desugar_node(ASTNode *node, unsigned *counter)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_PROGRAM:
    {
        size_t statement_count = 0;
        ASTNode **statements = ast_program_statements(node, &statement_count);
        desugar_statement_list(statements, statement_count, counter);
        return;
    }
    case AST_BLOCK:
    {
        size_t statement_count = 0;
        ASTNode **statements = ast_block_statements(node, &statement_count);
        desugar_statement_list(statements, statement_count, counter);
        return;
    }
    case AST_ASYNC_BLOCK:
    {
        size_t statement_count = 0;
        ASTNode **statements =
            ast_async_block_statements(node, &statement_count);
        desugar_statement_list(statements, statement_count, counter);
        return;
    }
    case AST_PARALLEL_BLOCK:
    {
        size_t task_count = 0;
        ASTNode **tasks = ast_parallel_tasks(node, &task_count);
        desugar_statement_list(tasks, task_count, counter);
        return;
    }
    case AST_EXTERN_BLOCK:
    {
        size_t declaration_count = 0;
        ASTNode **declarations =
            ast_extern_block_declarations(node, &declaration_count);
        desugar_statement_list(declarations, declaration_count, counter);
        return;
    }
    case AST_NAMESPACE_DECL:
    {
        size_t statement_count = 0;
        ASTNode **statements = ast_namespace_statements(node, &statement_count);
        desugar_statement_list(statements, statement_count, counter);
        return;
    }
    case AST_FUNC_DECL:
        desugar_node(ast_func_body(node), counter);
        return;
    case AST_FOR_LOOP:
        /* Only the body can hold nested loops; the iterable/range are
         * expressions, handled where the loop sits in its statement list. */
        desugar_node(ast_for_body(node), counter);
        return;
    case AST_WHILE_LOOP:
        desugar_node(ast_while_body(node), counter);
        return;
    case AST_IF_STMT:
        desugar_node(ast_if_then_branch(node), counter);
        desugar_node(ast_if_else_branch(node), counter);
        return;
    case AST_WITH_STMT:
        desugar_node(ast_with_body(node), counter);
        return;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            desugar_node(ast_match_case_at(node, i), counter);
        desugar_node(ast_match_default_body(node), counter);
        return;
    case AST_MATCH_CASE:
        desugar_node(ast_match_case_guard(node), counter);
        desugar_node(ast_match_case_body(node), counter);
        return;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            desugar_node(ast_select_case(node, i), counter);
        desugar_node(ast_select_default_case(node), counter);
        return;
    default:
        break;
    }

    /* Method-bearing declarations (subject == AST_CLASS_DECL, enum, ability,
     * impl, party, roster, world, relation, effect, zone): walk attached
     * method bodies. Mirrors the method-list switch in ast_analysis.c so no
     * container type is silently skipped. */
    ASTNode **methods = NULL;
    size_t method_count = 0;
    switch (node->type) {
    case AST_CLASS_DECL:
        methods = ast_class_methods(node, &method_count);
        break;
    case AST_ENUM_DECL:
        methods = ast_enum_methods(node, &method_count);
        break;
    case AST_ABILITY_DECL:
        methods = ast_ability_methods(node, &method_count);
        break;
    case AST_IMPL_ABILITY:
        for (size_t i = 0; i < ast_impl_ability_method_count(node); i++)
            desugar_node(ast_impl_ability_method(node, i), counter);
        return;
    case AST_PARTY_DECL:
        methods = ast_party_methods(node, &method_count);
        break;
    case AST_ROSTER_DECL:
        methods = ast_roster_methods(node, &method_count);
        break;
    case AST_WORLD_DECL:
        methods = ast_world_methods(node, &method_count);
        break;
    case AST_RELATION_DECL:
        methods = ast_relation_methods(node, &method_count);
        break;
    case AST_EFFECT_DECL:
        methods = ast_effect_methods(node, &method_count);
        break;
    case AST_ZONE_DECL:
        methods = ast_zone_methods(node, &method_count);
        break;
    default:
        return;
    }
    for (size_t i = 0; i < method_count; i++) {
        if (methods[i] != NULL)
            desugar_node(methods[i], counter);
    }
}

void
forin_desugar_program(ASTNode *program)
{
    unsigned counter = 0;

    if (program == NULL)
        return;
    desugar_node(program, &counter);
}
