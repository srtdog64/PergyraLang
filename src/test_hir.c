/*
 * Copyright (c) 2025 Pergyra Language Project
 * HIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"

static int g_pass = 0;
static int g_fail = 0;

typedef struct {
    size_t seen;
    size_t with_calls;
} HIRPassCounter;

typedef struct {
    size_t reachable_blocks;
    size_t blocks_with_phi;
    size_t blocks_with_dom_children;
} HIRBlockCounter;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static HIRProgram *
lower_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    HIRProgram *hir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        hir = hir_lower(sem->annotated_ast, &hir_error);

    if (hir == NULL && hir_error != NULL)
        fprintf(stderr, "HIR lowering error: %s\n", hir_error);

    free(hir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    /* HIR still references the annotated AST, so this helper intentionally
     * keeps semantic/AST memory alive for the duration of the test process.
     * The real driver destroys HIR before semantic/AST teardown. */
    return hir;
}

static bool
count_routines_pass(const HIRProgram *hir, const HIRRoutine *routine, void *userdata, char **error_message)
{
    (void)hir;
    (void)error_message;
    HIRPassCounter *counter = (HIRPassCounter *)userdata;
    counter->seen++;
    if (routine->direct_call_count > 0)
        counter->with_calls++;
    return true;
}

static bool
count_blocks_pass(const HIRProgram *hir,
                  const HIRRoutine *routine,
                  const HIRBasicBlock *block,
                  void *userdata,
                  char **error_message)
{
    (void)hir;
    (void)routine;
    (void)error_message;
    HIRBlockCounter *counter = (HIRBlockCounter *)userdata;
    if (block->is_reachable)
        counter->reachable_blocks++;
    if (block->phi_node_count > 0)
        counter->blocks_with_phi++;
    if (block->dom_tree_child_count > 0)
        counter->blocks_with_dom_children++;
    return true;
}

