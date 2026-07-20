static void
test_mir_lowering_part_a(void)
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

    TEST("MIR records view-backed resource owner slot");
    {
        const char *src =
            "func Flow() -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    let view: WriteView<Int> = ViewWrite(scores);\n"
            "    Write(view, 1);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *flow = NULL;
        bool found_borrow_owner = false;
        bool found_write_owner = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = (MIRRoutine *)find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        if (flow != NULL) {
            for (size_t bi = 0; bi < flow->block_count; bi++) {
                const MIRBasicBlock *block = &flow->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    const MIRInstruction *inst = &block->instructions[ii];
                    const char *abi_name = inst->type_layout != NULL
                        ? inst->type_layout->abi_type_name
                        : NULL;
                    if (inst->kind != MIR_INST_RESOURCE_OP || inst->name == NULL)
                        continue;
                    if (strcmp(inst->name, "BorrowWrite") == 0
                        && inst->resource_owner_slot_anchor != NULL
                        && strcmp(inst->resource_owner_slot_anchor, "scores") == 0
                        && inst->resource_owner_requires_metadata
                        && abi_name != NULL
                        && strcmp(abi_name, "Slot<Int>") == 0) {
                        found_borrow_owner = true;
                    }
                    if (strcmp(inst->name, "Write") == 0
                        && inst->slot_anchor != NULL
                        && strcmp(inst->slot_anchor, "view") == 0
                        && inst->resource_owner_slot_anchor != NULL
                        && strcmp(inst->resource_owner_slot_anchor, "scores") == 0
                        && inst->resource_owner_requires_metadata
                        && abi_name != NULL
                        && strcmp(abi_name, "Slot<Int>") == 0) {
                        found_write_owner = true;
                    }
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && flow != NULL
               && found_borrow_owner
               && found_write_owner);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR ABI table records explicit Option tag representation");
    {
        const MIRTypeLayout *opt_int = mir_abi_lookup("Option<Int>");
        const MIRTypeLayout *opt_float = mir_abi_lookup("Option<Float>");
        const MIRTypeLayout *opt_double = mir_abi_lookup("Option<Double>");
        const MIRTypeLayout *opt_string = mir_abi_lookup("Option<String>");
        EXPECT(opt_int != NULL
               && opt_float != NULL
               && opt_double != NULL
               && opt_string != NULL
               && opt_int->representation == MIR_ABI_REPR_EXPLICIT_TAG
               && opt_float->representation == MIR_ABI_REPR_EXPLICIT_TAG
               && opt_double->representation == MIR_ABI_REPR_EXPLICIT_TAG
               && opt_string->representation == MIR_ABI_REPR_EXPLICIT_TAG
               && opt_int->discriminant_field_name != NULL
               && strcmp(opt_int->discriminant_field_name, "tag") == 0
               && opt_int->primary_tag_value == 0
               && opt_int->secondary_tag_value == 1
               && opt_int->niche_none_pattern == NULL
               && opt_int->field_count == 2
               && strcmp(opt_int->fields[0].field_name, "tag") == 0
               && opt_int->fields[0].offset == 0
               && strcmp(opt_int->fields[1].field_name, "value") == 0
               && opt_int->fields[1].offset == 4
               && opt_float->fields[1].offset == 4
               && opt_float->niche_none_pattern == NULL
               && opt_double->fields[1].offset == 8
               && opt_double->niche_none_pattern == NULL
               && opt_string->niche_none_pattern == NULL);
    }

    TEST("MIR ABI owner synthesizes constructed resource runtime spellings");
    {
        const char *slot_claim = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SLOT, "Vec2", "Claim");
        const char *slot_write = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SLOT, "Vec2", "Write");
        const char *secure_claim = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SECURE_SLOT, "Vec2", "Claim");
        const char *secure_release = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SECURE_SLOT, "Vec2", "Release");
        const char *device_read = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_DEVICE_SLOT, "Vec2", "Read");
        const char *nested_write = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SLOT, "Array<Int>", "Write");
        const char *device_pin = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_DEVICE_SLOT, "Int", "PinRead");
        const char *void_write = mir_abi_resource_runtime_fn_by_kind(
            MIR_RESOURCE_ABI_SLOT, "Void", "Write");
        const MIRResourceRuntimeRow *slot_row =
            mir_abi_resource_runtime_row_by_kind(
                MIR_RESOURCE_ABI_SLOT, "Int", "Read");
        const MIRResourceRuntimeRow *constructed_row =
            mir_abi_resource_runtime_row_by_kind(
                MIR_RESOURCE_ABI_SECURE_SLOT, "Vec2", "Write");

        EXPECT(slot_claim != NULL
               && slot_write != NULL
               && secure_claim != NULL
               && secure_release != NULL
               && device_read != NULL
               && nested_write != NULL
               && device_pin == NULL
               && void_write == NULL
               && strcmp(slot_claim, "pgy_claim_Vec2") == 0
               && strcmp(slot_write, "pgy_write_Vec2") == 0
               && strcmp(secure_claim, "pgy_claim_secure_Vec2") == 0
               && strcmp(secure_release, "pgy_secure_release_Vec2") == 0
               && strcmp(device_read, "pgy_device_read_Vec2") == 0
               && strcmp(nested_write, "pgy_write_Array_Int") == 0
               && slot_row != NULL
               && strcmp(slot_row->domain, "native-resource") == 0
               && strcmp(slot_row->runtime_fn, "pgy_read_Int") == 0
               && strcmp(slot_row->call_shape, "container_ptr_to_value") == 0
               && constructed_row != NULL
               && strcmp(constructed_row->domain, "constructed-resource") == 0
               && strcmp(constructed_row->runtime_fn, "pgy_secure_write_Vec2") == 0
               && strcmp(constructed_row->call_shape, "container_ptr_value_token_ptr_to_void") == 0);
        EXPECT(mir_abi_resource_runtime_fn_by_kind(
                   MIR_RESOURCE_ABI_SLOT, "Unknown", "Claim") == NULL);
    }

    TEST("MIR owns TextBuilder layout and target-specific runtime symbols");
    {
        const MIRTypeLayout *layout = mir_abi_lookup("TextBuilder");
        const MIRTextBuilderRuntimeRow *append =
            mir_text_builder_runtime_row("Append");
        const MIRTextBuilderRuntimeRow *finish =
            mir_text_builder_runtime_row("Finish");
        const MIRTextBuilderRuntimeRow *create =
            mir_text_builder_runtime_row("New");

        EXPECT(layout != NULL
               && layout->field_count == 4
               && strcmp(layout->fields[0].field_name, "data") == 0
               && strcmp(layout->fields[3].field_name, "finished") == 0
               && mir_text_builder_runtime_row_count() == 4
               && append != NULL
               && strcmp(append->c_inline_fn,
                         "pgy_text_builder_append") == 0
               && strcmp(append->llvm_export_fn,
                         "pgy_text_builder_append_export") == 0
               && finish != NULL
               && finish->c_call_shape
                    == MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING
               && finish->llvm_call_shape
                    == MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING
               && create != NULL
               && create->c_call_shape
                    == MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER
               && create->llvm_call_shape
                    == MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID);
    }

    TEST("MIR ABI resource runtime row table exposes native resource rows");
    {
        EXPECT(mir_abi_resource_runtime_row_count() == 150);
        EXPECT(strcmp(mir_abi_resource_runtime_row_domain(0), "native-resource") == 0
               && strcmp(mir_abi_resource_runtime_row_type_name(0), "Slot<Int>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(0), "Claim") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(0), "pgy_claim_Int") == 0
               && strcmp(mir_abi_resource_runtime_row_target_kind(0), "function") == 0
               && strcmp(mir_abi_resource_runtime_row_materialization(0), "mir_abi_resource_row") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(0), "returns_container") == 0);
        EXPECT(strcmp(mir_abi_resource_runtime_row_type_name(47), "SecureSlot<String>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(47), "Write") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(47), "pgy_secure_write_String") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(47), "container_ptr_value_token_ptr_to_void") == 0);
        EXPECT(strcmp(mir_abi_resource_runtime_row_type_name(48), "Slot<Int>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(48), "PinRead") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(48), "pgy_pin_read_Int") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(48), "container_ptr_to_pinned_view") == 0);
        EXPECT(strcmp(mir_abi_resource_runtime_row_type_name(119), "SecureSlot<String>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(119), "UnpinCleanup") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(119), "pgy_secure_unpin_cleanup_String") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(119), "pinned_view_ptr_to_void") == 0);
        EXPECT(strcmp(mir_abi_resource_runtime_row_type_name(120), "DeviceSlot<Int>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(120), "Claim") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(120), "pgy_claim_device_Int") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(120), "returns_container") == 0);
        EXPECT(strcmp(mir_abi_resource_runtime_row_type_name(149), "DeviceSlot<String>") == 0
               && strcmp(mir_abi_resource_runtime_row_operation(149), "SubmitRead") == 0
               && strcmp(mir_abi_resource_runtime_row_symbol(149), "pgy_submit_device_read_String") == 0
               && strcmp(mir_abi_resource_runtime_row_call_shape(149), "container_ptr_to_task_handle") == 0);
        EXPECT(mir_abi_resource_runtime_row_domain(150) == NULL
               && mir_abi_resource_runtime_row_type_name(150) == NULL
               && mir_abi_resource_runtime_row_operation(150) == NULL
               && mir_abi_resource_runtime_row_symbol(150) == NULL
               && mir_abi_resource_runtime_row_target_kind(150) == NULL
               && mir_abi_resource_runtime_row_materialization(150) == NULL
               && mir_abi_resource_runtime_row_call_shape(150) == NULL);
    }

    TEST("MIR ABI native resource rows match self-host runtime-call artifact");
    {
        EXPECT(runtime_call_abi_expected_native_rows_match());
    }

    TEST("MIR validator rejects view-backed resource owner metadata drift");
    {
        const char *src =
            "func Flow() -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    let view: WriteView<Int> = ViewWrite(scores);\n"
            "    Write(view, 1);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *flow = NULL;
        MIRInstruction *borrow_inst = NULL;
        MIRInstruction *write_inst = NULL;
        const char *saved_borrow_owner = NULL;
        const char *saved_write_owner = NULL;
        char *mir_error = NULL;
        bool rejected_missing_borrow_owner = false;
        bool rejected_missing_owner = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = (MIRRoutine *)find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        if (flow != NULL) {
            for (size_t bi = 0; bi < flow->block_count
                    && (borrow_inst == NULL || write_inst == NULL); bi++) {
                MIRBasicBlock *block = &flow->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind != MIR_INST_RESOURCE_OP || inst->name == NULL)
                        continue;
                    if (strcmp(inst->name, "BorrowWrite") == 0
                        && inst->slot_anchor != NULL
                        && strcmp(inst->slot_anchor, "scores") == 0) {
                        borrow_inst = inst;
                    }
                    if (strcmp(inst->name, "Write") == 0
                        && inst->slot_anchor != NULL
                        && strcmp(inst->slot_anchor, "view") == 0) {
                        write_inst = inst;
                    }
                    if (borrow_inst != NULL && write_inst != NULL)
                        break;
                }
            }
        }
        if (borrow_inst != NULL) {
            saved_borrow_owner = borrow_inst->resource_owner_slot_anchor;
            borrow_inst->resource_owner_slot_anchor = NULL;
            rejected_missing_borrow_owner = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "view-backed resource op is missing owner slot ABI metadata") != NULL;
            free(mir_error);
            mir_error = NULL;
            borrow_inst->resource_owner_slot_anchor = saved_borrow_owner;
        }
        if (write_inst != NULL) {
            saved_write_owner = write_inst->resource_owner_slot_anchor;
            write_inst->resource_owner_slot_anchor = NULL;
            rejected_missing_owner = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "view-backed resource op is missing owner slot ABI metadata") != NULL;
            free(mir_error);
            mir_error = NULL;
            write_inst->resource_owner_slot_anchor = saved_write_owner;
        }
        EXPECT(ok
               && flow != NULL
               && borrow_inst != NULL
               && write_inst != NULL
               && rejected_missing_borrow_owner
               && rejected_missing_owner
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries await Future ABI layouts from RIR ops");
    {
        const char *src =
            "func Worker() -> Int { return 1; }\n"
            "async func AwaitKinds(pending: RemoteFuture<Int>) -> Void {\n"
            "    let f: Future<Int> = spawn Worker();\n"
            "    let localValue = await f;\n"
            "    let remoteValue = await pending;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *local_inst = NULL;
        const MIRTypeLayout *saved_local_layout = NULL;
        char *mir_error = NULL;
        bool local_layout_ok = false;
        bool remote_layout_ok = false;
        bool rejected_invalid_local_layout = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "AwaitKinds", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind != MIR_INST_RESOURCE_OP
                        || inst->name == NULL
                        || inst->type_layout == NULL
                        || inst->type_layout->abi_type_name == NULL) {
                        continue;
                    }
                    if (strcmp(inst->name, "AwaitLocal") == 0
                        && strcmp(inst->type_layout->abi_type_name, "Future") == 0) {
                        local_layout_ok = true;
                        local_inst = inst;
                    }
                    if (strcmp(inst->name, "AwaitRemote") == 0
                        && strcmp(inst->type_layout->abi_type_name, "RemoteFuture") == 0)
                        remote_layout_ok = true;
                }
            }
        }
        if (local_inst != NULL) {
            saved_local_layout = local_inst->type_layout;
            local_inst->type_layout = mir_abi_lookup("RemoteFuture");
            rejected_invalid_local_layout =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "await resource op has invalid MIR ABI type layout fact") != NULL;
            local_inst->type_layout = saved_local_layout;
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && local_layout_ok
               && remote_layout_ok
               && rejected_invalid_local_layout);
        free(mir_error);
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
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block],
                   "IntentStep", "pay")
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

    TEST("MIR validator rejects predecessor without forward edge");
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
        bool corrupted = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL) {
            for (size_t target = 0; target < purchase->block_count && !corrupted; target++) {
                MIRBasicBlock *block = &purchase->blocks[target];
                if (block->predecessor_count == 0)
                    continue;
                for (size_t pred = 0; pred < purchase->block_count; pred++) {
                    size_t next_count;
                    size_t *grown;
                    if (pred == target)
                        continue;
                    if (!test_block_has_forward_edge_to(&purchase->blocks[pred], target)) {
                        next_count = block->predecessor_count + 1;
                        grown = realloc(block->predecessors,
                                        next_count * sizeof(size_t));
                        if (grown == NULL)
                            break;
                        block->predecessors = grown;
                        block->predecessors[block->predecessor_count] = pred;
                        block->predecessor_count = next_count;
                        block->predecessor_capacity = next_count;
                        corrupted = true;
                        break;
                    }
                }
            }
        }
        rejected = ok
                   && purchase != NULL
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "predecessor") != NULL
                   && strstr(mir_error, "no matching forward edge") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
