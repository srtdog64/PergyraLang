#include "mir_abi_layout.h"

#include <string.h>

static const MIRTextBuilderRuntimeRow k_text_builder_rows[] = {
    { "Allocator", "AllocatorDestroy", "Destroy", "pgy_allocator_destroy",
      "pgy_allocator_destroy_export",
      MIR_TEXT_BUILDER_CALL_ALLOCATOR_PTR_TO_VOID,
      MIR_TEXT_BUILDER_CALL_ALLOCATOR_PTR_TO_VOID, "mir_builtin_abi_row" },
    { "Allocator", "AllocatorResult", "Result", "pgy_allocator_result",
      "pgy_allocator_result_init",
      MIR_TEXT_BUILDER_CALL_RETURNS_ALLOCATOR,
      MIR_TEXT_BUILDER_CALL_ALLOCATOR_OUT_TO_VOID, "mir_builtin_abi_row" },
    { "TextBuilder", "TextBuilderAppend", "Append", "pgy_text_builder_append",
      "pgy_text_builder_append_export",
      MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID,
      MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID, "mir_builtin_abi_row" },
    { "TextBuilder", "TextBuilderDrop", "Drop", "pgy_text_builder_drop",
      "pgy_text_builder_drop_export",
      MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID,
      MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID, "mir_builtin_abi_row" },
    { "TextBuilder", "TextBuilderFinish", "Finish", "pgy_text_builder_finish",
      "pgy_text_builder_finish_export",
      MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING,
      MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING,
      "mir_builtin_abi_row" },
    { "TextBuilder", "TextBuilderNew", "New", "pgy_text_builder_new",
      "pgy_text_builder_new_export",
      MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER,
      MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID, "mir_builtin_abi_row" },
};

const MIRTextBuilderRuntimeRow *
mir_text_builder_runtime_row(const char *operation)
{
    size_t count = mir_text_builder_runtime_row_count();

    if (operation == NULL)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(k_text_builder_rows[i].operation, operation) == 0)
            return &k_text_builder_rows[i];
    }
    return NULL;
}

const MIRTextBuilderRuntimeRow *
mir_text_builder_runtime_row_by_source_name(const char *source_name)
{
    size_t count = mir_text_builder_runtime_row_count();

    if (source_name == NULL)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(k_text_builder_rows[i].source_name, source_name) == 0)
            return &k_text_builder_rows[i];
    }
    return NULL;
}

const MIRTextBuilderRuntimeRow *
mir_text_builder_runtime_row_at(size_t index)
{
    return index < mir_text_builder_runtime_row_count()
        ? &k_text_builder_rows[index] : NULL;
}

size_t
mir_text_builder_runtime_row_count(void)
{
    return sizeof(k_text_builder_rows) / sizeof(k_text_builder_rows[0]);
}

const char *
mir_text_builder_call_shape_name(MIRTextBuilderCallShape shape)
{
    switch (shape) {
    case MIR_TEXT_BUILDER_CALL_RETURNS_ALLOCATOR:
        return "returns_allocator";
    case MIR_TEXT_BUILDER_CALL_ALLOCATOR_OUT_TO_VOID:
        return "allocator_out_to_void";
    case MIR_TEXT_BUILDER_CALL_ALLOCATOR_PTR_TO_VOID:
        return "allocator_ptr_to_void";
    case MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER:
        return "capacity_to_builder";
    case MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID:
        return "builder_out_capacity_to_void";
    case MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID:
        return "builder_ptr_string_to_void";
    case MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING:
        return "builder_ptr_allocator_ptr_to_string";
    case MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID:
        return "builder_ptr_to_void";
    }
    return "invalid";
}
