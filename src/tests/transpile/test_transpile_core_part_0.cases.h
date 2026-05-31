static void
test_codebuf(void)
{
    printf("\n[codebuf]\n");

    TEST("write simple string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "hello");
        EXPECT(strcmp(b->data, "hello") == 0 && b->len == 5);
        codebuf_destroy(b);
    }

    TEST("write formatted string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "int x = %d;", 42);
        EXPECT(strcmp(b->data, "int x = 42;") == 0);
        codebuf_destroy(b);
    }

    TEST("write triggers growth beyond initial capacity");
    {
        CodeBuf *b = codebuf_create();
        for (int i = 0; i < 1000; i++)
            codebuf_write(b, "a");
        EXPECT(b->len == 1000);
        codebuf_destroy(b);
    }

    TEST("multiple writes concatenate correctly");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "foo");
        codebuf_write(b, "bar");
        codebuf_write(b, "baz");
        EXPECT(strcmp(b->data, "foobarbaz") == 0);
        codebuf_destroy(b);
    }
}

/* -----------------------------------------------------------------
 * Tests: type mapping
 * ----------------------------------------------------------------- */

static bool
expect_type_to_c_copy(const char *input, const char *expected)
{
    char out[256];

    return pergyra_type_to_c_copy(input, out, sizeof(out))
        && strcmp(out, expected) == 0;
}

static bool
expect_type_to_c_rejects(const char *input)
{
    char out[256];

    out[0] = 'x';
    return !pergyra_type_to_c_copy(input, out, sizeof(out));
}

static void
test_type_mapping(void)
{
    printf("\n[type_mapping]\n");

    TEST("Int -> int32_t");
    EXPECT(expect_type_to_c_copy("Int", "int32_t"));

    TEST("Long -> int64_t");
    EXPECT(expect_type_to_c_copy("Long", "int64_t"));

    TEST("Float -> float");
    EXPECT(expect_type_to_c_copy("Float", "float"));

    TEST("Bool -> bool");
    EXPECT(expect_type_to_c_copy("Bool", "bool"));

    TEST("String -> char*");
    EXPECT(expect_type_to_c_copy("String", "char*"));

    TEST("Void -> void");
    EXPECT(expect_type_to_c_copy("Void", "void"));

    TEST("Slot<Int> -> PgySlot_Int");
    EXPECT(expect_type_to_c_copy("Slot<Int>", "PgySlot_Int"));

    TEST("Slot<String> -> PgySlot_String");
    EXPECT(expect_type_to_c_copy("Slot<String>", "PgySlot_String"));

    TEST("Slot<Vec2> -> PgySlot_Vec2");
    EXPECT(expect_type_to_c_copy("Slot<Vec2>", "PgySlot_Vec2"));

    TEST("SecureSlot<Int> -> PgySecureSlot_Int");
    EXPECT(expect_type_to_c_copy("SecureSlot<Int>", "PgySecureSlot_Int"));

    TEST("SecureSlot<Vec2> -> PgySecureSlot_Vec2");
    EXPECT(expect_type_to_c_copy("SecureSlot<Vec2>", "PgySecureSlot_Vec2"));

    TEST("Array<Vertex> -> PgyArray_Vertex");
    EXPECT(expect_type_to_c_copy("Array<Vertex>", "PgyArray_Vertex"));

    TEST("Slice<Vertex> -> PgySlice_Vertex");
    EXPECT(expect_type_to_c_copy("Slice<Vertex>", "PgySlice_Vertex"));

    TEST("List<Vertex> -> PgyList_Vertex");
    EXPECT(expect_type_to_c_copy("List<Vertex>", "PgyList_Vertex"));

    TEST("Queue<Vertex> -> PgyQueue_Vertex");
    EXPECT(expect_type_to_c_copy("Queue<Vertex>", "PgyQueue_Vertex"));

    TEST("Channel<Int> -> PgyChannel_Int");
    EXPECT(expect_type_to_c_copy("Channel<Int>", "PgyChannel_Int"));

    TEST("Channel<String> -> PgyChannel_String");
    EXPECT(expect_type_to_c_copy("Channel<String>", "PgyChannel_String"));

    TEST("Rc<Int> -> PgyRc_Int");
    EXPECT(expect_type_to_c_copy("Rc<Int>", "PgyRc_Int"));

    TEST("Weak<Int> -> PgyWeak_Int");
    EXPECT(expect_type_to_c_copy("Weak<Int>", "PgyWeak_Int"));

    TEST("Allocator -> PgyAllocator");
    EXPECT(expect_type_to_c_copy("Allocator", "PgyAllocator"));

    TEST("Box<Array<Int>> -> PgyBoxArray_Int");
    EXPECT(expect_type_to_c_copy("Box<Array<Int>>", "PgyBoxArray_Int"));

    TEST("pergyra_type_to_c_copy preserves rendered type across later mapping");
    {
        char copied[128];
        bool copied_ok = pergyra_type_to_c_copy("Array<Vertex>",
            copied, sizeof(copied));

        {
            char later[128];
            (void)pergyra_type_to_c_copy("Slot<Int>", later, sizeof(later));
        }
        EXPECT(copied_ok && strcmp(copied, "PgyArray_Vertex") == 0);
    }

    TEST("pergyra_type_to_c_copy fails closed on too-small output buffer");
    {
        char copied[4] = {'x', 'x', 'x', '\0'};

        EXPECT(!pergyra_type_to_c_copy("Array<Vertex>",
            copied, sizeof(copied)));
        EXPECT(copied[0] == '\0');
    }

    TEST("Array<Unknown> fails closed");
    EXPECT(expect_type_to_c_rejects("Array<Unknown>"));

    TEST("Array<Unknown > fails closed");
    EXPECT(expect_type_to_c_rejects("Array<Unknown >"));

    TEST("HashMap<String, Unknown> fails closed");
    EXPECT(expect_type_to_c_rejects("HashMap<String, Unknown>"));

    TEST("Channel<Bool> fails closed without runtime ABI");
    EXPECT(expect_type_to_c_rejects("Channel<Bool>"));

    TEST("Option<Void> fails closed");
    EXPECT(expect_type_to_c_rejects("Option<Void>"));

    TEST("Result<Void> fails closed");
    EXPECT(expect_type_to_c_rejects("Result<Void>"));

    TEST("Box<Array<Unknown>> fails closed");
    EXPECT(expect_type_to_c_rejects("Box<Array<Unknown>>"));

    TEST("Box<Array<UnknownError>> keeps user type name");
    EXPECT(expect_type_to_c_copy("Box<Array<UnknownError>>",
        "PgyBoxArray_UnknownError"));

    TEST("slot_inner_type_name_copy(Slot<Float>) -> Float");
    {
        char inner[128];
        EXPECT(slot_inner_type_name_copy("Slot<Float>", inner, sizeof(inner)));
        EXPECT(strcmp(inner, "Float") == 0);
    }

    TEST("slot_inner_type_name_copy(SecureSlot<Long>) -> Long");
    {
        char inner[128];
        EXPECT(slot_inner_type_name_copy("SecureSlot<Long>", inner, sizeof(inner)));
        EXPECT(strcmp(inner, "Long") == 0);
    }
}
