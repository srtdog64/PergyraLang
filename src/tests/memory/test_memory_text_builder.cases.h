static void
test_text_builder_lifetime(void)
{
    printf("\n[text_builder_lifetime]\n");

    TEST("TextBuilder runtime layout matches the ABI owner row");
    EXPECT(sizeof(PgyTextBuilder) == sizeof(pgy_abi_text_builder)
           && offsetof(PgyTextBuilder, data)
                == offsetof(pgy_abi_text_builder, data)
           && offsetof(PgyTextBuilder, length)
                == offsetof(pgy_abi_text_builder, length)
           && offsetof(PgyTextBuilder, capacity)
                == offsetof(pgy_abi_text_builder, capacity)
           && offsetof(PgyTextBuilder, finished)
                == offsetof(pgy_abi_text_builder, finished));

    EXPECT_PANIC("TextBuilder rejects negative capacity before allocation", {
        (void)pgy_text_builder_new(-1);
    });

    TEST("TextBuilder promotes one result and releases intermediate storage");
    {
        PgyAllocator result_allocator = pgy_allocator_result();
        PgyTextBuilder builder = pgy_text_builder_new(4);
        char *result;

        pgy_text_builder_append(&builder, "Pergyra");
        pgy_text_builder_append_n(&builder, "Lang!", 4);
        result = pgy_text_builder_finish(&builder, &result_allocator);
        EXPECT(strcmp(result, "PergyraLang") == 0
               && builder.finished
               && builder.data == NULL
               && result_allocator.bytes_in_use == strlen(result) + 1);
        pgy_free(&result_allocator, result, strlen(result) + 1);
    }

    TEST("TextBuilder drop releases owned storage");
    {
        PgyTextBuilder builder = pgy_text_builder_new(8);

        pgy_text_builder_append(&builder, "temporary");
        pgy_text_builder_drop(&builder);
        EXPECT(builder.finished && builder.data == NULL);
    }
}
