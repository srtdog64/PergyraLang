/*
 * Copyright (c) 2026 Pergyra Language Project
 * RIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"
#include "compiler/rir.h"

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
lower_rir_from_source(const char *source, HIRProgram **hir_out, RIRProgram **rir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    char *rir_error = NULL;
    *hir_out = NULL;
    *rir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
    }

    if ((*hir_out == NULL || *rir_out == NULL) && (hir_error != NULL || rir_error != NULL)) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
    }

    free(hir_error);
    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return *hir_out != NULL && *rir_out != NULL;
}

static bool
scope_has_op(const RIRScope *scope, RIROpKind kind)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind)
            return true;
    }
    return false;
}

static bool
scope_has_op_subject(const RIRScope *scope, RIROpKind kind, const char *subject)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind
            && scope->ops[i].subject != NULL
            && strcmp(scope->ops[i].subject, subject) == 0)
            return true;
    }
    return false;
}

static bool
scope_has_resource_fact(const RIRScope *scope, const char *name, RIRResourceKind kind)
{
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_RESOURCE
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->resource_kind == kind) {
            return true;
        }
    }
    return false;
}

static const RIRStateSummary *
scope_find_state_summary(const RIRScope *scope, const char *name)
{
    if (scope == NULL)
        return NULL;
    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return &scope->state_summaries[i];
        }
    }
    return NULL;
}

static const RIRScope *
find_scope(const RIRProgram *rir, const char *name, RIRScopeKind kind)
{
    if (rir == NULL)
        return NULL;
    for (size_t i = 0; i < rir->scope_count; i++) {
        if (rir->scopes[i].kind == kind
            && rir->scopes[i].name != NULL
            && strcmp(rir->scopes[i].name, name) == 0) {
            return &rir->scopes[i];
        }
    }
    return NULL;
}

static void
test_rir_lowering(void)
{
    printf("\n[rir]\n");

    TEST("RIR captures slot claim/read/write/release in function scope");
    {
        HIRProgram *hir = NULL;
        const char *src =
            "func Flow() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(s, 1);\n"
            "    let v = Read(s);\n"
            "    Release(s);\n"
            "}\n";
        RIRProgram *rir = NULL;
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "Flow", RIR_SCOPE_FUNCTION);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_resource_fact(flow, "s", RIR_RESOURCE_LOCAL_SLOT)
               && scope_has_op(flow, RIR_OP_CLAIM)
               && scope_has_op(flow, RIR_OP_WRITE)
               && scope_has_op(flow, RIR_OP_READ)
               && scope_has_op(flow, RIR_OP_RELEASE));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR captures projection, authority, lifecycle, and intent compensation");
    {
        HIRProgram *hir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object BuyerView { let hp: Int; }\n"
            "dto BuyerDto { let hp: Int; }\n"
            "relation CartLink for source: Buyer, target: Buyer { }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    dto slot packet: BuyerDto\n"
            "    relation slot cart: CartLink\n"
            "    effect slot paymentFx: PaymentEffect\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "    publish packet from buyer by buyer\n"
            "    apply paymentFx to buyer by buyer\n"
            "    link cart between buyer, buyer by buyer\n"
            "    detach paymentFx from buyer by buyer\n"
            "    unlink cart between buyer, buyer by buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    rollback: full;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        on: buyer.Pay();\n"
            "        compensate: buyer.Pay();\n"
            "    }\n"
            "}\n";
        RIRProgram *rir = NULL;
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *zone = find_scope(rir, "PaymentZone", RIR_SCOPE_ZONE);
        const RIRScope *intent = find_scope(rir, "Purchase", RIR_SCOPE_INTENT);
        const RIRStateSummary *payment_fx = scope_find_state_summary(zone, "paymentFx");
        const RIRStateSummary *cart = scope_find_state_summary(zone, "cart");
        bool zone_ok = zone != NULL
                       && scope_has_op(zone, RIR_OP_PROJECT_REFRESH)
                       && scope_has_op(zone, RIR_OP_PROJECT_PUBLISH)
                       && scope_has_op(zone, RIR_OP_ATTACH_EFFECT)
                       && scope_has_op(zone, RIR_OP_LINK_RELATION)
                       && scope_has_op(zone, RIR_OP_DETACH_EFFECT)
                       && scope_has_op(zone, RIR_OP_UNLINK_RELATION)
                       && payment_fx != NULL
                       && payment_fx->resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
                       && payment_fx->initial_state == RIR_STATE_DETACHED
                       && payment_fx->final_state == RIR_STATE_DETACHED
                       && cart != NULL
                       && cart->resource_kind == RIR_RESOURCE_RELATION_INSTANCE
                       && cart->initial_state == RIR_STATE_DETACHED
                       && cart->final_state == RIR_STATE_DETACHED;
        bool intent_ok = intent != NULL
                         && scope_has_op(intent, RIR_OP_AUTHORIZE)
                         && scope_has_op(intent, RIR_OP_COMPENSATE_INTENT_STEP)
                         && scope_has_op(intent, RIR_OP_ABORT_INTENT)
                         && scope_has_op(intent, RIR_OP_COMMIT_INTENT);
        EXPECT(ok && rir_validate(rir, NULL) && zone_ok && intent_ok);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR normalizes final resource state for valid linear flow");
    {
        HIRProgram *hir = NULL;
        const char *src =
            "func FlowEnd() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Read(s);\n"
            "    Release(s);\n"
            "}\n";
        RIRProgram *rir = NULL;
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "FlowEnd", RIR_SCOPE_FUNCTION);
        const RIRStateSummary *summary = scope_find_state_summary(flow, "s");
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && !flow->has_state_errors
               && summary != NULL
               && summary->initial_state == RIR_STATE_OWNED
               && summary->final_state == RIR_STATE_RELEASED
               && !summary->has_transition_error);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR enriches branch join with conservative flow merge facts");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "func MergeSlot(flag: Bool) -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    if flag {\n"
            "        let rv: ReadView<Int> = ViewRead(s);\n"
            "    } else {\n"
            "        let wv: WriteView<Int> = ViewWrite(s);\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "MergeSlot", RIR_SCOPE_FUNCTION);
        bool found_join_merge = false;
        if (flow != NULL) {
            for (size_t i = 0; i < flow->flow_block_count; i++) {
                const RIRFlowBlock *block = &flow->flow_blocks[i];
                if (!block->is_join)
                    continue;
                for (size_t j = 0; j < block->fact_count; j++) {
                    const RIRFlowFact *fact = &block->facts[j];
                    if (fact->name != NULL
                        && strcmp(fact->name, "s") == 0
                        && fact->merged_from_join
                        && !fact->has_merge_conflict
                        && fact->entry_state == RIR_STATE_BORROWED_WRITE) {
                        found_join_merge = true;
                    }
                }
            }
        }
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && flow->flow_block_count > 0
               && flow->has_flow_sensitive_merge
               && found_join_merge);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR marks loop-header joins as widened flow states");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "func LoopSlot(flag: Bool) -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    while flag {\n"
            "        let rv: ReadView<Int> = ViewRead(s);\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "LoopSlot", RIR_SCOPE_FUNCTION);
        bool found_widened = false;
        if (flow != NULL) {
            for (size_t i = 0; i < flow->flow_block_count; i++) {
                const RIRFlowBlock *block = &flow->flow_blocks[i];
                for (size_t j = 0; j < block->fact_count; j++) {
                    const RIRFlowFact *fact = &block->facts[j];
                    if (fact->name != NULL
                        && strcmp(fact->name, "s") == 0
                        && fact->widened_by_loop) {
                        found_widened = true;
                    }
                }
            }
        }
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && flow->has_flow_sensitive_merge
               && found_widened);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR materializes zone/world/relation/effect handles and intent transfer ops");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "relation CartLink for source: Buyer, target: Buyer { }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone PaymentZone { subject slot buyer: Buyer }\n"
            "world CommerceWorld { zone payment: PaymentZone activate payment }\n"
            "func HandleInfra(payment: PaymentZone, commerce: CommerceWorld, link: CartLink, fx: PaymentEffect) -> Void {\n"
            "    return;\n"
            "}\n"
            "intent Route(payment: PaymentZone, refund: PaymentZone, buyer: Buyer) {\n"
            "    step move {\n"
            "        where: PaymentZone;\n"
            "        who: buyer;\n"
            "        transfer: payment -> refund;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "    step settle {\n"
            "        where: PaymentZone;\n"
            "        using: refund;\n"
            "        who: buyer;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *infra = find_scope(rir, "HandleInfra", RIR_SCOPE_FUNCTION);
        const RIRScope *intent = find_scope(rir, "Route", RIR_SCOPE_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && infra != NULL
               && scope_has_resource_fact(infra, "payment", RIR_RESOURCE_ZONE_HANDLE)
               && scope_has_resource_fact(infra, "commerce", RIR_RESOURCE_WORLD_HANDLE)
               && scope_has_resource_fact(infra, "link", RIR_RESOURCE_RELATION_INSTANCE)
               && scope_has_resource_fact(infra, "fx", RIR_RESOURCE_EFFECT_INSTANCE)
               && intent != NULL
               && scope_has_resource_fact(intent, "payment", RIR_RESOURCE_ZONE_HANDLE)
               && scope_has_resource_fact(intent, "refund", RIR_RESOURCE_ZONE_HANDLE)
               && scope_has_op_subject(intent, RIR_OP_READ, "refund")
               && scope_has_op_subject(intent, RIR_OP_MOVE, "payment")
               && scope_has_op_subject(intent, RIR_OP_CLAIM, "refund"));
        rir_destroy(rir);
        hir_destroy(hir);
    }
}

int
main(void)
{
    printf("=== Pergyra RIR Lowering Test Suite ===\n");
    test_rir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
