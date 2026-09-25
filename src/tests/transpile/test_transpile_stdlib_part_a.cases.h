/* Lower `source` through the MIR pipeline and emit its C program. Slot
 * runtime calls come from MIR runtime-call rows, so these cases go through
 * the same lowering as a real compile. Returns NULL on any failure. */
static char *
slot_sugar_c_from_source(const char *source)
{
    ASTNode *program = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    char *out = NULL;
    TranspilerCtx *ctx;

    if (lower_pipeline_from_source(source, &program, &hir, &rir, &mir)) {
        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);
        if (ctx->backend_error == NULL && ctx->out->data != NULL)
            out = pergyra_strdup(ctx->out->data);
        transpiler_ctx_destroy(ctx);
    }
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    ast_destroy(program);
    return out;
}

static void
test_slot_sugar(void)
{
    printf("\n[slot_sugar]\n");

    TEST("let x: Slot<Int> = 42 -> claim + write");
    {
        char *out = slot_sugar_c_from_source(
            "func Main() -> Void {\n"
            "    let x: Slot<Int> = 42;\n"
            "    Release(x);\n"
            "}\n");
        EXPECT(out != NULL && strstr(out, "pgy_claim_Int()") != NULL);
        EXPECT(out != NULL && strstr(out, "pgy_write_Int(&x, 42)") != NULL);
        free(out);
    }

    TEST("slot sugar: identifier auto-read via Log");
    {
        char *out = slot_sugar_c_from_source(
            "func Main() -> Void {\n"
            "    let x: Slot<Int> = 42;\n"
            "    Log(x);\n"
            "    Release(x);\n"
            "}\n");
        EXPECT(out != NULL && strstr(out, "pgy_read_Int(&x)") != NULL);
        free(out);
    }

    TEST("slot sugar: x = 5 auto-write");
    {
        char *out = slot_sugar_c_from_source(
            "func Main() -> Void {\n"
            "    let x: Slot<Int> = 42;\n"
            "    x = 5;\n"
            "    Release(x);\n"
            "}\n");
        EXPECT(out != NULL && strstr(out, "pgy_write_Int(&x, 5)") != NULL);
        free(out);
    }

    TEST("explicit Release prevents double release");
    {
        char *out = slot_sugar_c_from_source(
            "func Main() -> Void {\n"
            "    let a: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(a, 10);\n"
            "    Release(a);\n"
            "}\n");
        int count = 0;
        const char *cursor = out;
        while (cursor != NULL
               && (cursor = strstr(cursor, "pgy_release_Int")) != NULL) {
            count++;
            cursor++;
        }
        EXPECT(out != NULL && count == 1);
        free(out);
    }

    TEST("pin block emits a typed pinned view that is released on exit");
    {
        char *out = slot_sugar_c_from_source(
            "func Main() -> Void {\n"
            "    let x: Slot<Int> = 42;\n"
            "    pin x as view: WriteView<Int> {\n"
            "        Write(view, 7);\n"
            "    }\n"
            "    Release(x);\n"
            "}\n");
        EXPECT(out != NULL && strstr(out, "PgyPinnedSlotView_Int") != NULL);
        EXPECT(out != NULL && strstr(out, ".can_write = true") != NULL);
        EXPECT(out != NULL && strstr(out, ".active = false;") != NULL);
        EXPECT(out != NULL && strstr(out, "pgy_write_Int(&x, 7)") != NULL);
        free(out);
    }
}

