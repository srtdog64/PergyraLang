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
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && flow != NULL
               && flow->block_count >= 1
               && flow->instruction_count >= 4
               && block_has_inst_named(&flow->blocks[flow->entry_block], "Claim")
               && block_has_inst_named(&flow->blocks[flow->entry_block], "Write")
               && block_has_inst_named(&flow->blocks[flow->entry_block], "Read")
               && block_has_inst_named(&flow->blocks[flow->entry_block], "Release"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR builds cleanup block for intent compensation");
    {
        const char *src =
            "subject Buyer { action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone { subject slot buyer: Buyer authority buyer requires Payable }\n"
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
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_cleanup_block) {
            const MIRBasicBlock *cleanup = &purchase->blocks[purchase->cleanup_block];
            cleanup_has_compensate = block_has_inst_kind(cleanup, MIR_INST_CLEANUP_EDGE)
                                     && block_has_inst_named(cleanup, "CompensateIntentStep");
        }
        EXPECT(ok
               && purchase != NULL
               && purchase->has_cleanup_block
               && purchase->cleanup_instruction_count >= 1
               && cleanup_has_compensate);
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
