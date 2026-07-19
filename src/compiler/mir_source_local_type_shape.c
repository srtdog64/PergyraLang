#include "mir_source_local_type_shape.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

char *
mir_source_local_type_scratch_next(MIRSourceLocalTypeScratch *scratch)
{
    char *buffer;

    if (scratch == NULL)
        return NULL;
    buffer = scratch->buffers[scratch->next
        % MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT];
    scratch->next++;
    buffer[0] = '\0';
    return buffer;
}

const char *
mir_source_local_type_scratch_format(MIRSourceLocalTypeScratch *scratch,
                                     const char *outer,
                                     const char *inner)
{
    char *buffer = mir_source_local_type_scratch_next(scratch);

    if (buffer == NULL || outer == NULL || inner == NULL || inner[0] == '\0')
        return NULL;
    if (snprintf(buffer, MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE, "%s<%s>",
            outer, inner) >= MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
}

static bool
mir_source_local_unwrap_prefixed_type(const char *type_name,
                                      const char *const *prefixes,
                                      size_t prefix_count,
                                      char *out,
                                      size_t out_size)
{
    if (type_name == NULL || prefixes == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    for (size_t i = 0; i < prefix_count; i++) {
        const char *prefix = prefixes[i];
        size_t prefix_len = strlen(prefix);
        const char *open;
        const char *close;
        size_t len;

        if (strncmp(type_name, prefix, prefix_len) != 0)
            continue;
        open = type_name + prefix_len;
        close = strrchr(open, '>');
        if (close == NULL || close <= open)
            return false;
        len = (size_t)(close - open);
        if (len >= out_size)
            return false;
        memcpy(out, open, len);
        out[len] = '\0';
        return true;
    }
    return false;
}

bool
mir_source_local_unwrap_slot_like_type(const char *type_name,
                                       char *out,
                                       size_t out_size)
{
    static const char *const prefixes[] = {
        "Slot<",
        "SecureSlot<",
        "DeviceSlot<",
        "PinnedSlotView<",
        "PinnedSecureSlotView<",
        "ReadView<",
        "WriteView<",
    };

    return mir_source_local_unwrap_prefixed_type(type_name, prefixes,
        sizeof(prefixes) / sizeof(prefixes[0]), out, out_size);
}

bool
mir_source_local_unwrap_iterable_type(const char *type_name,
                                      char *out,
                                      size_t out_size)
{
    static const char *const prefixes[] = {
        "Array<",
        "Slice<",
        "List<",
    };

    return mir_source_local_unwrap_prefixed_type(type_name, prefixes,
        sizeof(prefixes) / sizeof(prefixes[0]), out, out_size);
}

bool
mir_source_local_unwrap_channel_type(const char *type_name,
                                     char *out,
                                     size_t out_size)
{
    static const char *const prefixes[] = {"Channel<"};

    return mir_source_local_unwrap_prefixed_type(type_name, prefixes, 1,
        out, out_size);
}

bool
mir_source_local_unwrap_array_or_slice_type(const char *type_name,
                                            char *out,
                                            size_t out_size)
{
    static const char *const prefixes[] = {
        "Array<",
        "Slice<",
    };

    return mir_source_local_unwrap_prefixed_type(type_name, prefixes,
        sizeof(prefixes) / sizeof(prefixes[0]), out, out_size);
}

bool
mir_source_local_tuple_element_type(const char *type_name,
                                    size_t element_index,
                                    char *out,
                                    size_t out_size,
                                    size_t *arity_out)
{
    const char *cursor;
    const char *element_start;
    const char *close;
    size_t angle_depth = 0;
    size_t tuple_depth = 0;
    size_t arity = 0;
    bool found = false;

    if (arity_out != NULL)
        *arity_out = 0;
    if (type_name == NULL || out == NULL || out_size == 0
        || type_name[0] != '(') {
        return false;
    }
    out[0] = '\0';
    close = type_name + strlen(type_name);
    while (close > type_name && isspace((unsigned char)close[-1]))
        close--;
    if (close <= type_name + 1 || close[-1] != ')')
        return false;

    cursor = type_name + 1;
    element_start = cursor;
    for (; cursor < close; cursor++) {
        char ch = *cursor;
        bool boundary = false;

        if (ch == '<')
            angle_depth++;
        else if (ch == '>') {
            if (angle_depth == 0)
                return false;
            angle_depth--;
        } else if (ch == '(')
            tuple_depth++;
        else if (ch == ')') {
            if (tuple_depth == 0) {
                if (cursor != close - 1)
                    return false;
                boundary = true;
            } else {
                tuple_depth--;
            }
        } else if (ch == ',' && angle_depth == 0 && tuple_depth == 0) {
            boundary = true;
        }

        if (boundary) {
            const char *start = element_start;
            const char *end = cursor;
            size_t length;

            while (start < end && isspace((unsigned char)*start))
                start++;
            while (end > start && isspace((unsigned char)end[-1]))
                end--;
            if (start == end)
                return false;
            if (arity == element_index) {
                length = (size_t)(end - start);
                if (length >= out_size)
                    return false;
                memcpy(out, start, length);
                out[length] = '\0';
                found = true;
            }
            arity++;
            element_start = cursor + 1;
        }
    }
    if (angle_depth != 0 || tuple_depth != 0 || element_start != close)
        return false;
    if (arity_out != NULL)
        *arity_out = arity;
    return found;
}

bool
mir_source_local_unwrap_future_type(const char *type_name,
                                    char *out,
                                    size_t out_size,
                                    bool *is_remote_out)
{
    static const char *const future_prefixes[] = {"Future<"};
    static const char *const remote_prefixes[] = {"RemoteFuture<"};

    if (is_remote_out != NULL)
        *is_remote_out = false;
    if (mir_source_local_unwrap_prefixed_type(type_name, future_prefixes, 1,
            out, out_size)) {
        return true;
    }
    if (!mir_source_local_unwrap_prefixed_type(type_name, remote_prefixes, 1,
            out, out_size)) {
        return false;
    }
    if (is_remote_out != NULL)
        *is_remote_out = true;
    return true;
}
