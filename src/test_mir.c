/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"
#include "compiler/rir.h"
#include "compiler/mir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static bool
lower_mir_from_source(const char *source, HIRProgram **hir_out, RIRProgram **rir_out, MIRProgram **mir_out)
{
    bool ok = false;
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;

    *hir_out = NULL;
    *rir_out = NULL;
    *mir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            *mir_out = mir_lower(*hir_out, *rir_out, &mir_error);
    }

    ok = (*hir_out != NULL && *rir_out != NULL && *mir_out != NULL);
    if (!ok) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
        if (mir_error != NULL)
            fprintf(stderr, "MIR lowering error: %s\n", mir_error);
    }

    free(hir_error);
    free(rir_error);
    free(mir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static const MIRRoutine *
find_mir_routine(const MIRProgram *mir, const char *name, MIRScopeKind kind)
{
    if (mir == NULL)
        return NULL;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (mir->routines[i].kind == kind
            && mir->routines[i].name != NULL
            && strcmp(mir->routines[i].name, name) == 0) {
            return &mir->routines[i];
        }
    }
    return NULL;
}

static bool
block_has_inst_kind(const MIRBasicBlock *block, MIRInstKind kind)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        if (block->instructions[i].kind == kind)
            return true;
    }
    return false;
}

static bool
block_has_entry_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->ssa_entry_value_count; i++) {
        if (block->ssa_entry_values[i] != NULL
            && strncmp(block->ssa_entry_values[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_exit_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->ssa_exit_value_count; i++) {
        if (block->ssa_exit_values[i] != NULL
            && strncmp(block->ssa_exit_values[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_inst_named(const MIRBasicBlock *block, const char *name)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        if (block->instructions[i].name != NULL
            && strcmp(block->instructions[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_inst_named_with_slot(const MIRBasicBlock *block, const char *name, const char *slot_anchor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->name != NULL
            && strcmp(inst->name, name) == 0
            && inst->slot_anchor != NULL
            && strcmp(inst->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_phi_result_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI
            && inst->result_name != NULL
            && strncmp(inst->result_name, prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_renamed_local_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->renamed_local_count; i++) {
        if (block->renamed_locals[i] != NULL
            && strncmp(block->renamed_locals[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_use_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        for (size_t j = 0; j < inst->use_count; j++) {
            if (inst->uses[j] != NULL
                && strncmp(inst->uses[j], prefix, prefix_len) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool
block_has_live_out_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->live_out_name_count; i++) {
        if (block->live_out_names[i] != NULL
            && strncmp(block->live_out_names[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static const MIRValueSummary *
find_value_summary_prefix_with_use(const MIRRoutine *routine, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    const MIRValueSummary *best = NULL;
    if (routine == NULL)
        return NULL;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        const MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->name != NULL
            && strncmp(summary->name, prefix, prefix_len) == 0) {
            if (best == NULL || summary->use_count > best->use_count)
                best = summary;
        }
    }
    return best;
}

static bool
routine_has_result_prefix(const MIRRoutine *routine, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (routine == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->result_name != NULL
                && strncmp(inst->result_name, prefix, prefix_len) == 0) {
                return true;
            }
        }
    }
    return false;
}

static const MIRValueSummary *
find_value_summary_with_slot(const MIRRoutine *routine, const char *prefix, const char *slot_anchor)
{
    size_t prefix_len = strlen(prefix);
    if (routine == NULL)
        return NULL;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        const MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->name != NULL
            && strncmp(summary->name, prefix, prefix_len) == 0
            && summary->slot_anchor != NULL
            && strcmp(summary->slot_anchor, slot_anchor) == 0) {
            return summary;
        }
    }
    return NULL;
}

static void
test_mir_lowering(void)
{
    printf("\n[mir]\n");

    TEST("MIR lifts slot flow into routine instructions");
    {
        const char *src =
            "func Flow() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(s, 1);\n"
            "    let v = Read(s);\n"
            "    Release(s);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *flow = NULL;
        const MIRValueSummary *value_summary = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        if (flow != NULL)
            value_summary = find_value_summary_with_slot(flow, "s.", "s");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && flow != NULL
               && flow->block_count >= 1
               && flow->instruction_count >= 4
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Claim", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Write", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Read", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Release", "s")
               && value_summary != NULL);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR builds cleanup block for intent compensation");
    {
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object BuyerView { let hp: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    rollback: full;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        on: buyer.Pay();\n"
            "        compensate: buyer.Pay();\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *purchase = NULL;
        bool cleanup_has_compensate = false;
        bool cleanup_has_policy = false;
        bool cleanup_has_invalidation = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_rollback_block) {
            const MIRBasicBlock *rollback = &purchase->blocks[purchase->rollback_block];
            cleanup_has_compensate = block_has_inst_kind(rollback, MIR_INST_CLEANUP_EDGE)
                                     && block_has_inst_named_with_slot(rollback, "CompensateIntentStep", "pay");
            cleanup_has_policy = block_has_inst_named(rollback, "RollbackPolicy");
        }
        if (purchase != NULL && purchase->has_invalidation_block) {
            const MIRBasicBlock *invalidation = &purchase->blocks[purchase->invalidation_block];
            cleanup_has_invalidation = block_has_inst_named_with_slot(invalidation, "DetachInvalidation", "payment");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && purchase != NULL
               && purchase->has_cleanup_block
               && purchase->has_rollback_block
               && purchase->has_invalidation_block
               && purchase->cleanup_instruction_count >= 2
               && purchase->cleanup_edge_count >= 1
               && purchase->blocks[purchase->entry_block].has_cleanup_succ
               && purchase->blocks[purchase->entry_block].cleanup_succ == purchase->cleanup_block
               && purchase->blocks[purchase->cleanup_block].has_rollback_succ
               && purchase->blocks[purchase->cleanup_block].rollback_succ == purchase->rollback_block
               && purchase->blocks[purchase->cleanup_block].has_invalidation_succ
               && purchase->blocks[purchase->cleanup_block].invalidation_succ == purchase->invalidation_block
               && purchase->blocks[purchase->rollback_block].has_cleanup_succ
               && purchase->blocks[purchase->rollback_block].cleanup_succ == purchase->invalidation_block
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block], "cleanup-edge", "cleanup")
               && cleanup_has_compensate
               && cleanup_has_policy
               && cleanup_has_invalidation
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->rollback_block], "AbortIntent", "Purchase"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR inserts phi nodes and SSA-renamed locals on merge");
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *merge = NULL;
        bool has_phi = false;
        bool has_phi_inputs = false;
        bool has_renamed_local = false;
        bool has_branch = false;
        bool has_return = false;
        bool has_uses = false;
        bool has_def = false;
        bool has_entry = false;
        bool has_exit = false;
        bool has_live_out = false;
        const MIRValueSummary *score_summary = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            merge = find_mir_routine(mir, "Merge", MIR_SCOPE_FUNCTION);
        if (merge != NULL)
            score_summary = find_value_summary_prefix_with_use(merge, "score.");
        if (merge != NULL) {
            for (size_t i = 0; i < merge->block_count; i++) {
                const MIRBasicBlock *block = &merge->blocks[i];
                if (block_has_phi_result_prefix(block, "score."))
                    has_phi = true;
                if (block_has_renamed_local_prefix(block, "score."))
                    has_renamed_local = true;
                if (block_has_use_prefix(block, "score."))
                    has_uses = true;
                if (block_has_inst_kind(block, MIR_INST_DEF))
                    has_def = true;
                if (block_has_entry_prefix(block, "score."))
                    has_entry = true;
                if (block_has_exit_prefix(block, "score."))
                    has_exit = true;
                if (block_has_live_out_prefix(block, "score."))
                    has_live_out = true;
                for (size_t j = 0; j < block->instruction_count; j++) {
                    const MIRInstruction *inst = &block->instructions[j];
                    if (inst->kind == MIR_INST_PHI && inst->phi_incoming_count >= 2)
                        has_phi_inputs = true;
                    if (inst->kind == MIR_INST_BRANCH)
                        has_branch = true;
                    if (inst->kind == MIR_INST_RETURN)
                        has_return = true;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && merge != NULL
               && merge->phi_inserted_count > 0
               && merge->renamed_value_count > 0
               && has_phi
               && has_phi_inputs
               && has_renamed_local
               && has_def
               && has_entry
               && has_exit
               && has_live_out
               && has_uses
               && has_branch
               && has_return
               && merge->has_liveness
               && merge->has_dce
               && merge->live_value_count > 0
               && merge->has_use_def_summary
               && score_summary != NULL
               && score_summary->slot_anchor != NULL
               && strcmp(score_summary->slot_anchor, "score") == 0
               && score_summary->use_count > 0
               && score_summary->live_out_block_count > 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR DCE removes dead SSA defs and dead phi merges");
    {
        const char *src =
            "func DeadMerge(flag: Bool) -> Int {\n"
            "    let live = 1;\n"
            "    let dead = 0;\n"
            "    if flag {\n"
            "        dead = 2;\n"
            "    } else {\n"
            "        dead = 3;\n"
            "    }\n"
            "    return live;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *dead_merge = NULL;
        const MIRValueSummary *live_summary = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            dead_merge = find_mir_routine(mir, "DeadMerge", MIR_SCOPE_FUNCTION);
        if (dead_merge != NULL)
            live_summary = find_value_summary_prefix_with_use(dead_merge, "live.");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && dead_merge != NULL
               && dead_merge->has_dce
               && dead_merge->dce_removed_count > 0
               && live_summary != NULL
               && !routine_has_result_prefix(dead_merge, "dead."));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}

int
main(void)
{
    printf("=== Pergyra MIR Lowering Test Suite ===\n");
    test_mir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
