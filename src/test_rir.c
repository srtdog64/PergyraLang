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

static RIRProgram *
lower_rir_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *rir_error = NULL;
    RIRProgram *rir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        rir = rir_lower(sem->annotated_ast, &rir_error);

    if (rir == NULL && rir_error != NULL)
        fprintf(stderr, "RIR lowering error: %s\n", rir_error);

    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return rir;
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
        const char *src =
            "func Flow() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(s, 1);\n"
            "    let v = Read(s);\n"
            "    Release(s);\n"
            "}\n";
        RIRProgram *rir = lower_rir_from_source(src);
        const RIRScope *flow = find_scope(rir, "Flow", RIR_SCOPE_FUNCTION);
        EXPECT(flow != NULL
               && scope_has_resource_fact(flow, "s", RIR_RESOURCE_LOCAL_SLOT)
               && scope_has_op(flow, RIR_OP_CLAIM)
               && scope_has_op(flow, RIR_OP_WRITE)
               && scope_has_op(flow, RIR_OP_READ)
               && scope_has_op(flow, RIR_OP_RELEASE));
        rir_destroy(rir);
    }

    TEST("RIR captures projection, authority, lifecycle, and intent compensation");
    {
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
        RIRProgram *rir = lower_rir_from_source(src);
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
        EXPECT(zone_ok && intent_ok);
        rir_destroy(rir);
    }

    TEST("RIR normalizes final resource state for valid linear flow");
    {
        const char *src =
            "func FlowEnd() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Read(s);\n"
            "    Release(s);\n"
            "}\n";
        RIRProgram *rir = lower_rir_from_source(src);
        const RIRScope *flow = find_scope(rir, "FlowEnd", RIR_SCOPE_FUNCTION);
        const RIRStateSummary *summary = scope_find_state_summary(flow, "s");
        EXPECT(flow != NULL
               && !flow->has_state_errors
               && summary != NULL
               && summary->initial_state == RIR_STATE_OWNED
               && summary->final_state == RIR_STATE_RELEASED
               && !summary->has_transition_error);
        rir_destroy(rir);
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
