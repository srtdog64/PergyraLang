static void
test_text_builder_lifetime(void)
{
    printf("\n[text_builder_lifetime]\n");

    TEST("TextBuilder promotes one result and releases scratch storage");
    {
        PgyAllocator scratch = pgy_allocator_scratch();
        PgyAllocator result_allocator = pgy_allocator_result();
        PgyTextBuilder builder = pgy_text_builder_new(&scratch, 4);
        char *result;

        pgy_text_builder_append(&builder, "Pergyra");
        pgy_text_builder_append_n(&builder, "Lang!", 4);
        result = pgy_text_builder_finish(&builder, &result_allocator);
        EXPECT(strcmp(result, "PergyraLang") == 0
               && builder.finished
               && builder.data == NULL
               && scratch.bytes_in_use == 0
               && result_allocator.bytes_in_use == strlen(result) + 1);
        pgy_free(&result_allocator, result, strlen(result) + 1);
    }

    TEST("TextBuilder drop is idempotent and releases owned storage");
    {
        PgyAllocator scratch = pgy_allocator_scratch();
        PgyTextBuilder builder = pgy_text_builder_new(&scratch, 8);

        pgy_text_builder_append(&builder, "temporary");
        pgy_text_builder_drop(&builder);
        pgy_text_builder_drop(&builder);
        EXPECT(builder.finished && builder.data == NULL
               && scratch.bytes_in_use == 0);
    }
}
