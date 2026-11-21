/* Machine contact facts are attached from the RIR contact op's identity to
 * the resource-op MIR lowers from it, and to nothing else. */

static void
test_mir_machine_contact_identity(void)
{
    const char *src =
        "func Sum() -> Int {\n"
        "    let dev: DeviceSlot<Int> = ClaimDeviceSlot();\n"
        "    let other: DeviceSlot<Int> = ClaimDeviceSlot();\n"
        "    let sum: Int = DeviceRead(dev) + DeviceRead(other);\n"
        "    DeviceWrite(dev, sum);\n"
        "    ReleaseDeviceSlot(dev);\n"
        "    ReleaseDeviceSlot(other);\n"
        "    return sum;\n"
        "}\n";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    MIRRoutine *routine = NULL;
    MIRInstruction *read_op = NULL;
    MIRInstruction *write_op = NULL;
    MIRInstruction *write_stmt = NULL;
    size_t contact_ops = 0;
    size_t facts = 0;
    bool identity = true;
    bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

    if (ok)
        routine = find_mir_routine_mut(mir, "Sum", MIR_SCOPE_FUNCTION);
    for (size_t bi = 0; routine != NULL && bi < routine->block_count; bi++) {
        MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            MIRInstruction *inst = &block->instructions[ii];
            bool contact_op = inst->kind == MIR_INST_RESOURCE_OP
                && inst->rir_op != NULL
                && rir_machine_contact_kind_is_present(
                    inst->rir_op->machine_contact_kind);
            if (contact_op)
                contact_ops++;
            if (inst->machine_layer_fact_present) {
                facts++;
                identity = identity && contact_op
                    && inst->machine_contact_kind
                        == inst->rir_op->machine_contact_kind
                    && mir_machine_layer_fact_is_valid(inst);
            }
            if (contact_op && read_op == NULL
                && inst->machine_contact_kind == RIR_MACHINE_CONTACT_READ)
                read_op = inst;
            if (contact_op
                && inst->machine_contact_kind == RIR_MACHINE_CONTACT_WRITE)
                write_op = inst;
            if (inst->kind == MIR_INST_STMT && inst->arg0 != NULL
                && strcmp(inst->arg0, "DeviceWrite") == 0)
                write_stmt = inst;
        }
    }

    TEST("MIR attaches a machine fact to each contact resource-op and nothing else");
    EXPECT(ok && routine != NULL && identity
           && contact_ops == 7 && facts == contact_ops
           && read_op != NULL && write_op != NULL && write_stmt != NULL
           && mir_validate(mir, NULL));

    TEST("MIR validation refuses a machine fact on the statement around a contact");
    if (write_op != NULL && write_stmt != NULL) {
        MIRInstruction saved = *write_stmt;
        char *error = NULL;
        bool refused;
        bool attached = mir_attach_machine_layer_fact(write_stmt,
                                                      write_op->rir_op);
        refused = attached && !mir_validate(mir, &error) && error != NULL
            && strstr(error,
                "carries a machine-layer fact without a machine contact op")
                != NULL;
        free(error);
        *write_stmt = saved;
        EXPECT(refused);
    } else {
        EXPECT(false);
    }

    TEST("MIR validation refuses a contact resource-op without its fact");
    if (read_op != NULL) {
        MIRInstruction saved = *read_op;
        char *error = NULL;
        bool refused;
        read_op->machine_layer_fact_present = false;
        refused = !mir_validate(mir, &error) && error != NULL
            && strstr(error, "is missing valid machine-layer fact") != NULL;
        free(error);
        *read_op = saved;
        EXPECT(refused);
    } else {
        EXPECT(false);
    }

    TEST("MIR validation refuses a contact fact crossed to another contact op");
    if (read_op != NULL && write_op != NULL) {
        MIRInstruction saved = *read_op;
        char *error = NULL;
        bool refused;
        bool attached = mir_attach_machine_layer_fact(read_op,
                                                      write_op->rir_op);
        /* The crossed fact names the Write runtime operation, which the
         * Read op's lowered runtime-call row does not carry. */
        refused = attached && mir_machine_layer_fact_is_valid(read_op)
            && !mir_validate(mir, &error) && error != NULL
            && strstr(error,
                "resource op is missing lowered runtime-call ABI row fact")
                != NULL;
        free(error);
        *read_op = saved;
        EXPECT(refused && mir_validate(mir, NULL));
    } else {
        EXPECT(false);
    }

    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}
