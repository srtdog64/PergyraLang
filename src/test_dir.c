/*
 * Copyright (c) 2026 Pergyra Language Project
 * DIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/dir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static DIRProgram *
lower_dir_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *dir_error = NULL;
    DIRProgram *dir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        dir = dir_lower(sem->annotated_ast, &dir_error);

    if (dir == NULL && dir_error != NULL)
        fprintf(stderr, "DIR lowering error: %s\n", dir_error);

    free(dir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return dir;
}

static bool
dir_has_node(const DIRProgram *dir, DIRNodeKind kind, const char *name)
{
    if (dir == NULL || name == NULL)
        return false;

    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].kind == kind
            && dir->nodes[i].name != NULL
            && strcmp(dir->nodes[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
dir_has_edge(const DIRProgram *dir,
             DIREdgeKind kind,
             size_t from_kind_filter,
             size_t to_kind_filter,
             const char *label,
             const char *target_name)
{
    if (dir == NULL)
        return false;

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->kind != kind)
            continue;
        if (from_kind_filter != SIZE_MAX
            && dir->nodes[edge->from_node_id].kind != (DIRNodeKind)from_kind_filter)
            continue;
        if (to_kind_filter != SIZE_MAX) {
            if (edge->to_node_id == SIZE_MAX)
                continue;
            if (dir->nodes[edge->to_node_id].kind != (DIRNodeKind)to_kind_filter)
                continue;
        }
        if (label != NULL) {
            if (edge->label == NULL || strcmp(edge->label, label) != 0)
                continue;
        }
        if (target_name != NULL) {
            if (edge->target_name == NULL || strcmp(edge->target_name, target_name) != 0)
                continue;
        }
        return true;
    }

    return false;
}

static void
test_dir_lowering(void)
{
    printf("\n[dir]\n");

    TEST("DIR builds domain graph and intent metadata");
    {
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "subject Merchant { let trust: Int; }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "role MerchantRole for Merchant {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "party ShopParty {\n"
            "    role slot seller: Payable\n"
            "}\n"
            "systemic CommerceSystem {\n"
            "    party slot staff: ShopParty\n"
            "}\n"
            "relation CartLink for source: Buyer, target: Merchant { }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    subject slot seller: Merchant\n"
            "    relation slot cart: CartLink\n"
            "    effect slot paymentFx: PaymentEffect\n"
            "    authority buyer requires Payable\n"
            "    state charged: effect paymentFx on buyer\n"
            "}\n"
            "world CommerceWorld {\n"
            "    systemic commerce: CommerceSystem\n"
            "    zone payment: PaymentZone\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer, seller: Merchant) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        causes: PaymentEffect;\n"
            "    }\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        bool has_role_impl = false;
        bool has_party_slot = false;
        bool has_world_zone = false;
        bool has_zone_layer = false;
        bool has_zone_auth = false;
        bool has_intent_step = false;
        bool has_intent_participant_edge = false;
        bool has_intent_zone_edge = false;
        bool has_intent_requires_edge = false;
        bool has_intent_authorized_edge = false;
        bool has_intent_causes_edge = false;
        bool has_role_complete_edge = false;

        if (dir != NULL) {
            for (size_t i = 0; i < dir->edge_count; i++) {
                const DIREdge *edge = &dir->edges[i];
                if (edge->kind == DIR_EDGE_ROLE_IMPL_ABILITY
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Payable") == 0) {
                    has_role_impl = true;
                }
                if (edge->kind == DIR_EDGE_PARTY_SLOT_ABILITY
                    && edge->label != NULL
                    && strcmp(edge->label, "seller") == 0) {
                    has_party_slot = true;
                }
                if (edge->kind == DIR_EDGE_WORLD_ZONE
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "PaymentZone") == 0) {
                    has_world_zone = true;
                }
                if (edge->kind == DIR_EDGE_ZONE_LAYER_TYPE
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "PaymentEffect") == 0) {
                    has_zone_layer = true;
                }
                if (edge->kind == DIR_EDGE_ZONE_AUTHORITY_ABILITY
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Payable") == 0) {
                    has_zone_auth = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_PARTICIPANT_TYPE
                    && edge->label != NULL
                    && strcmp(edge->label, "buyer") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Buyer") == 0) {
                    has_intent_participant_edge = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_STEP_ZONE
                    && edge->label != NULL
                    && strcmp(edge->label, "pay") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "PaymentZone") == 0) {
                    has_intent_zone_edge = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_STEP_REQUIRES
                    && edge->label != NULL
                    && strcmp(edge->label, "pay") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Payable") == 0) {
                    has_intent_requires_edge = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_STEP_AUTHORIZED_BY
                    && edge->label != NULL
                    && strcmp(edge->label, "pay") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "buyer") == 0) {
                    has_intent_authorized_edge = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_STEP_CAUSES
                    && edge->label != NULL
                    && strcmp(edge->label, "pay") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "PaymentEffect") == 0) {
                    has_intent_causes_edge = true;
                }
                if (edge->kind == DIR_EDGE_ROLE_COMPLETES_ABILITY
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Payable") == 0) {
                    has_role_complete_edge = true;
                }
            }

            if (dir->intent_count == 1) {
                const DIRIntentInfo *intent = &dir->intents[0];
                if (intent->participant_count == 3 && intent->step_count == 1) {
                    const DIRIntentStep *step = &intent->steps[0];
                    has_intent_step =
                        step->index == 0
                        &&
                        step->where_type_name != NULL
                        && strcmp(step->where_type_name, "PaymentZone") == 0
                        && step->using_alias != NULL
                        && strcmp(step->using_alias, "payment") == 0
                        && step->causes_effect_name != NULL
                        && strcmp(step->causes_effect_name, "PaymentEffect") == 0
                        && step->who_count == 1
                        && strcmp(step->who_names[0], "buyer") == 0
                        && step->required_ability_count == 1
                        && strcmp(step->required_abilities[0], "Payable") == 0;
                }
            }
        }

        EXPECT(dir != NULL
               && dir_validate(dir, NULL)
               && dir->node_count >= 9
               && dir->edge_count >= 6
               && dir->intent_count == 1
               && has_role_impl
               && has_party_slot
               && has_world_zone
               && has_zone_layer
               && has_zone_auth
               && has_role_complete_edge
               && has_intent_participant_edge
               && has_intent_zone_edge
               && has_intent_requires_edge
               && has_intent_authorized_edge
               && has_intent_causes_edge
               && has_intent_step);
        dir_destroy(dir);
    }

    TEST("DIR captures intent transfer and zone parameter participants");
    {
        const char *src =
            "subject Customer { action Checkout(self) -> Void { return; } }\n"
            "zone CartZone { subject slot customer: Customer }\n"
            "zone PaymentZone { subject slot customer: Customer }\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Customer) {\n"
            "    step move_to_payment {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        transfer: cart -> payment;\n"
            "    }\n"
            "    step charge {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "    }\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        bool ok = false;
        bool has_dep_edge = false;

        if (dir != NULL) {
            for (size_t i = 0; i < dir->edge_count; i++) {
                const DIREdge *edge = &dir->edges[i];
                if (edge->kind == DIR_EDGE_INTENT_STEP_DEPENDS_ON
                    && edge->label != NULL
                    && strcmp(edge->label, "move_to_payment") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "charge") == 0) {
                    has_dep_edge = true;
                }
            }
        }

        if (dir != NULL && dir->intent_count == 1) {
            const DIRIntentInfo *intent = &dir->intents[0];
            if (intent->participant_count == 3 && intent->step_count == 2) {
                const DIRIntentParticipant *cart = &intent->participants[0];
                const DIRIntentParticipant *payment = &intent->participants[1];
                const DIRIntentParticipant *buyer = &intent->participants[2];
                const DIRIntentStep *step = &intent->steps[0];
                const DIRIntentStep *step2 = &intent->steps[1];

                ok = cart->subject_type_name != NULL
                     && strcmp(cart->subject_type_name, "CartZone") == 0
                     && cart->subject_type_node_id != SIZE_MAX
                     && payment->subject_type_name != NULL
                     && strcmp(payment->subject_type_name, "PaymentZone") == 0
                     && payment->subject_type_node_id != SIZE_MAX
                     && buyer->subject_type_name != NULL
                     && strcmp(buyer->subject_type_name, "Customer") == 0
                     && buyer->subject_type_node_id != SIZE_MAX
                     && step->transfer_from_alias != NULL
                     && strcmp(step->transfer_from_alias, "cart") == 0
                     && step->transfer_to_alias != NULL
                     && strcmp(step->transfer_to_alias, "payment") == 0
                     && step->where_type_node_id != SIZE_MAX
                     && step2->predecessor_step_name != NULL
                     && strcmp(step2->predecessor_step_name, "move_to_payment") == 0
                     && step2->predecessor_step_index == 0
                     && has_dep_edge;
            }
        }

        EXPECT(ok && dir_validate(dir, NULL));
        dir_destroy(dir);
    }

    TEST("DIR reports missing ability methods on incomplete role impl");
    {
        const char *src =
            "subject Buyer { action Pay(self) -> Void { return; } }\n"
            "ability Payable {\n"
            "    func Pay() -> Void;\n"
            "    func Audit() -> Void;\n"
            "}\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable {\n"
            "        func Pay() -> Void { return; }\n"
            "    }\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        bool has_missing = false;
        if (dir != NULL) {
            for (size_t i = 0; i < dir->edge_count; i++) {
                const DIREdge *edge = &dir->edges[i];
                if (edge->kind == DIR_EDGE_ROLE_MISSING_ABILITY_METHOD
                    && edge->label != NULL
                    && strcmp(edge->label, "Payable") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Audit") == 0) {
                    has_missing = true;
                }
            }
        }
        EXPECT(dir != NULL && dir_validate(dir, NULL) && has_missing);
        dir_destroy(dir);
    }

    TEST("DIR materializes party, zone, projection, and authority slot contracts");
    {
        const char *src =
            "subject Buyer { let total: Int; }\n"
            "object ReceiptView { let total: Int; }\n"
            "tobject ReceiptExport { let total: Int; }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "party ShopParty {\n"
            "    role slot seller: Payable\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot preview: ReceiptView\n"
            "    tobject slot receipt_out: ReceiptExport\n"
            "    authority buyer requires Payable\n"
            "    refresh preview from buyer by buyer;\n"
            "    publish receipt_out from buyer by buyer;\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        EXPECT(dir != NULL
               && dir_validate(dir, NULL)
               && dir_has_node(dir, DIR_NODE_PARTY_SLOT, "ShopParty.seller")
               && dir_has_node(dir, DIR_NODE_ZONE_SLOT, "PaymentZone.buyer")
               && dir_has_node(dir, DIR_NODE_PROJECTION_SLOT, "PaymentZone.preview")
               && dir_has_node(dir, DIR_NODE_PROJECTION_SLOT, "PaymentZone.receipt_out")
               && dir_has_node(dir, DIR_NODE_AUTHORITY_SLOT, "PaymentZone.buyer")
               && dir_has_edge(dir,
                               DIR_EDGE_PARTY_HAS_SLOT,
                               DIR_NODE_PARTY,
                               DIR_NODE_PARTY_SLOT,
                               "seller",
                               "ShopParty.seller")
               && dir_has_edge(dir,
                               DIR_EDGE_ZONE_HAS_SLOT,
                               DIR_NODE_ZONE,
                               DIR_NODE_ZONE_SLOT,
                               "buyer",
                               "PaymentZone.buyer")
               && dir_has_edge(dir,
                               DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                               DIR_NODE_ZONE,
                               DIR_NODE_PROJECTION_SLOT,
                               "preview",
                               "PaymentZone.preview")
               && dir_has_edge(dir,
                               DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                               DIR_NODE_ZONE,
                               DIR_NODE_PROJECTION_SLOT,
                               "receipt_out",
                               "PaymentZone.receipt_out")
               && dir_has_edge(dir,
                               DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT,
                               DIR_NODE_ZONE,
                               DIR_NODE_AUTHORITY_SLOT,
                               "buyer",
                               "PaymentZone.buyer"));
        dir_destroy(dir);
    }

    TEST("DIR projection slot contracts carry type and source edges");
    {
        const char *src =
            "subject Buyer { let total: Int; }\n"
            "object ReceiptView { let total: Int; }\n"
            "tobject ReceiptExport { let total: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot preview: ReceiptView\n"
            "    tobject slot receipt_out: ReceiptExport\n"
            "    refresh preview from buyer by buyer;\n"
            "    publish receipt_out from preview by buyer;\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        EXPECT(dir != NULL
               && dir_validate(dir, NULL)
               && dir_has_edge(dir,
                               DIR_EDGE_PROJECTION_SLOT_TYPE,
                               DIR_NODE_PROJECTION_SLOT,
                               DIR_NODE_TYPE,
                               "preview",
                               "ReceiptView")
               && dir_has_edge(dir,
                               DIR_EDGE_PROJECTION_SLOT_TYPE,
                               DIR_NODE_PROJECTION_SLOT,
                               DIR_NODE_TYPE,
                               "receipt_out",
                               "ReceiptExport")
               && dir_has_edge(dir,
                               DIR_EDGE_PROJECTION_SLOT_SOURCE,
                               DIR_NODE_PROJECTION_SLOT,
                               DIR_NODE_ZONE_SLOT,
                               "refresh",
                               "buyer")
               && dir_has_edge(dir,
                               DIR_EDGE_PROJECTION_SLOT_SOURCE,
                               DIR_NODE_PROJECTION_SLOT,
                               DIR_NODE_PROJECTION_SLOT,
                               "publish",
                               "preview"));
        dir_destroy(dir);
    }

    TEST("DIR authority slot contracts bind subject slot and abilities");
    {
        const char *src =
            "subject Buyer { let total: Int; }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "ability Refundable { func Refund() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "    impl ability Refundable { func Refund() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable, Refundable\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        EXPECT(dir != NULL
               && dir_validate(dir, NULL)
               && dir_has_edge(dir,
                               DIR_EDGE_AUTHORITY_SLOT_SUBJECT,
                               DIR_NODE_AUTHORITY_SLOT,
                               DIR_NODE_ZONE_SLOT,
                               "buyer",
                               "buyer")
               && dir_has_edge(dir,
                               DIR_EDGE_ZONE_AUTHORITY_ABILITY,
                               DIR_NODE_AUTHORITY_SLOT,
                               DIR_NODE_ABILITY,
                               "buyer",
                               "Payable")
               && dir_has_edge(dir,
                               DIR_EDGE_ZONE_AUTHORITY_ABILITY,
                               DIR_NODE_AUTHORITY_SLOT,
                               DIR_NODE_ABILITY,
                               "buyer",
                               "Refundable"));
        dir_destroy(dir);
    }

    TEST("DIR relation and effect declarations keep projection contracts distinct");
    {
        const char *src =
            "subject Buyer { let total: Int; }\n"
            "object ReceiptView { let total: Int; }\n"
            "tobject ReceiptExport { let total: Int; }\n"
            "relation CartLink between subject, subject {\n"
            "    subject slot owner: Buyer\n"
            "    object slot summary: ReceiptView\n"
            "    refresh summary from owner;\n"
            "}\n"
            "effect PaymentFx {\n"
            "    subject slot bearer: Buyer\n"
            "    tobject slot receipt_out: ReceiptExport\n"
            "    publish receipt_out from bearer;\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        EXPECT(dir != NULL
               && dir_validate(dir, NULL)
               && dir_has_node(dir, DIR_NODE_PROJECTION_SLOT, "CartLink.summary")
               && dir_has_node(dir, DIR_NODE_PROJECTION_SLOT, "PaymentFx.receipt_out")
               && dir_has_edge(dir,
                               DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                               DIR_NODE_RELATION,
                               DIR_NODE_PROJECTION_SLOT,
                               "summary",
                               "CartLink.summary")
               && dir_has_edge(dir,
                               DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                               DIR_NODE_EFFECT,
                               DIR_NODE_PROJECTION_SLOT,
                               "receipt_out",
                               "PaymentFx.receipt_out"));
        dir_destroy(dir);
    }
}

int
main(void)
{
    printf("=== Pergyra DIR Lowering Test Suite ===\n");
    test_dir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
