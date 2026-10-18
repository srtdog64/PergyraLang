    TEST("RIR enrichment binds original parsed program and callable scope identities");
    {
        const char *sources[2] = {
            "func Main() -> Void { Log(1); }\n",
            "func Main() -> Void { Log(2); }\n"
        };
        Lexer *lexers[2] = {NULL, NULL};
        Parser *parsers[2] = {NULL, NULL};
        ASTNode *asts[2] = {NULL, NULL};
        SemanticResult *sems[2] = {NULL, NULL};
        HIRProgram *hirs[2] = {NULL, NULL};
        RIRProgram *rirs[2] = {NULL, NULL};
        bool ready = true;
        for (size_t i = 0; i < 2; i++) {
            lexers[i] = lexer_create(sources[i]);
            parsers[i] = parser_create(lexers[i]);
            asts[i] = parser_parse_program(parsers[i]);
            sems[i] = semantic_analyze(asts[i]);
            if (parser_has_error(parsers[i]) || sems[i] == NULL || !sems[i]->success) {
                ready = false;
                continue;
            }
            hirs[i] = hir_lower(asts[i], NULL);
            rirs[i] = rir_lower(asts[i], NULL);
            if (hirs[i] == NULL || rirs[i] == NULL || hirs[i]->routine_count != 1 ||
                rirs[i]->scope_count != 1 || !rir_enrich_with_hir_flow(rirs[i], hirs[i], NULL))
                ready = false;
        }
        EXPECT(ready);
        if (ready) {
            HIRRoutine *a = &hirs[0]->routines[0];
            HIRRoutine *b = &hirs[1]->routines[0];
            RIRScope *scope = &rirs[0]->scopes[0];
            ASTNode *root = rirs[0]->program_root;
            ASTNode *scope_root = scope->program_root;
            ASTNode *scope_ast = scope->ast;
            uint32_t source_id = scope->source_syntax_id;
            EXPECT(asts[0] != asts[1] && ast_node_stable_id(asts[0]) == ast_node_stable_id(asts[1]) &&
                a->ast != b->ast && a->source_syntax_id == b->source_syntax_id);
            EXPECT(rir_validate(rirs[0], NULL) && rir_validate(rirs[1], NULL) &&
                rir_scope_for_hir_routine(rirs[0], a) == scope &&
                rir_scope_for_hir_routine(rirs[1], b) == &rirs[1]->scopes[0]);
            EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[1], NULL) &&
                !rir_enrich_with_hir_flow(rirs[1], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[0], b) == NULL &&
                rirs[0]->program_root == root && scope->ast == scope_ast &&
                scope->source_syntax_id == source_id);
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_enrich_with_hir_flow(rirs[1], hirs[1], NULL));
            rirs[0]->program_root = NULL;
            EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[0], a) == NULL &&
                rirs[0]->program_root == NULL && scope->ast == scope_ast);
            rirs[0]->program_root = root;
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL));
            ASTNode *foreign_root = rirs[1]->program_root;
            ASTNode *foreign_scope_root = rirs[1]->scopes[0].program_root;
            rirs[1]->program_root = root;
            rirs[1]->scopes[0].program_root = root;
            EXPECT(!rir_enrich_with_hir_flow(rirs[1], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[1], a) == NULL &&
                rirs[1]->program_root == root && rirs[1]->scopes[0].ast == b->ast);
            rirs[1]->program_root = foreign_root;
            rirs[1]->scopes[0].program_root = foreign_scope_root;
            EXPECT(rir_enrich_with_hir_flow(rirs[1], hirs[1], NULL));
            scope->ast = b->ast;
            EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[0], a) == NULL &&
                scope->ast == b->ast && scope->source_syntax_id == source_id);
            scope->ast = scope_ast;
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL));
            scope->program_root = NULL;
            EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[0], a) == NULL && scope->program_root == NULL);
            scope->program_root = scope_root;
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL));
            rirs[0]->scope_count = 0;
            EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_scope_for_hir_routine(rirs[0], a) == NULL && rirs[0]->scope_count == 0);
            rirs[0]->scope_count = 1;
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL));
            RIRScope *duplicates = malloc(2 * sizeof(*duplicates));
            EXPECT(duplicates != NULL);
            if (duplicates != NULL) {
                duplicates[0] = *scope;
                duplicates[1] = *scope;
                RIRScope *original_scopes = rirs[0]->scopes;
                size_t original_capacity = rirs[0]->scope_capacity;
                rirs[0]->scopes = duplicates;
                rirs[0]->scope_count = rirs[0]->scope_capacity = 2;
                EXPECT(!rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                    rir_scope_for_hir_routine(rirs[0], a) == NULL &&
                    rirs[0]->scope_count == 2 && duplicates[1].ast == scope_ast);
                rirs[0]->scopes = original_scopes;
                rirs[0]->scope_count = 1;
                rirs[0]->scope_capacity = original_capacity;
                free(duplicates);
            }
            EXPECT(rir_enrich_with_hir_flow(rirs[0], hirs[0], NULL) &&
                rir_validate(rirs[0], NULL) && rir_validate(rirs[1], NULL) &&
                rirs[0]->program_root == root && scope->program_root == scope_root &&
                scope->ast == scope_ast && scope->source_syntax_id == source_id);
        }
        for (size_t i = 0; i < 2; i++) {
            rir_destroy(rirs[i]);
            hir_destroy(hirs[i]);
            semantic_result_destroy(sems[i]);
            ast_destroy(asts[i]);
            parser_destroy(parsers[i]);
            lexer_destroy(lexers[i]);
        }
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
