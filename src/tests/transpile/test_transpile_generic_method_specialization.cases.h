#include "../../codegen/llvm_backend.h"

static void
test_generic_method_specialization_fact(void)
{
    const char *source =
        "class Box {\n"
        "  let marker: Int;\n"
        "  func Echo<T>(self, value: T) -> T { return value; }\n"
        "}\n"
        "func Main() -> Void {\n"
        "  let box: Box = Box(0);\n"
        "  let result: Int = box.Echo<Int>(41);\n"
        "  Log(ToString(result));\n"
        "}\n";
    ASTNode *program = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    char output_path[1024];

    make_tmp_path(output_path, sizeof(output_path),
        "pgy_generic_method_specialization.c");

    printf("\n[generic_method_specialization]\n");

    TEST("MIR generic method specialization row drives C symbol and body");
    if (!lower_pipeline_from_source(source, &program, &hir, &rir, &mir)) {
        EXPECT(false);
    } else {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, 0);
        TranspileResult *result =
            transpile_mir_with_test_evidence(mir, output_path);
        char *generated = read_file_text(output_path);
        bool specialization_ok =
            mir_generic_method_specialization_count(mir) == 1
            && fact != NULL
            && fact->specialized_name != NULL
            && strcmp(fact->specialized_name, "Box_Echo_Int") == 0
            && result != NULL && result->success
            && generated != NULL
            && strstr(generated, "Box_Echo_Int(") != NULL
            && strstr(generated, "T Box_Echo(") == NULL;

        if (!specialization_ok) {
            fprintf(stderr,
                "[generic-method-specialization] count=%zu name=%s "
                "success=%d error=%s has_specialized=%d has_formal=%d\n",
                mir_generic_method_specialization_count(mir),
                fact != NULL && fact->specialized_name != NULL
                    ? fact->specialized_name : "<missing>",
                result != NULL && result->success ? 1 : 0,
                result != NULL && result->error_message != NULL
                    ? result->error_message : "<none>",
                generated != NULL
                    && strstr(generated, "Box_Echo_Int(") != NULL ? 1 : 0,
                generated != NULL
                    && strstr(generated, "T Box_Echo(") != NULL ? 1 : 0);
        }

        EXPECT(specialization_ok);

        free(generated);
        transpile_result_destroy(result);
        remove(output_path);
    }

    TEST("MIR generic member actual type rejects a residual formal token");
    if (mir == NULL || mir->generic_method_specialization_count == 0) {
        EXPECT(false);
    } else {
        MIRGenericMethodSpecializationFact *fact =
            &mir->generic_method_specializations[0];
        char *saved_actual = fact->actual_type_names[0];
        char *residual_actual = pergyra_strdup("Option<T>");
        char *validation_error = NULL;

        fact->actual_type_names[0] = residual_actual;
        EXPECT(!mir_generic_method_specializations_validate(
                mir, &validation_error)
            && validation_error != NULL
            && strstr(validation_error,
                "retains unresolved formal 'T'") != NULL);
        free(validation_error);
        fact->actual_type_names[0] = saved_actual;
        free(residual_actual);
    }

    TEST("C generic member call fails closed when MIR specialization row is missing");
    if (mir == NULL) {
        EXPECT(false);
    } else {
        size_t saved_count = mir->generic_method_specialization_count;
        TranspileResult *result;

        mir->generic_method_specialization_count = 0;
        result = transpile_mir_with_test_evidence(mir, output_path);
        EXPECT(result != NULL && !result->success
            && result->error_message != NULL
            && strstr(result->error_message,
                "missing generic member-call specialization fact") != NULL);
        mir->generic_method_specialization_count = saved_count;
        transpile_result_destroy(result);
        remove(output_path);
    }

    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    ast_destroy(program);
}

