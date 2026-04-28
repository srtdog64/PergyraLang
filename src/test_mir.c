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

static MIRRoutine *
find_mir_routine_mut(MIRProgram *mir, const char *name, MIRScopeKind kind)
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
block_has_inst_named_args(const MIRBasicBlock *block,
                          const char *name,
                          const char *arg0,
                          const char *arg1)
{
    if (block == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->name == NULL || strcmp(inst->name, name) != 0)
            continue;
        if (arg0 != NULL) {
            if (inst->arg0 == NULL || strcmp(inst->arg0, arg0) != 0)
                continue;
        }
        if (arg1 != NULL) {
            if (inst->arg1 == NULL || strcmp(inst->arg1, arg1) != 0)
                continue;
        }
        return true;
    }
    return false;
}

static bool
routine_has_stmt_call_named(const MIRRoutine *routine, const char *callee_name)
{
    if (routine == NULL || callee_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const ASTNode *ast = inst->ast;
            if (inst->kind != MIR_INST_STMT || ast == NULL || ast->type != AST_CALL)
                continue;
            if (ast->data.call.callee == NULL
                || ast->data.call.callee->type != AST_IDENTIFIER
                || ast->data.call.callee->data.identifier.name == NULL) {
                continue;
            }
            if (strcmp(ast->data.call.callee->data.identifier.name, callee_name) == 0)
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
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentParticipant", "payment", "PaymentZone")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentParticipant", "buyer", "Buyer")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentZoneWhere", "PaymentZone", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentZoneAlias", "payment", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentInvalidationTarget", "payment", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentWho", "buyer", "pay")
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block],
                   "IntentAuthorizedBy", "pay")
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

    TEST("MIR validator rejects cleanup block with normal CFG successor");
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
        MIRRoutine *purchase = NULL;
        char *mir_error = NULL;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_cleanup_block) {
            MIRBasicBlock *cleanup = &purchase->blocks[purchase->cleanup_block];
            cleanup->has_succ_true = true;
            cleanup->succ_true = purchase->entry_block;
        }
        rejected = ok
                   && purchase != NULL
                   && purchase->has_cleanup_block
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "cleanup block") != NULL
                   && strstr(mir_error, "normal CFG successors") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries pin-region cleanup edge metadata");
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_pin_block = false;
        bool found_pin_cleanup = false;
        bool found_after_pin_release = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinFlow", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                const MIRBasicBlock *block = &routine->blocks[i];
                if (block->is_pin_region) {
                    found_pin_block = true;
                    if (block->has_cleanup_succ
                        && block->cleanup_succ == routine->cleanup_block
                        && block_has_inst_named_with_slot(block, "pin-unpin-cleanup-edge", "scores")) {
                        found_pin_cleanup = true;
                    }
                } else if (block_has_inst_named_with_slot(block, "Release", "scores")) {
                    found_after_pin_release = true;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_cleanup_block
               && found_pin_block
               && found_pin_cleanup
               && found_after_pin_release);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pin-region without unpin cleanup fact");
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool corrupted = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "PinFlow", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count && !corrupted; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                for (size_t j = 0; j < block->instruction_count; j++) {
                    MIRInstruction *inst = &block->instructions[j];
                    if (inst->name != NULL
                        && strcmp(inst->name, "pin-unpin-cleanup-edge") == 0) {
                        inst->name = "corrupted-pin-cleanup-edge";
                        corrupted = true;
                        break;
                    }
                }
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pin-region") != NULL
                   && strstr(mir_error, "pin-unpin cleanup fact") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures intent dispatch and causes metadata");
    {
        const char *src =
            "subject Hero {\n"
            "    action Guard(self) -> Void { return; }\n"
            "}\n"
            "effect Guarded {\n"
            "    subject slot hero: Hero\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Hero\n"
            "    effect slot guarded: Guarded\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Hero) {\n"
            "    step Guard {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        who: hero;\n"
            "        causes: Guarded;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *patrol = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            patrol = find_mir_routine(mir, "Patrol", MIR_SCOPE_INTENT);
        EXPECT(ok && mir_validate(mir, NULL) && patrol != NULL);
        if (patrol != NULL) {
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentParticipant", "hero", "Hero"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentZoneWhere", "Arena", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentWho", "hero", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentDispatch", "hero", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentCauses", "Guarded", "Guard"));
        }
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
               && score_summary->ast_write_count > 1
               && score_summary->has_ast_reassignment
               && score_summary->used_outside_def_block
               && score_summary->crosses_block_boundary
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
        bool has_dead_phi = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            dead_merge = find_mir_routine(mir, "DeadMerge", MIR_SCOPE_FUNCTION);
        if (dead_merge != NULL)
            live_summary = find_value_summary_prefix_with_use(dead_merge, "live.");
        if (dead_merge != NULL) {
            for (size_t bi = 0; bi < dead_merge->block_count; bi++) {
                if (block_has_phi_result_prefix(&dead_merge->blocks[bi], "dead.")) {
                    has_dead_phi = true;
                    break;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && dead_merge != NULL
               && dead_merge->has_dce
               && dead_merge->dce_removed_count > 0
               && live_summary != NULL
               && !has_dead_phi);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR DCE removes dead pure-query statements while preserving routine validity");
    {
        const char *src =
            "func Probe(ch: Channel<Int>) -> Int {\n"
            "    ChannelLength(ch);\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *probe = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            probe = find_mir_routine(mir, "Probe", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && probe != NULL
               && probe->has_dce
               && probe->dce_removed_count > 0
               && !routine_has_stmt_call_named(probe, "ChannelLength"));
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
