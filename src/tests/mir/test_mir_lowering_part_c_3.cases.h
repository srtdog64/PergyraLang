static void
test_mir_lowering_part_c_3(void)
{
    TEST("MIR captures array literal source-local types");
    {
        const char *src =
            "func ArrayLiteralLocalFacts() -> Int {\n"
            "    let values = [1, 2, 3];\n"
            "    return ArrayLength(values);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *values_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ArrayLiteralLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            values_type = mir_routine_source_local_type_name(routine,
                "values");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && values_type != NULL
               && strcmp(values_type, "Array<Int>") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures zone constructor source-local types");
    {
        const char *src =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "}\n"
            "object DriverView {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    object slot dashboard: DriverView = DriverView(0)\n"
            "}\n"
            "func ZoneConstructorLocalFacts() -> Int {\n"
            "    let cockpit = CockpitZone(Driver(5), DriverView(0));\n"
            "    return cockpit.driver.hp;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *cockpit_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ZoneConstructorLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            cockpit_type = mir_routine_source_local_type_name(routine,
                "cockpit");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && cockpit_type != NULL
               && strcmp(cockpit_type, "CockpitZone") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures intent call source-local types");
    {
        const char *src =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "}\n"
            "object DriverView {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    object slot dashboard: DriverView = DriverView(0)\n"
            "}\n"
            "intent SyncDrive(cockpit: CockpitZone, driver: Driver) {\n"
            "    step verify {\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func IntentCallLocalFacts() -> Int {\n"
            "    let cockpit = CockpitZone(Driver(5), DriverView(0));\n"
            "    let ok = SyncDrive(cockpit, cockpit.driver);\n"
            "    if ok {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *ok_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "IntentCallLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            ok_type = mir_routine_source_local_type_name(routine, "ok");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && ok_type != NULL
               && strcmp(ok_type, "Bool") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures generic spawn and await source-local types");
    {
        const char *src =
            "func Identity<T>(x: T) -> T {\n"
            "    return x;\n"
            "}\n"
            "async func Main() -> Void {\n"
            "    let task = spawn Identity(42);\n"
            "    let value = await task;\n"
            "    Log(value);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *task_type = NULL;
        const char *value_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            task_type = mir_routine_source_local_type_name(routine, "task");
            value_type = mir_routine_source_local_type_name(routine, "value");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && task_type != NULL && strcmp(task_type, "Future<Int>") == 0
               && value_type != NULL && strcmp(value_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures extern call source-local types");
    {
        const char *src =
            "extern \"c\" {\n"
            "    func pgy_now_ms() -> Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let ignored = pgy_now_ms();\n"
            "    Log(1);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *ignored_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            ignored_type = mir_routine_source_local_type_name(routine,
                "ignored");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && ignored_type != NULL
               && strcmp(ignored_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