static void
test_generic_direct_specialization_fact(void)
{
    const char *source =
        "subject Card { let value: Int; }\n"
        "func Rank<T>(value: T) -> Int { return 1; }\n"
        "func Forward<T>(value: T) -> Int { return Rank(value); }\n"
        "func Main() -> Void {\n"
        "  Log(ToString(Rank(Card(7))));\n"
        "  Log(ToString(Forward(Card(8))));\n"
        "}\n";
    ASTNode *program = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    TEST("MIR direct generic bindings retain nominal and caller-formal identity");
    bool lowered = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
    EXPECT(lowered && mir_generic_method_specialization_count(mir) == 3);
    if (lowered) {
        size_t nominal = 0, deferred = 0;
        for (size_t i = 0; i < mir_generic_method_specialization_count(mir); i++) {
            const MIRGenericMethodSpecializationFact *fact =
                mir_generic_method_specialization_at(mir, i);
            if (fact->binding_count == 1 && strcmp(fact->actual_type_names[0], "Card") == 0)
                nominal++;
            if (fact->binding_count == 1 && strcmp(fact->actual_type_names[0], "T") == 0
                && strcmp(mir->routines[fact->caller_routine_index].name, "Forward") == 0)
                deferred++;
        }
        TEST("MIR nested direct call defers only the caller's declared formal");
        EXPECT(nominal == 2 && deferred == 1);
        MIRGenericMethodSpecializationFact *fact = &mir->generic_method_specializations[0];
        size_t saved_caller = fact->caller_routine_index;
        for (size_t i = 0; i < mir->routine_count; i++)
            if (strcmp(mir->routines[i].name, "Main") == 0) fact->caller_routine_index = i;
        char *error = NULL;
        TEST("MIR forwarded formal cannot be reattributed to a nongeneric caller");
        EXPECT(!mir_generic_method_specializations_validate(mir, &error)
            && error != NULL && strstr(error, "retains unresolved formal") != NULL);
        free(error);
        fact->caller_routine_index = saved_caller;
        char output_path[1024];
        make_tmp_path(output_path, sizeof(output_path), "pgy_generic_direct_specialization.c");
        TEST("C nominal/forward generic execution consumes MIR bindings");
        TranspileResult *c_result = transpile_mir_with_test_evidence(mir, output_path);
        char *generated = read_file_text(output_path);
        EXPECT(c_result != NULL && c_result->success && generated != NULL
            && strstr(generated, "Rank_Card(Card *value)") != NULL
            && strstr(generated, "Forward_Card(Card *value)") != NULL
            && strstr(generated, "Rank_Card(value)") != NULL
            && strstr(generated, "&(\u0028Card){ .value = 7 })") != NULL);
        free(generated);
        transpile_result_destroy(c_result);
        remove(output_path);
        TEST("C direct generic call refuses erased specialization rows");
        size_t c_saved_count = mir->generic_method_specialization_count;
        mir->generic_method_specialization_count = 0;
        c_result = transpile_mir_with_test_evidence(mir, output_path);
        EXPECT(c_result != NULL && !c_result->success && c_result->error_message != NULL
            && strstr(c_result->error_message, "MIR") != NULL
            && strstr(c_result->error_message, "specialization") != NULL);
        mir->generic_method_specialization_count = c_saved_count;
        transpile_result_destroy(c_result);
        remove(output_path);
#ifdef PGY_LLVM_ENABLED
        AIRProgram air = {0};
        air.has_hir_input = true;
        air.has_rir_input = true;
        air.has_mir_input = true;
        PgyVerifiedProjectionPlanRow plan = {0};
        const char *plan_error = NULL;
        bool ready = pgy_air_evidence_certificate_issue(&air, &plan_error)
            && pgy_verified_projection_plan_intent_observability_with_air(
                &air, mir, PGY_PROJECTION_TARGET_LLVM, &plan, &plan_error);
        TEST("LLVM nominal/forward generic execution consumes MIR bindings");
        LLVMGenResult *result = ready ? llvm_codegen_from_mir_with_projection_plans(
            mir, &plan, &test_empty_parallel_capture_plan, &test_empty_spawn_lane_plan,
            NULL, "generic_direct_fact") : NULL;
        if (result != NULL && !result->success)
            fprintf(stderr, "[generic-direct] %s\n", result->error_message);
        EXPECT(result != NULL && result->success && result->ir_text != NULL
            && strstr(result->ir_text, "@Rank_Card") != NULL
            && strstr(result->ir_text, "@Forward_Card") != NULL);
        llvm_gen_result_destroy(result);
        TEST("LLVM direct generic call refuses erased specialization rows");
        size_t saved_count = mir->generic_method_specialization_count;
        mir->generic_method_specialization_count = 0;
        result = ready ? llvm_codegen_from_mir_with_projection_plans(
            mir, &plan, &test_empty_parallel_capture_plan, &test_empty_spawn_lane_plan,
            NULL, "generic_direct_missing") : NULL;
        EXPECT(result != NULL && !result->success && result->error_message != NULL
            && strstr(result->error_message, "missing generic call specialization fact") != NULL);
        mir->generic_method_specialization_count = saved_count;
        llvm_gen_result_destroy(result);
#endif
    }
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    ast_destroy(program);
}