static void
test_hir_lowering(void)
{
    printf("\n[hir]\n");

    TEST("mixed top-level program is bucketed correctly");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "extern \"C\" { func SDL_Quit(); }\n"
            "func Main() -> Int { return 0; }\n"
            "let boot = 1;\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->extern_count == 1
               && hir->function_count == 1
               && hir->executable_count == 1
               && hir->has_main_function);
        hir_destroy(hir);
    }

    TEST("unsupported root is rejected");
    {
        ASTNode *number = ast_create_number("1");
        char *error = NULL;
        HIRProgram *hir = hir_lower(number, &error);
        EXPECT(hir == NULL && error != NULL);
        free(error);
        ast_destroy(number);
    }

    TEST("event lambda subscription lowers through HIR");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "func Main() -> Void {\n"
            "    OnHit += (d: Int) => { Log(d); };\n"
            "    OnHit(77);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->function_count == 1
               && hir->executable_count == 0
               && hir->has_main_function);
        hir_destroy(hir);
    }

    TEST("HIR exposes declaration index and routine summaries");
    {
        const char *src =
            "subject Member { let hp: Int; }\n"
            "zone PaymentZone { subject slot buyer: Member }\n"
            "func Helper() -> Int { return 1; }\n"
            "intent Purchase(payment: PaymentZone, buyer: Member) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: Helper();\n"
            "    }\n"
            "}\n"
            "func Main() -> Int {\n"
            "    return Helper();\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRDecl *intent_decl = hir_find_decl(hir, "Purchase", HIR_TOPLEVEL_INTENT);
        const HIRRoutine *main_routine = hir_find_routine(hir, "Main", HIR_TOPLEVEL_FUNCTION);
        const HIRRoutine *helper_routine = hir_find_routine(hir, "Helper", HIR_TOPLEVEL_FUNCTION);
        const HIRRoutine *intent_routine = hir_find_routine(hir, "Purchase", HIR_TOPLEVEL_INTENT);
        size_t helper_index = 0;
        if (hir != NULL && helper_routine != NULL)
            helper_index = (size_t)(helper_routine - hir->routines);
        EXPECT(hir != NULL
               && hir->decl_count >= 3
               && hir->routine_count >= 2
               && intent_decl != NULL
               && intent_decl->phase == HIR_PHASE_ROUTINE
               && main_routine != NULL
               && main_routine->is_entry_reachable
               && main_routine->signature_type_ref_count >= 1
               && strcmp(main_routine->signature_type_refs[0], "Int") == 0
               && main_routine->direct_call_count == 1
               && main_routine->callee_routine_count == 1
               && helper_routine != NULL
               && main_routine->callee_routine_ids[0] == helper_index
               && strcmp(main_routine->direct_calls[0], "Helper") == 0
               && intent_routine != NULL
               && intent_routine->is_entry_reachable
               && intent_routine->signature_type_ref_count >= 2
               && intent_routine->callee_routine_count == 1
               && intent_routine->direct_call_count == 1
               && strcmp(intent_routine->direct_calls[0], "Helper") == 0);
        hir_destroy(hir);
    }

    TEST("HIR routine pass can filter control-flow routines");
    {
        const char *src =
            "subject Member { let hp: Int; }\n"
            "zone PaymentZone { subject slot buyer: Member }\n"
            "func Helper() -> Int { return 1; }\n"
            "func Looping(flag: Bool) -> Int {\n"
            "    while flag {\n"
            "        return Helper();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Member) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: Helper();\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRPassCounter counter = {0};
        HIRRoutinePass pass;
        memset(&pass, 0, sizeof(pass));
        pass.name = "count-control-flow";
        pass.filter.include_functions = true;
        pass.filter.include_intents = true;
        pass.filter.require_control_flow = true;
        pass.run = count_routines_pass;
        pass.userdata = &counter;
        EXPECT(hir != NULL
               && hir_run_routine_pass(hir, &pass, NULL)
               && pass.routines_visited == 3
               && pass.routines_matched == 2
               && counter.seen == 2
               && counter.with_calls == 2);
        hir_destroy(hir);
    }

    TEST("HIR function CFG lowers basic blocks for if and while");
    {
        const char *src =
            "func Flow(flag: Bool) -> Int {\n"
            "    let base = 1;\n"
            "    if flag {\n"
            "        while flag {\n"
            "            base = base + 1;\n"
            "        }\n"
            "    }\n"
            "    return base;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *flow = hir_find_routine(hir, "Flow", HIR_TOPLEVEL_FUNCTION);
        bool found_loop_header = false;
        bool found_branch = false;
        bool loop_header_has_pred = false;
        bool loop_header_has_idom = false;
        bool found_frontier = false;
        bool found_loop_depth = false;
        bool entry_has_self_idom = false;
        if (flow != NULL && flow->has_cfg) {
            for (size_t i = 0; i < flow->cfg.block_count; i++) {
                const HIRBasicBlock *block = &flow->cfg.blocks[i];
                if (i == flow->cfg.entry_block
                    && block->is_reachable
                    && block->has_immediate_dominator
                    && block->immediate_dominator == i) {
                    entry_has_self_idom = true;
                }
                if (block->is_loop_header) {
                    found_loop_header = true;
                    if (block->predecessor_count > 0)
                        loop_header_has_pred = true;
                    if (block->has_immediate_dominator)
                        loop_header_has_idom = true;
                }
                if (block->dominance_frontier_count > 0)
                    found_frontier = true;
                if (block->loop_depth > 0)
                    found_loop_depth = true;
                if (block->terminator_kind == HIR_BLOCK_BRANCH)
                    found_branch = true;
            }
        }
        EXPECT(hir != NULL
               && flow != NULL
               && flow->has_cfg
               && flow->cfg.block_count >= 6
               && flow->return_block_count == 1
               && flow->normal_exit_block_count == 0
               && flow->cfg.entry_block == 0
               && found_loop_header
               && loop_header_has_pred
               && loop_header_has_idom
               && found_frontier
               && found_loop_depth
               && entry_has_self_idom
               && found_branch);
        hir_destroy(hir);
    }

    TEST("HIR computes phi candidates for joined local defs");
    {
        const char *src =
            "func Merge(flag: Bool) -> Int {\n"
            "    let score = 0;\n"
            "    if flag {\n"
            "        score = 1;\n"
            "    } else {\n"
            "        score = 2;\n"
            "    }\n"
            "    return score;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *merge = hir_find_routine(hir, "Merge", HIR_TOPLEVEL_FUNCTION);
        bool found_score_phi = false;
        bool found_defs = false;
        if (merge != NULL && merge->has_cfg) {
            for (size_t i = 0; i < merge->cfg.block_count; i++) {
                const HIRBasicBlock *block = &merge->cfg.blocks[i];
                if (block->local_def_count > 0)
                    found_defs = true;
                for (size_t j = 0; j < block->phi_candidate_count; j++) {
                    if (strcmp(block->phi_candidates[j], "score") == 0)
                        found_score_phi = true;
                }
            }
        }
        EXPECT(hir != NULL
               && merge != NULL
               && merge->has_cfg
               && found_defs
               && merge->return_block_count == 1
               && merge->normal_exit_block_count == 0
               && merge->phi_candidate_count > 0
               && merge->phi_candidate_block_count > 0
               && found_score_phi);
        hir_destroy(hir);
    }

    TEST("HIR preserves pin block region metadata across CFG blocks");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        if flag {\n"
            "            Write(view, 1);\n"
            "        } else {\n"
            "            Write(view, 2);\n"
            "        }\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "PinFlow", HIR_TOPLEVEL_FUNCTION);
        bool found_pin = false;
        bool found_after_pin = false;
        bool found_write_mode = false;
        bool found_source = false;
        bool found_view = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_pin_region) {
                    found_pin = true;
                    if (block->pin_view_is_write)
                        found_write_mode = true;
                    if (block->pin_source_name != NULL
                        && strcmp(block->pin_source_name, "scores") == 0)
                        found_source = true;
                    if (block->pin_view_name != NULL
                        && strcmp(block->pin_view_name, "view") == 0)
                        found_view = true;
                } else {
                    for (size_t s = 0; s < block->statement_count; s++) {
                        ASTNode *stmt = block->statements[s];
                        if (stmt != NULL && stmt->type == AST_CALL
                            && stmt->data.call.callee != NULL
                            && stmt->data.call.callee->type == AST_IDENTIFIER
                            && strcmp(stmt->data.call.callee->data.identifier.name, "Release") == 0) {
                            found_after_pin = true;
                        }
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && routine->return_block_count == 0
               && routine->normal_exit_block_count == 1
               && found_pin
               && found_after_pin
               && found_write_mode
               && found_source
               && found_view);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers loop break and continue edges explicitly");
    {
        const char *src =
            "func LoopEdges(flag: Bool) -> Int {\n"
            "    let i = 0;\n"
            "    while flag {\n"
            "        if flag {\n"
            "            continue;\n"
            "        }\n"
            "        break;\n"
            "    }\n"
            "    return i;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "LoopEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_loop_header = false;
        bool found_break_edge = false;
        bool found_continue_edge = false;
        size_t loop_header = 0;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_loop_header) {
                    found_loop_header = true;
                    loop_header = i;
                    break;
                }
            }
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind != HIR_BLOCK_GOTO || !block->has_succ_true)
                    continue;
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_CONTINUE
                        && found_loop_header && block->succ_true == loop_header) {
                        found_continue_edge = true;
                    }
                    if (stmt != NULL && stmt->type == AST_BREAK
                        && found_loop_header && block->succ_true != loop_header) {
                        found_break_edge = true;
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && routine->return_block_count == 1
               && routine->normal_exit_block_count == 0
               && found_loop_header
               && found_break_edge
               && found_continue_edge);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers match cases and default as explicit edges");
    {
        const char *src =
            "func MatchEdges(value: Int) -> Int {\n"
            "    match value {\n"
            "        case 0:\n"
            "            return 1;\n"
            "        case 1:\n"
            "            value = value + 1;\n"
            "        default:\n"
            "            value = value + 2;\n"
            "    }\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "MatchEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_match_dispatch = false;
        bool found_case_return = false;
        bool found_case_join = false;
        bool found_default_join = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind == HIR_BLOCK_BRANCH
                    && block->terminator_condition != NULL
                    && block->terminator_condition->type == AST_MATCH_CASE) {
                    found_match_dispatch = true;
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN) {
                    for (size_t j = 0; j < block->statement_count; j++) {
                        ASTNode *stmt = block->statements[j];
                        if (stmt != NULL && stmt->type == AST_RETURN
                            && stmt->data.return_stmt.value != NULL
                            && stmt->data.return_stmt.value->type == AST_NUMBER
                            && stmt->data.return_stmt.value->data.number.value == 1) {
                            found_case_return = true;
                        }
                    }
                }
                if (block->terminator_kind == HIR_BLOCK_GOTO && block->has_succ_true) {
                    for (size_t j = 0; j < block->statement_count; j++) {
                        ASTNode *stmt = block->statements[j];
                        if (stmt != NULL && stmt->type == AST_ASSIGNMENT) {
                            if (stmt->data.assignment.value != NULL
                                && stmt->data.assignment.value->type == AST_BINARY
                                && stmt->data.assignment.value->data.binary.right != NULL
                                && stmt->data.assignment.value->data.binary.right->type == AST_NUMBER
                                && stmt->data.assignment.value->data.binary.right->data.number.value == 1) {
                                found_case_join = true;
                            }
                            if (stmt->data.assignment.value != NULL
                                && stmt->data.assignment.value->type == AST_BINARY
                                && stmt->data.assignment.value->data.binary.right != NULL
                                && stmt->data.assignment.value->data.binary.right->type == AST_NUMBER
                                && stmt->data.assignment.value->data.binary.right->data.number.value == 2) {
                                found_default_join = true;
                            }
                        }
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_match_dispatch
               && found_case_return
               && found_case_join
               && found_default_join);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers select cases and default as explicit edges");
    {
        const char *src =
            "func SelectEdges(ch: Channel<Int>) -> Int {\n"
            "    select {\n"
            "        case v = <-ch:\n"
            "            return v;\n"
            "        default:\n"
            "            Log(0);\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "SelectEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_select_dispatch = false;
        bool found_case_receive = false;
        bool found_case_return = false;
        bool found_default_join = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind == HIR_BLOCK_BRANCH
                    && block->terminator_condition != NULL
                    && block->terminator_condition->type == AST_BLOCK) {
                    ASTNode *case_block = block->terminator_condition;
                    if (case_block->data.block.count > 0
                        && case_block->data.block.statements[0] != NULL
                        && case_block->data.block.statements[0]->type == AST_ASSIGNMENT) {
                        found_select_dispatch = true;
                    }
                }
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_ASSIGNMENT
                        && stmt->data.assignment.value != NULL
                        && stmt->data.assignment.value->type == AST_CHANNEL_RECV) {
                        found_case_receive = true;
                    }
                    if (stmt != NULL && stmt->type == AST_CALL
                        && block->terminator_kind == HIR_BLOCK_GOTO
                        && block->has_succ_true) {
                        found_default_join = true;
                    }
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN
                    && block->terminator_value != NULL
                    && block->terminator_value->type == AST_IDENTIFIER
                    && strcmp(block->terminator_value->data.identifier.name, "v") == 0) {
                    found_case_return = true;
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_select_dispatch
               && found_case_receive
               && found_case_return
               && found_default_join);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers unsafe block body control flow");
    {
        const char *src =
            "func UnsafeReturn(flag: Bool) -> Int {\n"
            "    unsafe {\n"
            "        if flag {\n"
            "            return 1;\n"
            "        }\n"
            "    }\n"
            "    return 2;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "UnsafeReturn", HIR_TOPLEVEL_FUNCTION);
        bool found_unsafe_payload = false;
        bool found_nested_return = false;
        bool found_after_return = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_UNSAFE_BLOCK)
                        found_unsafe_payload = true;
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN
                    && block->terminator_value != NULL
                    && block->terminator_value->type == AST_NUMBER) {
                    if (block->terminator_value->data.number.value == 1)
                        found_nested_return = true;
                    if (block->terminator_value->data.number.value == 2)
                        found_after_return = true;
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_unsafe_payload
               && found_nested_return
               && found_after_return);
        hir_destroy(hir);
    }

    TEST("HIR CFG resolves labeled loop control to the named loop");
    {
        const char *src =
            "func LabeledLoopEdges(flag: Bool) -> Int {\n"
            "    let i = 0;\n"
            "    outer: while flag {\n"
            "        while flag {\n"
            "            if flag {\n"
            "                continue outer;\n"
            "            }\n"
            "            break outer;\n"
            "        }\n"
            "        i = i + 1;\n"
            "    }\n"
            "    return i;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "LabeledLoopEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_outer_header = false;
        bool found_continue_outer = false;
        bool found_break_outer_exit = false;
        size_t outer_header = 0;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_loop_header) {
                    outer_header = i;
                    found_outer_header = true;
                    break;
                }
            }
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind != HIR_BLOCK_GOTO || !block->has_succ_true)
                    continue;
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_CONTINUE
                        && stmt->data.continue_stmt.label != NULL
                        && strcmp(stmt->data.continue_stmt.label, "outer") == 0
                        && found_outer_header
                        && block->succ_true == outer_header) {
                        found_continue_outer = true;
                    }
                    if (stmt != NULL && stmt->type == AST_BREAK
                        && stmt->data.break_stmt.label != NULL
                        && strcmp(stmt->data.break_stmt.label, "outer") == 0
                        && block->succ_true < routine->cfg.block_count) {
                        const HIRBasicBlock *target = &routine->cfg.blocks[block->succ_true];
                        if (target->terminator_kind == HIR_BLOCK_RETURN)
                            found_break_outer_exit = true;
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_outer_header
               && found_continue_outer
               && found_break_outer_exit);
        hir_destroy(hir);
    }

    TEST("HIR block pass can walk SSA-prep reachable blocks");
    {
        const char *src =
            "func Merge(flag: Bool) -> Int {\n"
            "    let score = 0;\n"
            "    if flag {\n"
            "        score = 1;\n"
            "    } else {\n"
            "        score = 2;\n"
            "    }\n"
            "    return score;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRBlockCounter counter = {0};
        HIRBlockPass pass;
        memset(&pass, 0, sizeof(pass));
        pass.name = "count-ssa-blocks";
        pass.filter.include_functions = true;
        pass.filter.require_cfg = true;
        pass.filter.include_reachable_blocks = true;
        pass.run = count_blocks_pass;
        pass.userdata = &counter;
        EXPECT(hir != NULL
               && hir_run_block_pass(hir, &pass, NULL)
               && pass.routines_visited >= 1
               && pass.blocks_visited >= pass.blocks_matched
               && pass.blocks_matched == counter.reachable_blocks
               && counter.blocks_with_phi > 0
               && counter.blocks_with_dom_children > 0);
        hir_destroy(hir);
    }
}

int
main(void)
{
    printf("=== Pergyra HIR Lowering Test Suite ===\n");
    test_hir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
