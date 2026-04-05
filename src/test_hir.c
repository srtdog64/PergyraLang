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
               && merge->phi_candidate_count > 0
               && merge->phi_candidate_block_count > 0
               && found_score_phi);
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
