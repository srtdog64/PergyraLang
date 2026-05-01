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
               && scope_has_fact_slot_anchor(flow, RIR_FACT_RESOURCE, "s", "s")
               && scope_has_op(flow, RIR_OP_CLAIM)
               && scope_has_op(flow, RIR_OP_WRITE)
               && scope_has_op(flow, RIR_OP_READ)
               && scope_has_op(flow, RIR_OP_RELEASE));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR materializes source channel send recv and select ops");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "func ChannelOps(ch: Channel<Int>) -> Int {\n"
            "    ch <- 1;\n"
            "    let first: Int = <- ch;\n"
            "    select {\n"
            "        case next = <-ch:\n"
            "            return next;\n"
            "        default:\n"
            "            return first;\n"
            "    }\n"
            "    return first;\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "ChannelOps", RIR_SCOPE_FUNCTION);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_op_subject(flow, RIR_OP_CHANNEL_SEND, "ch")
               && scope_has_op_subject(flow, RIR_OP_CHANNEL_RECV, "ch")
               && scope_has_op_subject(flow, RIR_OP_CHANNEL_SELECT, "select"));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR materializes stable IO calls as boundary ops");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "func Load() -> Void {\n"
            "    ReadFile(\"settings.txt\");\n"
            "    Sleep(1);\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "Load", RIR_SCOPE_FUNCTION);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_op_subject(flow, RIR_OP_IO, "ReadFile")
               && scope_has_op_subject(flow, RIR_OP_IO, "Sleep"));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR materializes parallel async and spawn boundary ops");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "func Worker() -> Int { return 1; }\n"
            "func ParallelOps() -> Void {\n"
            "    let f: Future<Int> = spawn Worker();\n"
            "    async { Log(1); }\n"
            "    parallel {\n"
            "        Log(2);\n"
            "        Log(3);\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "ParallelOps", RIR_SCOPE_FUNCTION);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_op_subject(flow, RIR_OP_SPAWN, "spawn")
               && scope_has_op_subject(flow, RIR_OP_ASYNC, "async")
               && scope_has_op_subject(flow, RIR_OP_PARALLEL, "parallel"));
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
            "tobject BuyerDto { let hp: Int; }\n"
            "relation CartLink for source: Buyer, target: Buyer { }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    tobject slot packet: BuyerDto\n"
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
                       && scope_has_fact_slot_anchor(zone, RIR_FACT_AUTHORITY, "buyer", "buyer")
                       && scope_has_fact_slot_anchor(zone, RIR_FACT_CAPABILITY, "buyer", "buyer")
                       && scope_has_fact_slot_anchor(zone, RIR_FACT_PROJECTION, "view", "view")
                       && scope_has_fact_slot_anchor(zone, RIR_FACT_PROJECTION, "packet", "packet")
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
                         && scope_has_fact_slot_anchor(intent, RIR_FACT_RESOURCE, "payment", "payment")
                         && scope_has_op_slot_anchor(intent, RIR_OP_READ, "payment")
                         && scope_has_op_slot_anchor(intent, RIR_OP_AUTHORIZE, "buyer")
                         && scope_has_op(intent, RIR_OP_AUTHORIZE)
                         && scope_has_op(intent, RIR_OP_COMPENSATE_INTENT_STEP)
                         && scope_has_op(intent, RIR_OP_ABORT_INTENT)
                         && scope_has_op(intent, RIR_OP_COMMIT_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && zone_ok
               && intent_ok
               && scope_has_conservative_semantics(zone,
                                                   RIR_FLOW_PROJECTION
                                                       | RIR_FLOW_INVALIDATION
                                                       | RIR_FLOW_PROJECTION_INVALIDATION)
               && scope_has_conservative_semantics(intent,
                                                   RIR_FLOW_AUTHORITY
                                                       | RIR_FLOW_INVALIDATION
                                                       | RIR_FLOW_AUTHORITY_LOSS));
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

    TEST("RIR tracks invalidation semantics across projection and transfer flow");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "object BuyerView { let hp: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "world CommerceWorld { zone pay: PaymentZone }\n"
            "intent Shift(pay: PaymentZone, buyer: Buyer) {\n"
            "    step step1 {\n"
            "        where: PaymentZone;\n"
            "        using: pay;\n"
            "        who: buyer;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *zone = find_scope(rir, "PaymentZone", RIR_SCOPE_ZONE);
        const RIRScope *intent = find_scope(rir, "Shift", RIR_SCOPE_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && zone != NULL
               && intent != NULL
               && scope_has_conservative_semantics(zone, RIR_FLOW_PROJECTION)
               && scope_has_conservative_semantics(intent, RIR_FLOW_INVALIDATION));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR keeps embedded world-zone handles visible while zone projection remains conservative");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; }\n"
            "object BuyerView { let hp: Int; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "world CommerceWorld {\n"
            "    zone checkout: CheckoutZone\n"
            "    activate checkout\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *zone = find_scope(rir, "CheckoutZone", RIR_SCOPE_ZONE);
        const RIRScope *world = find_scope(rir, "CommerceWorld", RIR_SCOPE_WORLD);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && zone != NULL
               && world != NULL
               && scope_has_fact_slot_anchor(zone, RIR_FACT_PROJECTION, "view", "view")
               && scope_has_conservative_semantics(zone, RIR_FLOW_PROJECTION)
               && scope_has_resource_fact(world, "checkout", RIR_RESOURCE_ZONE_HANDLE));
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

    TEST("RIR flow tracks projection semantics across joins and handoff semantics conservatively");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "object BuyerView { let hp: Int; }\n"
            "tobject BuyerDto { let hp: Int; }\n"
            "zone PaymentZone { subject slot buyer: Buyer }\n"
            "func MergeProjection(flag: Bool, buyer: Buyer) -> Void {\n"
            "    if flag {\n"
            "        ToObject(BuyerView, buyer);\n"
            "    } else {\n"
            "        ToTObject(BuyerDto, buyer);\n"
            "    }\n"
            "}\n"
            "intent Route(payment: PaymentZone, refund: PaymentZone, buyer: Buyer) {\n"
            "    step move {\n"
            "        where: PaymentZone;\n"
            "        who: buyer;\n"
            "        transfer: payment -> refund;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "MergeProjection", RIR_SCOPE_FUNCTION);
        const RIRScope *intent = find_scope(rir, "Route", RIR_SCOPE_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && flow->has_flow_sensitive_merge
               && scope_has_flow_semantics(flow, RIR_FLOW_PROJECTION)
               && intent != NULL
               && scope_has_conservative_semantics(intent,
                                                  RIR_FLOW_WORLD_HANDOFF
                                                      | RIR_FLOW_INVALIDATION));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR derives authority-loss and projection-invalidation semantics conservatively");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer { impl ability Payable { func Pay() -> Void { return; } } }\n"
            "object BuyerView { let hp: Int; }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    effect slot paymentFx: PaymentEffect\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "    detach paymentFx from buyer by buyer\n"
            "}\n"
            "intent Route(payment: PaymentZone, refund: PaymentZone, buyer: Buyer) {\n"
            "    step move {\n"
            "        where: PaymentZone;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        transfer: payment -> refund;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *zone = find_scope(rir, "PaymentZone", RIR_SCOPE_ZONE);
        const RIRScope *intent = find_scope(rir, "Route", RIR_SCOPE_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && zone != NULL
               && intent != NULL
               && scope_has_conservative_semantics(zone,
                                                  RIR_FLOW_PROJECTION
                                                      | RIR_FLOW_INVALIDATION
                                                      | RIR_FLOW_PROJECTION_INVALIDATION)
               && scope_has_conservative_semantics(intent,
                                                  RIR_FLOW_AUTHORITY
                                                      | RIR_FLOW_WORLD_HANDOFF
                                                      | RIR_FLOW_INVALIDATION
                                                      | RIR_FLOW_AUTHORITY_LOSS));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR treats slots as the common anchor across projection authority and handoff");
    {
        HIRProgram *hir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; }\n"
            "object BuyerView { let hp: Int; }\n"
            "effect PaymentEffect for bearer: Buyer { }\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    effect slot paymentFx: PaymentEffect\n"
            "    authority buyer\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        transfer: cart -> payment;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        RIRProgram *rir = NULL;
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *cart = find_scope(rir, "CartZone", RIR_SCOPE_ZONE);
        const RIRScope *checkout = find_scope(rir, "Checkout", RIR_SCOPE_INTENT);
        EXPECT(ok
               && rir_validate(rir, NULL)
               && cart != NULL
               && checkout != NULL
               && scope_has_fact_slot_anchor(cart, RIR_FACT_RESOURCE, "buyer", "buyer")
               && scope_has_fact_slot_anchor(cart, RIR_FACT_PROJECTION, "view", "view")
               && scope_has_fact_slot_anchor(cart, RIR_FACT_AUTHORITY, "buyer", "buyer")
               && scope_has_op_slot_anchor(cart, RIR_OP_PROJECT_REFRESH, "view")
               && scope_has_fact_slot_anchor(checkout, RIR_FACT_RESOURCE, "cart", "cart")
               && scope_has_fact_slot_anchor(checkout, RIR_FACT_RESOURCE, "payment", "payment")
               && scope_has_op_slot_anchor(checkout, RIR_OP_MOVE, "cart")
               && scope_has_op_slot_anchor(checkout, RIR_OP_CLAIM, "payment")
               && scope_has_conservative_semantics(checkout, RIR_FLOW_WORLD_HANDOFF));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR refines capability and handoff summaries with richer lattice states");
    {
        HIRProgram *hir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object BuyerView { let hp: Int; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    rollback: full;\n"
            "    step move {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        transfer: checkout -> payment;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        RIRProgram *rir = NULL;
        const RIRScope *intent = NULL;
        const RIRStateSummary *capability = NULL;
        const RIRStateSummary *checkout = NULL;
        const RIRStateSummary *payment = NULL;
        bool ok = lower_rir_from_source(src, &hir, &rir);
        if (ok)
            intent = find_scope(rir, "Checkout", RIR_SCOPE_INTENT);
        if (intent != NULL) {
            capability = scope_find_state_summary(intent, "move");
            checkout = scope_find_state_summary(intent, "checkout");
            payment = scope_find_state_summary(intent, "payment");
        }
        EXPECT(ok
               && rir_validate(rir, NULL)
               && intent != NULL
               && capability != NULL
               && capability->resource_kind == RIR_RESOURCE_CAPABILITY_TOKEN
               && capability->final_state == RIR_STATE_AUTHORIZED
               && checkout != NULL
               && checkout->final_state == RIR_STATE_HANDOFF_PENDING
               && payment != NULL
               && payment->final_state == RIR_STATE_HANDED_OFF);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR lattice helper keeps detached projection and authority loss conservative");
    {
        bool projection_conflict = false;
        bool authority_conflict = false;
        bool handoff_conflict = false;
        RIRResourceState projection_state =
            rir_merge_state_for_kind(RIR_RESOURCE_PROJECTION_OBJECT,
                                     RIR_STATE_DETACHED,
                                     RIR_STATE_PUBLISHED,
                                     &projection_conflict);
        RIRResourceState authority_state =
            rir_merge_state_for_kind(RIR_RESOURCE_AUTHORITY_HANDLE,
                                     RIR_STATE_AUTHORIZED,
                                     RIR_STATE_AUTHORITY_LOST,
                                     &authority_conflict);
        RIRResourceState handoff_state =
            rir_merge_state_for_kind(RIR_RESOURCE_ZONE_HANDLE,
                                     RIR_STATE_OWNED,
                                     RIR_STATE_HANDED_OFF,
                                     &handoff_conflict);
        EXPECT(projection_state == RIR_STATE_DETACHED
               && !projection_conflict
               && authority_state == RIR_STATE_AUTHORITY_LOST
               && !authority_conflict
               && handoff_state == RIR_STATE_HANDOFF_PENDING
               && !handoff_conflict);
    }

    TEST("RIR bind derives tobject target as published boundary projection");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "subject Buyer { let hp: Int; let name: String; }\n"
            "object BuyerView { let hp: Int; }\n"
            "tobject BuyerReceipt { let hp: Int; let name: String; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot summary: BuyerView\n"
            "    tobject slot receipt: BuyerReceipt\n"
            "    bind summary from buyer\n"
            "    bind receipt from buyer\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *zone = find_scope(rir, "CheckoutZone", RIR_SCOPE_ZONE);
        const RIRStateSummary *summary = scope_find_state_summary(zone, "summary");
        const RIRStateSummary *receipt = scope_find_state_summary(zone, "receipt");
        EXPECT(ok
               && rir_validate(rir, NULL)
               && zone != NULL
               && summary != NULL
               && receipt != NULL
               && summary->resource_kind == RIR_RESOURCE_OBJECT_SLOT
               && summary->final_state == RIR_STATE_SYNCED
               && receipt->resource_kind == RIR_RESOURCE_TOBJECT_SLOT
               && receipt->final_state == RIR_STATE_PUBLISHED
               && scope_has_projection_fact_kind(zone, "summary",
                                                RIR_RESOURCE_PROJECTION_OBJECT,
                                                RIR_STATE_DIRTY)
               && scope_has_projection_fact_kind(zone, "receipt",
                                                RIR_RESOURCE_PROJECTION_TOBJECT,
                                                RIR_STATE_PUBLISHED)
               && scope_has_op_slot_anchor(zone, RIR_OP_PROJECT_REFRESH, "summary")
               && scope_has_op_slot_anchor(zone, RIR_OP_PROJECT_PUBLISH, "receipt"));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR validates zone/projection/authority slot contracts against DIR");
    {
        DIRProgram *dir = NULL;
        HIRProgram *hir = NULL;
        const char *src =
            "subject Buyer { let total: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object ReceiptView { let total: Int; }\n"
            "tobject ReceiptExport { let total: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot preview: ReceiptView\n"
            "    tobject slot receipt_out: ReceiptExport\n"
            "    authority buyer requires Payable\n"
            "    refresh preview from buyer by buyer\n"
            "    publish receipt_out from preview by buyer\n"
            "}\n";
        RIRProgram *rir = NULL;
        bool ok = lower_dir_rir_from_source(src, &dir, &hir, &rir);
        EXPECT(ok
               && dir_validate(dir, NULL)
               && rir_validate(rir, NULL)
               && rir_validate_against_dir(rir, dir, NULL));
        rir_destroy(rir);
        hir_destroy(hir);
        dir_destroy(dir);
    }
}
