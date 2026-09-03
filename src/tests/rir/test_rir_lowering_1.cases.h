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
            "func ChannelOps() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
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
            "async func ParallelOps() -> Void {\n"
            "    let f: Future<Int> = spawn Worker();\n"
            "    let first: Int = await f;\n"
            "    let second: Int = await spawn Worker();\n"
            "    async { Log(1); }\n"
            "    parallel {\n"
            "        Log(2);\n"
            "        Log(3);\n"
            "    }\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "ParallelOps", RIR_SCOPE_FUNCTION);
        const RIRStateSummary *future = scope_find_state_summary(flow, "f");
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_op_subject(flow, RIR_OP_SPAWN, "spawn")
               && scope_has_op_subject(flow, RIR_OP_AWAIT_LOCAL, "f")
               && scope_has_op_subject(flow, RIR_OP_AWAIT_LOCAL, "spawn")
               && scope_has_resource_fact(flow,
                                          "f",
                                          RIR_RESOURCE_LOCAL_FUTURE_HANDLE)
               && future != NULL
               && future->initial_state == RIR_STATE_REMOTE_PENDING
               && future->final_state == RIR_STATE_RELEASED
               && scope_has_op_subject(flow, RIR_OP_ASYNC, "async")
               && scope_has_op_subject(flow, RIR_OP_PARALLEL, "parallel"));
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("RIR await consumes remote Future handles");
    {
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        const char *src =
            "async func JoinRemote(own pending: RemoteFuture<Int>) -> Void {\n"
            "    let result = await pending;\n"
            "}\n";
        bool ok = lower_rir_from_source(src, &hir, &rir);
        const RIRScope *flow = find_scope(rir, "JoinRemote", RIR_SCOPE_FUNCTION);
        const RIRStateSummary *pending =
            scope_find_state_summary(flow, "pending");
        EXPECT(ok
               && rir_validate(rir, NULL)
               && flow != NULL
               && scope_has_op_subject(flow, RIR_OP_AWAIT_REMOTE, "pending")
               && scope_has_resource_fact(flow,
                                          "pending",
                                          RIR_RESOURCE_REMOTE_FUTURE_HANDLE)
               && pending != NULL
               && pending->initial_state == RIR_STATE_REMOTE_PENDING
               && pending->final_state == RIR_STATE_RELEASED);
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
