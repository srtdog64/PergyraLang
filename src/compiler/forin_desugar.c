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
 * counter makes every site unique. */
#define PGY_FORIN_PREFIX "__pgy_forin_"

static unsigned g_forin_counter;

static void desugar_node(ASTNode *node);

/* Build `{ let __pgy_forin_N = <iterable>; for VAR in __pgy_forin_N { BODY } }`
 * by mutating `loop` in place and wrapping it in a fresh block. Ownership of
 * the original iterable node is transferred to the synthetic let (re-parented,
 * not copied). Returns the wrapping block, or `loop` unchanged on allocation
 * failure (so the caller keeps a valid tree). */
static ASTNode *
desugar_for_loop_slot(ASTNode *loop)
{
    ASTNode *iterable = ast_for_iterable(loop);
    if (iterable == NULL || iterable->type == AST_IDENTIFIER)
        return loop;

    char name[64];
    unsigned n = g_forin_counter++;
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
    let->data.let_decl.initializer = iterable;

    /* Rewrite the loop to iterate the hoisted local (now an identifier). */
    loop->data.for_loop.iterable = ident;

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
desugar_statement_list(ASTNode **statements, size_t count)
{
    if (statements == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        if (stmt == NULL)
            continue;
        desugar_node(stmt);
        if (stmt->type == AST_FOR_LOOP
            && ast_for_iterable(stmt) != NULL
            && ast_for_iterable(stmt)->type != AST_IDENTIFIER) {
            statements[i] = desugar_for_loop_slot(stmt);
        }
    }
}

/* Recurse into every child that can transitively contain a statement list.
 * Statement-list owners get their slots rewritten in desugar_statement_list;
 * everything else just forwards into its body/branch nodes. The body slots
 * (func/loop/if/with/match) are AST_BLOCK nodes, so visiting them reaches
 * their statement lists. */
static void
desugar_node(ASTNode *node)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_PROGRAM:
        desugar_statement_list(node->data.program.statements,
                               node->data.program.count);
        return;
    case AST_BLOCK:
        desugar_statement_list(node->data.block.statements,
                               node->data.block.count);
        return;
    case AST_ASYNC_BLOCK:
        desugar_statement_list(node->data.async_block.statements,
                               node->data.async_block.statement_count);
        return;
    case AST_PARALLEL_BLOCK:
        desugar_statement_list(node->data.parallel.tasks,
                               node->data.parallel.task_count);
        return;
    case AST_EXTERN_BLOCK:
        desugar_statement_list(node->data.extern_block.declarations,
                               node->data.extern_block.count);
        return;
    case AST_NAMESPACE_DECL:
        desugar_statement_list(node->data.namespace_decl.statements,
                               node->data.namespace_decl.count);
        return;
    case AST_FUNC_DECL:
        desugar_node(node->data.func_decl.body);
        return;
    case AST_FOR_LOOP:
        /* Only the body can hold nested loops; the iterable/range are
         * expressions, handled where the loop sits in its statement list. */
        desugar_node(node->data.for_loop.body);
        return;
    case AST_WHILE_LOOP:
        desugar_node(node->data.while_loop.body);
        return;
    case AST_IF_STMT:
        desugar_node(node->data.if_stmt.then_branch);
        desugar_node(node->data.if_stmt.else_branch);
        return;
    case AST_WITH_STMT:
        desugar_node(node->data.with_stmt.body);
        return;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            desugar_node(node->data.match_stmt.cases[i]);
        desugar_node(node->data.match_stmt.default_body);
        return;
    case AST_MATCH_CASE:
        desugar_node(node->data.match_case.guard);
        desugar_node(node->data.match_case.body);
        return;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            desugar_node(node->data.select_stmt.cases[i]);
        desugar_node(node->data.select_stmt.default_case);
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
        methods = node->data.class_decl.methods;
        method_count = node->data.class_decl.method_count;
        break;
    case AST_ENUM_DECL:
        methods = node->data.enum_decl.methods;
        method_count = node->data.enum_decl.method_count;
        break;
    case AST_ABILITY_DECL:
        methods = node->data.ability_decl.methods;
        method_count = node->data.ability_decl.method_count;
        break;
    case AST_IMPL_ABILITY:
        methods = node->data.impl_ability.methods;
        method_count = node->data.impl_ability.method_count;
        break;
    case AST_PARTY_DECL:
        methods = node->data.party_decl.methods;
        method_count = node->data.party_decl.method_count;
        break;
    case AST_ROSTER_DECL:
        methods = node->data.roster_decl.methods;
        method_count = node->data.roster_decl.method_count;
        break;
    case AST_WORLD_DECL:
        methods = node->data.world_decl.methods;
        method_count = node->data.world_decl.method_count;
        break;
    case AST_RELATION_DECL:
        methods = node->data.relation_decl.methods;
        method_count = node->data.relation_decl.method_count;
        break;
    case AST_EFFECT_DECL:
        methods = node->data.effect_decl.methods;
        method_count = node->data.effect_decl.method_count;
        break;
    case AST_ZONE_DECL:
        methods = node->data.zone_decl.methods;
        method_count = node->data.zone_decl.method_count;
        break;
    default:
        return;
    }
    for (size_t i = 0; i < method_count; i++) {
        if (methods[i] != NULL)
            desugar_node(methods[i]);
    }
}

void
forin_desugar_program(ASTNode *program)
{
    if (program == NULL)
        return;
    g_forin_counter = 0;
    desugar_node(program);
}
