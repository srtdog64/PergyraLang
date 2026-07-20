static void
test_mir_runtime_call_abi_facts(void)
{
    TEST("MIR ABI runtime-call identities are nonzero and unique");
    {
        bool unique = true;
        for (size_t i = 0; i < mir_abi_resource_runtime_row_count(); i++) {
            const MIRResourceRuntimeRow *left =
                mir_abi_resource_runtime_row_at(i);
            uint32_t left_id = mir_abi_resource_runtime_row_id(left);
            if (left_id == 0
                || (left_id & UINT32_C(0xf0000000))
                    != UINT32_C(0x40000000)) {
                unique = false;
                break;
            }
            for (size_t j = i + 1;
                 j < mir_abi_resource_runtime_row_count(); j++) {
                const MIRResourceRuntimeRow *right =
                    mir_abi_resource_runtime_row_at(j);
                if (left_id == mir_abi_resource_runtime_row_id(right)) {
                    unique = false;
                    break;
                }
            }
            if (!unique)
                break;
        }
        EXPECT(unique);
    }

    TEST("MIR materializes constructed resource runtime rows once");
    {
        const char *src =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Flow() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2));\n"
            "    Release(s);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *flow = NULL;
        bool saw_write = false;
        bool saw_release = false;
        bool rejected_id_mismatch = false;
        bool rejected_payload_mismatch = false;
        MIRInstruction *write_inst = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        if (flow != NULL) {
            for (size_t bi = 0; bi < flow->block_count; bi++) {
                const MIRBasicBlock *block = &flow->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    const MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind != MIR_INST_RESOURCE_OP
                        || inst->name == NULL
                        || inst->abi_type_name == NULL
                        || strcmp(inst->abi_type_name, "Slot<Vec2>") != 0)
                        continue;
                    if (strcmp(inst->name, "Write") == 0)
                        saw_write = inst->resource_runtime_fact_present
                            && strcmp(inst->resource_runtime_fact.domain,
                                      "constructed-resource") == 0
                            && strcmp(inst->resource_runtime_fact.runtime_fn,
                                      "pgy_write_Vec2") == 0
                            && strcmp(inst->resource_runtime_fact.call_shape,
                                      "container_ptr_value_to_void") == 0
                            && mir_abi_resource_runtime_row_is_constructed_nominal(
                                &inst->resource_runtime_fact)
                            && inst->resource_runtime_fact.runtime_call_abi_id != 0
                            && inst->resource_runtime_fact.runtime_call_abi_id ==
                                mir_abi_resource_runtime_row_id(
                                    &inst->resource_runtime_fact);
                    if (strcmp(inst->name, "Write") == 0)
                        write_inst = (MIRInstruction *)inst;
                    if (strcmp(inst->name, "Release") == 0)
                        saw_release = inst->resource_runtime_fact_present
                            && strcmp(inst->resource_runtime_fact.runtime_fn,
                                      "pgy_release_Vec2") == 0
                            && inst->resource_runtime_fact.runtime_call_abi_id != 0
                            && inst->resource_runtime_fact.runtime_call_abi_id ==
                                mir_abi_resource_runtime_row_id(
                                    &inst->resource_runtime_fact);
                }
            }
        }
        if (write_inst != NULL) {
            uint32_t saved_id =
                write_inst->resource_runtime_fact.runtime_call_abi_id;
            write_inst->resource_runtime_fact.runtime_call_abi_id = saved_id + 1;
            rejected_id_mismatch = !mir_validate(mir, NULL);
            write_inst->resource_runtime_fact.runtime_call_abi_id = saved_id;
            const char *saved_runtime_fn =
                write_inst->resource_runtime_fact.runtime_fn;
            write_inst->resource_runtime_fact.runtime_fn = "pgy_wrong_Vec2";
            rejected_payload_mismatch = !mir_validate(mir, NULL);
            write_inst->resource_runtime_fact.runtime_fn = saved_runtime_fn;
        }
        EXPECT(ok && mir_validate(mir, NULL) && flow != NULL
               && saw_write && saw_release && rejected_id_mismatch
               && rejected_payload_mismatch);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR rejects missing runtime rows on concrete resource consumers");
    {
        const char *src =
            "func Consume(own dev: DeviceSlot<Int>) -> Int {\n"
            "    let value: Int = DeviceRead(dev);\n"
            "    ReleaseDeviceSlot(dev);\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *consume = NULL;
        MIRInstruction *read_consumer = NULL;
        MIRInstruction *release_consumer = NULL;
        char *mir_error = NULL;
        bool rejected_read = false;
        bool rejected_release = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            consume = find_mir_routine_mut(mir, "Consume", MIR_SCOPE_FUNCTION);
        if (consume != NULL) {
            for (size_t bi = 0; bi < consume->block_count; bi++) {
                MIRBasicBlock *block = &consume->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->arg1 != NULL
                        && strcmp(inst->arg1, "DeviceRead") == 0) {
                        read_consumer = inst;
                    }
                    if (inst->kind == MIR_INST_STMT
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "ReleaseDeviceSlot") == 0) {
                        release_consumer = inst;
                    }
                }
            }
        }
        if (read_consumer != NULL) {
            read_consumer->resource_runtime_fact_present = false;
            rejected_read = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                    "resource consumer is missing its lowered runtime-call ABI row fact") != NULL;
            free(mir_error);
            mir_error = NULL;
            read_consumer->resource_runtime_fact_present = true;
        }
        if (release_consumer != NULL) {
            release_consumer->resource_runtime_fact_present = false;
            rejected_release = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                    "resource consumer is missing its lowered runtime-call ABI row fact") != NULL;
            free(mir_error);
            mir_error = NULL;
            release_consumer->resource_runtime_fact_present = true;
        }
        EXPECT(ok
               && consume != NULL
               && read_consumer != NULL
               && release_consumer != NULL
               && rejected_read
               && rejected_release
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR keeps multiple resource runtime rows distinct within one statement");
    {
        const char *src =
            "func Sum(own left: DeviceSlot<Int>, own right: DeviceSlot<Int>) -> Int {\n"
            "    let total: Int = DeviceRead(left) + DeviceRead(right);\n"
            "    return total;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *sum = NULL;
        const MIRInstruction *reads[2] = { NULL, NULL };
        const MIRInstruction *consumer = NULL;
        size_t read_count = 0;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            sum = find_mir_routine_mut(mir, "Sum", MIR_SCOPE_FUNCTION);
        if (sum != NULL) {
            for (size_t bi = 0; bi < sum->block_count; bi++) {
                MIRBasicBlock *block = &sum->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_RESOURCE_OP
                        && inst->name != NULL
                        && strcmp(inst->name, "Read") == 0
                        && read_count < 2) {
                        reads[read_count++] = inst;
                    }
                    if (inst->kind == MIR_INST_DEF
                        && inst->result_name != NULL
                        && (strcmp(inst->result_name, "total") == 0
                            || strncmp(inst->result_name, "total.", 6) == 0)) {
                        consumer = inst;
                    }
                }
            }
        }
        EXPECT(ok
               && sum != NULL
               && read_count == 2
               && reads[0] != NULL
               && reads[1] != NULL
               && consumer != NULL
               && reads[0]->has_source_statement_stable_id
               && reads[1]->has_source_statement_stable_id
               && consumer->has_source_statement_stable_id
               && reads[0]->source_statement_stable_id
                    == reads[1]->source_statement_stable_id
               && reads[0]->source_statement_stable_id
                    == consumer->source_statement_stable_id
               && reads[0]->source_stable_id != reads[1]->source_stable_id
               && mir_abi_resource_runtime_instruction_for_source(
                    sum, reads[0]->source_stable_id) == reads[0]
               && mir_abi_resource_runtime_instruction_for_source(
                    sum, reads[1]->source_stable_id) == reads[1]
               && !consumer->resource_runtime_fact_present
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR pin runtime rows require a same-type resource owner fact");
    {
        const char *src =
            "func Pin() -> Void {\n"
            "    let s: SecureSlot<Int> = ClaimSecureSlot();\n"
            "    Write(s, 1, s_token);\n"
            "    pin s as v: ReadView<Int> {\n"
            "        Log(Read(v));\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *pin = NULL;
        const MIRInstruction *owner = NULL;
        const MIRResourceRuntimeRow *pin_row = NULL;
        MIRInstruction *disabled_rows[16];
        size_t disabled_row_count = 0;
        char *validation_error = NULL;
        bool rejected_missing_owner = false;
        bool validation_ok = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            pin = find_mir_routine_mut(mir, "Pin", MIR_SCOPE_FUNCTION);
        if (pin != NULL) {
            owner = mir_abi_resource_runtime_pin_owner_for_mir(
                pin, MIR_RESOURCE_ABI_SECURE_SLOT, "Int");
            pin_row = mir_abi_resource_runtime_pin_row_for_mir(
                pin, MIR_RESOURCE_ABI_SECURE_SLOT, "Int", "PinReadInit");
        }
        if (owner != NULL) {
            /* The owner lookup is deliberately allowed to choose any
             * same-type Read/Write fact.  Remove every candidate so the
             * negative check proves the missing-fact boundary rather than
             * merely disabling one of several equivalent rows. */
            for (size_t bi = 0; bi < pin->block_count; bi++) {
                MIRBasicBlock *block = &pin->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    const MIRResourceRuntimeRow *row =
                        &inst->resource_runtime_fact;
                    if (inst->kind != MIR_INST_RESOURCE_OP
                        || !inst->resource_runtime_fact_present
                        || row->abi_type_name == NULL
                        || strcmp(row->abi_type_name, "SecureSlot<Int>") != 0
                        || row->resource_op_name == NULL
                        || (strcmp(row->resource_op_name, "Read") != 0
                            && strcmp(row->resource_op_name, "Write") != 0))
                        continue;
                    if (disabled_row_count
                        >= sizeof(disabled_rows) / sizeof(disabled_rows[0]))
                        continue;
                    disabled_rows[disabled_row_count++] = inst;
                    inst->resource_runtime_fact_present = false;
                }
            }
            rejected_missing_owner =
                mir_abi_resource_runtime_pin_owner_for_mir(
                    pin, MIR_RESOURCE_ABI_SECURE_SLOT, "Int") == NULL
                && mir_abi_resource_runtime_pin_row_for_mir(
                    pin, MIR_RESOURCE_ABI_SECURE_SLOT, "Int",
                    "PinReadInit") == NULL;
            for (size_t i = 0; i < disabled_row_count; i++)
                disabled_rows[i]->resource_runtime_fact_present = true;
        }
        validation_ok = mir_validate(mir, &validation_error);
        if (!validation_ok && validation_error != NULL)
            fprintf(stderr, "pin-row validation: %s\n", validation_error);
        EXPECT(ok && pin != NULL && owner != NULL && pin_row != NULL
               && strcmp(pin_row->resource_op_name, "PinReadInit") == 0
               && pin_row->runtime_call_abi_id != 0
               && rejected_missing_owner
               && validation_ok);
        free(validation_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR synthetic slot consumers derive rows from an existing owner");
    {
        const char *src =
            "func Main() -> Void {\n"
            "    let ss: SecureSlot<Int> = ClaimSecureSlot<Int>(1);\n"
            "    Write(ss, 42, ss_token);\n"
            "    Log(ss);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *main_routine = NULL;
        const MIRInstruction *owner = NULL;
        const MIRResourceRuntimeRow *read_row = NULL;
        MIRInstruction *disabled_rows[16];
        size_t disabled_row_count = 0;
        char *validation_error = NULL;
        bool rejected_without_owner = false;
        bool validation_ok = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            main_routine = find_mir_routine_mut(
                mir, "Main", MIR_SCOPE_FUNCTION);
        if (main_routine != NULL) {
            owner = mir_abi_resource_runtime_owner_for_mir_abi(
                main_routine, MIR_RESOURCE_ABI_SECURE_SLOT, "Int");
            read_row = mir_abi_resource_runtime_row_for_mir_abi(
                main_routine, MIR_RESOURCE_ABI_SECURE_SLOT, "Int", "Read");
        }
        if (main_routine != NULL) {
            for (size_t bi = 0; bi < main_routine->block_count; bi++) {
                MIRBasicBlock *block = &main_routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    const MIRResourceRuntimeRow *row =
                        &inst->resource_runtime_fact;
                    if (inst->resource_runtime_fact_present
                        && row->abi_type_name != NULL
                        && strcmp(row->abi_type_name, "SecureSlot<Int>") == 0
                        && disabled_row_count
                            < sizeof(disabled_rows) / sizeof(disabled_rows[0])) {
                        disabled_rows[disabled_row_count++] = inst;
                        inst->resource_runtime_fact_present = false;
                    }
                }
            }
            rejected_without_owner =
                mir_abi_resource_runtime_owner_for_mir_abi(
                    main_routine, MIR_RESOURCE_ABI_SECURE_SLOT, "Int") == NULL
                && mir_abi_resource_runtime_row_for_mir_abi(
                    main_routine, MIR_RESOURCE_ABI_SECURE_SLOT, "Int",
                    "Read") == NULL;
        }
        for (size_t i = 0; i < disabled_row_count; i++)
            disabled_rows[i]->resource_runtime_fact_present = true;
        validation_ok = mir_validate(mir, &validation_error);
        if (!validation_ok && validation_error != NULL)
            fprintf(stderr, "synthetic-row validation: %s\n",
                    validation_error);
        EXPECT(ok && main_routine != NULL && owner != NULL && read_row != NULL
               && strcmp(read_row->resource_op_name, "Read") == 0
               && strcmp(read_row->runtime_fn, "pgy_secure_read_Int") == 0
               && read_row->runtime_call_abi_id != 0
               && rejected_without_owner
               && validation_ok);
        free(validation_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR slot-sugar DEF carries a canonical Claim runtime row");
    {
        const char *src =
            "struct Vec2 { x: Int; y: Int; }\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(1, 2);\n"
            "    Log(Read(s));\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *main_routine = NULL;
        MIRInstruction *claim_def = NULL;
        const MIRResourceRuntimeRow *derived_write = NULL;
        char *mir_error = NULL;
        bool rejected_payload = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok)
            main_routine = find_mir_routine_mut(
                mir, "Main", MIR_SCOPE_FUNCTION);
        if (main_routine != NULL) {
            for (size_t bi = 0; bi < main_routine->block_count; bi++) {
                MIRBasicBlock *block = &main_routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->abi_type_name != NULL
                        && strcmp(inst->abi_type_name, "Slot<Vec2>") == 0
                        && inst->resource_runtime_fact_present) {
                        claim_def = inst;
                        break;
                    }
                }
                if (claim_def != NULL)
                    break;
            }
        }
        if (claim_def != NULL) {
            derived_write = mir_abi_resource_runtime_row_for_mir_abi(
                main_routine, MIR_RESOURCE_ABI_SLOT, "Vec2", "Write");
            const char *saved_symbol =
                claim_def->resource_runtime_fact.runtime_fn;
            claim_def->resource_runtime_fact.runtime_fn = "pgy_wrong_Vec2";
            rejected_payload = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                    "carries an invalid runtime-call ABI row") != NULL;
            claim_def->resource_runtime_fact.runtime_fn = saved_symbol;
        }
        EXPECT(ok && main_routine != NULL && claim_def != NULL
               && strcmp(claim_def->resource_runtime_fact.resource_op_name,
                         "Claim") == 0
               && strcmp(claim_def->resource_runtime_fact.runtime_fn,
                         "pgy_claim_Vec2") == 0
               && claim_def->resource_runtime_fact.runtime_call_abi_id != 0
               && derived_write != NULL
               && strcmp(derived_write->runtime_fn,
                         "pgy_write_Vec2") == 0
               && rejected_payload
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
