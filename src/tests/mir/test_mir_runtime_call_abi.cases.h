static void
test_mir_runtime_call_abi_facts(void)
{
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
                                      "container_ptr_value_to_void") == 0;
                    if (strcmp(inst->name, "Release") == 0)
                        saw_release = inst->resource_runtime_fact_present
                            && strcmp(inst->resource_runtime_fact.runtime_fn,
                                      "pgy_release_Vec2") == 0;
                }
            }
        }
        EXPECT(ok && mir_validate(mir, NULL) && flow != NULL
               && saw_write && saw_release);
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
}
