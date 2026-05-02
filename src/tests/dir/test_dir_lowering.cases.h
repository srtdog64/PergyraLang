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
            "roster CommerceSystem {\n"
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
            "    roster commerce: CommerceSystem\n"
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
                     && step->where_type_name != NULL
                     && strcmp(step->where_type_name, "PaymentZone") == 0
                     && step->using_alias != NULL
                     && strcmp(step->using_alias, "payment") == 0
                     && step->transfer_from_alias != NULL
                     && strcmp(step->transfer_from_alias, "cart") == 0
                     && step->transfer_to_alias != NULL
                     && strcmp(step->transfer_to_alias, "payment") == 0
                     && step->where_derived_from_transfer
                     && step->using_derived_from_transfer
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

    TEST("DIR captures intent value bindings alongside participants");
    {
        const char *src =
            "subject Buyer { action Pay(self) -> Void { return; } }\n"
            "zone CheckoutZone { subject slot buyer: Buyer }\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer, price: Int, adjustments: Array<Int>) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: price > 0;\n"
            "        expect: ArrayLength(adjustments) == 2;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        bool has_price_edge = false;
        bool has_adjustments_edge = false;
        bool ok = false;

        if (dir != NULL) {
            for (size_t i = 0; i < dir->edge_count; i++) {
                const DIREdge *edge = &dir->edges[i];
                if (edge->kind == DIR_EDGE_INTENT_PARTICIPANT_TYPE
                    && edge->label != NULL
                    && strcmp(edge->label, "price") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Int") == 0) {
                    has_price_edge = true;
                }
                if (edge->kind == DIR_EDGE_INTENT_PARTICIPANT_TYPE
                    && edge->label != NULL
                    && strcmp(edge->label, "adjustments") == 0
                    && edge->target_name != NULL
                    && strcmp(edge->target_name, "Array<Int>") == 0) {
                    has_adjustments_edge = true;
                }
            }
        }

        if (dir != NULL && dir->intent_count == 1) {
            const DIRIntentInfo *intent = &dir->intents[0];
            if (intent->participant_count == 4 && intent->step_count == 1) {
                const DIRIntentParticipant *checkout = &intent->participants[0];
                const DIRIntentParticipant *buyer = &intent->participants[1];
                const DIRIntentParticipant *price = &intent->participants[2];
                const DIRIntentParticipant *adjustments = &intent->participants[3];

                ok = checkout->subject_type_name != NULL
                     && strcmp(checkout->subject_type_name, "CheckoutZone") == 0
                     && !checkout->is_value_binding
                     && checkout->subject_type_node_id != SIZE_MAX
                     && buyer->subject_type_name != NULL
                     && strcmp(buyer->subject_type_name, "Buyer") == 0
                     && !buyer->is_value_binding
                     && buyer->subject_type_node_id != SIZE_MAX
                     && price->subject_type_name != NULL
                     && strcmp(price->subject_type_name, "Int") == 0
                     && price->is_value_binding
                     && price->subject_type_node_id == SIZE_MAX
                     && adjustments->subject_type_name != NULL
                     && strcmp(adjustments->subject_type_name, "Array<Int>") == 0
                     && adjustments->is_value_binding
                     && adjustments->subject_type_node_id == SIZE_MAX
                     && has_price_edge
                     && has_adjustments_edge;
            }
        }

        EXPECT(ok && dir_validate(dir, NULL));
        dir_destroy(dir);
    }

    TEST("DIR preserves intent step using derivation provenance");
    {
        const char *src =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void within Arena authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Player\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Player) {\n"
            "    step Verify {\n"
            "        on: hero.Guard();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        DIRProgram *dir = lower_dir_from_source(src);
        bool ok = false;

        if (dir != NULL && dir->intent_count == 1) {
            const DIRIntentInfo *intent = &dir->intents[0];
            if (intent->step_count == 1) {
                const DIRIntentStep *step = &intent->steps[0];
                ok = step->where_type_name != NULL
                     && strcmp(step->where_type_name, "Arena") == 0
                     && step->using_alias != NULL
                     && strcmp(step->using_alias, "arena") == 0
                     && step->where_inherited_from_action
                     && step->using_derived_from_where
                     && step->who_derived_from_on_receiver
                     && step->authorized_by_inherited_from_action;
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
