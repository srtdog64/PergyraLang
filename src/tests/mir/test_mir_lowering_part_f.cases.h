static void
test_mir_lowering_part_f(void)
{
    TEST("MIR DCE removes dead pure-query statements while preserving routine validity");
    {
        const char *src =
            "func Probe(ch: Channel<Int>) -> Int {\n"
            "    ChannelLength(ch);\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *probe = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            probe = find_mir_routine(mir, "Probe", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && probe != NULL
               && !probe->used_non_cfg_body_fallback
               && probe->non_cfg_body_fallback_count == 0
               && probe->has_dce
               && probe->dce_removed_count > 0
               && !routine_has_stmt_call_named(probe, "ChannelLength"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
