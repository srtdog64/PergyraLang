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
