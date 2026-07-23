#include "mir_source_local_type_shape.h"

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

static bool
mir_source_local_unwrap_first_constructed_type_arg(const char *type_name,
                                                    const char *prefix,
                                                    bool require_separator,
                                                    char *out,
                                                    size_t out_size)
{
    const char *inner;
    const char *end;
    size_t start = 0;
    unsigned depth = 0;
    size_t inner_len;
    bool saw_separator = false;

    if (type_name == NULL || out == NULL || out_size == 0
        || prefix == NULL || strncmp(type_name, prefix, strlen(prefix)) != 0)
        return false;
    inner = type_name + strlen(prefix);
    end = strrchr(inner, '>');
    if (end == NULL || end <= inner)
        return false;
    inner_len = (size_t)(end - inner);
    for (size_t i = 0; i < inner_len; i++) {
        if (inner[i] == '<') {
            depth++;
        } else if (inner[i] == '>') {
            if (depth == 0)
                return false;
            depth--;
        } else if (inner[i] == ',' && depth == 0) {
            size_t arg_start = start;
            size_t arg_end = i;
            while (arg_start < arg_end
                   && (inner[arg_start] == ' '
                       || inner[arg_start] == '\t')) {
                arg_start++;
            }
            while (arg_end > arg_start
                   && (inner[arg_end - 1] == ' '
                       || inner[arg_end - 1] == '\t')) {
                arg_end--;
            }
            if (!saw_separator) {
                size_t len = arg_end - arg_start;
                if (len == 0 || len >= out_size)
                    return false;
                memcpy(out, inner + arg_start, len);
                out[len] = '\0';
            }
            saw_separator = true;
            start = i + 1;
        }
    }
    if (!saw_separator && require_separator)
        return false;
    if (saw_separator)
        return out[0] != '\0';
    {
        size_t arg_start = start;
        size_t arg_end = inner_len;
        size_t len;
        while (arg_start < arg_end
               && (inner[arg_start] == ' '
                   || inner[arg_start] == '\t')) {
            arg_start++;
        }
        while (arg_end > arg_start
               && (inner[arg_end - 1] == ' '
                   || inner[arg_end - 1] == '\t')) {
            arg_end--;
        }
        len = arg_end - arg_start;
        if (len == 0 || len >= out_size)
            return false;
        memcpy(out, inner + arg_start, len);
        out[len] = '\0';
        return true;
    }
}

bool
mir_source_local_unwrap_hash_map_key_type(const char *type_name,
                                          char *out,
                                          size_t out_size)
{
    return mir_source_local_unwrap_first_constructed_type_arg(type_name,
        "HashMap<", true, out, out_size);
}

bool
mir_source_local_unwrap_set_element_type(const char *type_name,
                                         char *out,
                                         size_t out_size)
{
    return mir_source_local_unwrap_first_constructed_type_arg(type_name,
        "Set<", false, out, out_size);
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
