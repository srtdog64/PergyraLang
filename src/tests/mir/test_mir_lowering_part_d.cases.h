static void
test_mir_lowering_part_d(void)
{
    TEST("MIR declaration headers preserve field metadata");
    {
        const char *src =
            "class Item {\n"
            "    let value: Int;\n"
            "    let label: String;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *item = NULL;
        const MIRDeclField *value = NULL;
        const MIRDeclField *label = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            item = mir_find_decl_header(mir, "Item");
        if (item != NULL) {
            value = mir_decl_header_field(item, 0);
            label = mir_decl_header_field(item, 1);
        }
        EXPECT(ok
               && item != NULL
               && mir_decl_header_field_count(item) == 2
               && value != NULL
               && label != NULL
               && mir_decl_field_kind_or(value, MIR_DECL_FIELD_UNKNOWN)
                    == MIR_DECL_FIELD_CLASS
               && mir_decl_field_name(value) != NULL
               && strcmp(mir_decl_field_name(value), "value") == 0
               && mir_decl_field_type(value) != NULL
               && ast_type_name(mir_decl_field_type(value)) != NULL
               && strcmp(ast_type_name(mir_decl_field_type(value)), "Int") == 0
               && mir_decl_field_name(label) != NULL
               && strcmp(mir_decl_field_name(label), "label") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve role-slot ability refs");
    {
        const char *src =
            "subject IntBag { let item: Int; }\n"
            "subject TextBag { let item: String; }\n"
            "ability Bufferable<T = Int> {\n"
            "    func Put(self, value: T) -> Int;\n"
            "}\n"
            "role IntBuffer for IntBag {\n"
            "    impl ability Bufferable {\n"
            "        func Put(self, value: Int) -> Int { return value; }\n"
            "    }\n"
            "}\n"
            "role TextBuffer for TextBag {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(self, value: String) -> Int { return 1; }\n"
            "    }\n"
            "}\n"
            "party StorageParty {\n"
            "    role slot defaulted: Bufferable\n"
            "    role slot explicit: Bufferable<String>\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *party = NULL;
        const MIRDeclField *defaulted = NULL;
        const MIRDeclField *explicit_slot = NULL;
        const MIRAbilityRef *default_ref = NULL;
        const MIRAbilityRef *explicit_ref = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            party = mir_find_decl_header(mir, "StorageParty");
        if (party != NULL) {
            defaulted = mir_decl_header_field(party, 0);
            explicit_slot = mir_decl_header_field(party, 1);
        }
        if (defaulted != NULL)
            default_ref = mir_decl_field_required_ability_ref(defaulted, 0);
        if (explicit_slot != NULL)
            explicit_ref =
                mir_decl_field_required_ability_ref(explicit_slot, 0);
        EXPECT(ok
               && party != NULL
               && mir_decl_header_field_count(party) == 2
               && default_ref != NULL
               && explicit_ref != NULL
               && mir_decl_field_required_ability_count(defaulted) == 1
               && mir_ability_ref_base_name(default_ref) != NULL
               && strcmp(mir_ability_ref_base_name(default_ref),
                         "Bufferable") == 0
               && mir_ability_ref_actual_arg_count(default_ref) == 0
               && mir_ability_ref_base_name(explicit_ref) != NULL
               && strcmp(mir_ability_ref_base_name(explicit_ref),
                         "Bufferable") == 0
               && mir_ability_ref_actual_arg_count(explicit_ref) == 1
               && mir_ability_ref_actual_arg_type_name(explicit_ref, 0)
                    != NULL
               && strcmp(mir_ability_ref_actual_arg_type_name(
                             explicit_ref, 0),
                         "String") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve zone authority ability refs");
    {
        const char *src =
            "subject Buyer { let name: String; }\n"
            "ability Payable<T = Int> {\n"
            "    func Pay(self, value: T) -> Int;\n"
            "}\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable<String> {\n"
            "        func Pay(self, value: String) -> Int { return 1; }\n"
            "    }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable<String>\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *zone = NULL;
        const MIRDeclZoneAuthority *authority = NULL;
        const MIRAbilityRef *ability = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            zone = mir_find_decl_header(mir, "PaymentZone");
        if (zone != NULL)
            authority = mir_decl_header_zone_authority(zone, 0);
        if (authority != NULL)
            ability =
                mir_decl_zone_authority_required_ability_ref(authority, 0);
        EXPECT(ok
               && zone != NULL
               && mir_decl_header_zone_authority_count(zone) == 1
               && authority != NULL
               && mir_decl_zone_authority_subject_slot_name(authority) != NULL
               && strcmp(mir_decl_zone_authority_subject_slot_name(authority),
                         "buyer") == 0
               && mir_decl_zone_authority_required_ability_count(authority) == 1
               && ability != NULL
               && mir_ability_ref_base_name(ability) != NULL
               && strcmp(mir_ability_ref_base_name(ability), "Payable") == 0
               && mir_ability_ref_actual_arg_count(ability) == 1
               && mir_ability_ref_actual_arg_type_name(ability, 0) != NULL
               && strcmp(mir_ability_ref_actual_arg_type_name(ability, 0),
                         "String") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve domain slot classification metadata");
    {
        const char *src =
            "subject Buyer { }\n"
            "tobject ReceiptExport { let id: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    tobject slot receipt_out: ReceiptExport\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *zone = NULL;
        const MIRDeclField *buyer = NULL;
        const MIRDeclField *receipt = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            zone = mir_find_decl_header(mir, "PaymentZone");
        if (zone != NULL) {
            buyer = mir_decl_header_field(zone, 0);
            receipt = mir_decl_header_field(zone, 1);
        }
        EXPECT(ok
               && zone != NULL
               && mir_decl_header_field_count(zone) == 2
               && buyer != NULL
               && receipt != NULL
               && mir_decl_field_kind_or(buyer, MIR_DECL_FIELD_UNKNOWN)
                    == MIR_DECL_FIELD_DOMAIN_SLOT
               && mir_decl_field_kind_or(receipt, MIR_DECL_FIELD_UNKNOWN)
                    == MIR_DECL_FIELD_DOMAIN_SLOT
               && mir_decl_field_is_subject_like(buyer)
               && !mir_decl_field_is_tobject_like(buyer)
               && mir_decl_field_is_binding_like(buyer)
               && !mir_decl_field_is_subject_like(receipt)
               && mir_decl_field_is_tobject_like(receipt)
               && !mir_decl_field_is_binding_like(receipt)
               && mir_decl_field_type_name(receipt) != NULL
               && strcmp(mir_decl_field_type_name(receipt), "ReceiptExport") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers include intent callables");
    {
        const char *src =
            "subject Buyer { }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *intent = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            intent = mir_find_decl_header_of_type(
                mir, AST_INTENT_DECL, "Charge");
        }
        EXPECT(ok
               && intent != NULL
               && mir_decl_header_ast_type_or(intent, AST_PROGRAM)
                    == AST_INTENT_DECL
               && mir_decl_header_name(intent) != NULL
               && strcmp(mir_decl_header_name(intent), "Charge") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers include event declarations");
    {
        const char *src =
            "event OnScore(points: Int);\n"
            "func Main() -> Void { }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *event = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            event = mir_find_decl_header_of_type(
                mir, AST_EVENT_DECL, "OnScore");
        }
        EXPECT(ok
               && event != NULL
               && mir_decl_header_ast_type_or(event, AST_PROGRAM)
                    == AST_EVENT_DECL
               && mir_decl_header_name(event) != NULL
               && strcmp(mir_decl_header_name(event), "OnScore") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects declaration field owner metadata drift");
    {
        const char *src =
            "class Item {\n"
            "    let value: Int;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            MIRDeclHeader *item =
                (MIRDeclHeader *) mir_find_decl_header(mir, "Item");
            if (item != NULL && item->field_metadata_count > 0) {
                item->field_metadata[0].owner_name = "OtherItem";
                mutated = true;
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "field[0] has owner metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects terminal CFG-owned control fallback statements");
    {
        const char *src =
            "func CfgOwnedTerminal(value: Int) -> Int {\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool injected = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "CfgOwnedTerminal", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && !injected; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                ASTNode *stmt = block->source_statement_inventory.count > 0
                    && block->source_statement_inventory.items != NULL
                    ? block->source_statement_inventory.items[0]
                    : NULL;
                if (stmt == NULL || stmt->type != AST_RETURN
                    || block->has_succ_true || block->has_succ_false) {
                    continue;
                }
                MIRInstruction *grown = realloc(block->instructions,
                    (block->instruction_count + 1) * sizeof(MIRInstruction));
                if (grown == NULL)
                    break;
                block->instructions = grown;
                memset(&block->instructions[block->instruction_count], 0,
                    sizeof(MIRInstruction));
                block->instructions[block->instruction_count].id =
                    routine->instruction_count++;
                block->instructions[block->instruction_count].kind = MIR_INST_STMT;
                block->instructions[block->instruction_count].name = "stmt";
                block->instructions[block->instruction_count].ast = stmt;
                block->instruction_count++;
                injected = true;
            }
        }
        rejected = ok
                   && injected
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "CFG-owned control statement") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR does not treat Intent-prefixed user calls as observability");
    {
        const char *src =
            "func IntentDomainAction() -> String {\n"
            "    return \"ok\";\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(IntentDomainAction());\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool any_observability_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t b = 0; b < routine->block_count; b++) {
                const MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    const MIRInstruction *inst = &block->instructions[i];
                    if (inst->has_surface_usage_facts
                        && inst->uses_intent_observability_surface) {
                        any_observability_fact = true;
                    }
                }
            }
        }
        EXPECT(ok
               && routine != NULL
               && mir != NULL
               && mir->has_inventory_surface_usage_facts
               && !mir->inventory_uses_intent_observability_surface
               && !any_observability_fact
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects duplicate declaration header names");
    {
        const char *src =
            "class A { let value: Int; }\n"
            "class B { let value: Int; }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        size_t mutated = 0;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL && strcmp(header->name, "B") == 0) {
                    header->name = "A";
                    mutated++;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated == 1
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "duplicates declaration header") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
