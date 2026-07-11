#ifndef PGY_RUNTIME_LIB_TEXT_BUILDER_EXPORTS_H
#define PGY_RUNTIME_LIB_TEXT_BUILDER_EXPORTS_H

#include "pgy_runtime_text_builder_inline.h"

void
pgy_text_builder_new_export(PgyTextBuilder *out,
                            int64_t initial_capacity)
{
    if (out != NULL)
        *out = pgy_text_builder_new(initial_capacity);
}

void
pgy_text_builder_append_export(PgyTextBuilder *builder, const char *text)
{
    pgy_text_builder_append(builder, text);
}

char *
pgy_text_builder_finish_export(PgyTextBuilder *builder,
                               PgyAllocator *result_allocator)
{
    return pgy_text_builder_finish(builder, result_allocator);
}

void
pgy_text_builder_drop_export(PgyTextBuilder *builder)
{
    pgy_text_builder_drop(builder);
}

#endif /* PGY_RUNTIME_LIB_TEXT_BUILDER_EXPORTS_H */
