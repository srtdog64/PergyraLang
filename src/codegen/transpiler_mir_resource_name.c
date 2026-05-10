#include "transpiler_mir_resource_name_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SlotRuntimeFnSpec {
    const char *op_name;
    const char *plain_prefix;
    const char *secure_prefix;
    const char *device_prefix;
} SlotRuntimeFnSpec;

static int
slot_runtime_fn_spec_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key,
                  ((const SlotRuntimeFnSpec *)entry)->op_name);
}

const char *
transpiler_extract_type_suffix_from_fn(const char *fn_name)
{
    if (fn_name == NULL)
        return NULL;

    const char *last_underscore = NULL;
    for (const char *p = fn_name; *p; p++) {
        if (*p == '_')
            last_underscore = p;
    }
    if (last_underscore == NULL)
        return NULL;

    return last_underscore + 1;
}

bool
transpiler_format_slot_runtime_fn(const char *op_name,
                                  bool is_secure_slot,
                                  bool is_device_slot,
                                  const char *inner_name,
                                  char *out,
                                  size_t out_size)
{
    static const SlotRuntimeFnSpec specs[] = {
        { "Claim", "pgy_claim", "pgy_claim_secure", "pgy_claim_device" },
        { "Read", "pgy_read", "pgy_secure_read", "pgy_device_read" },
        { "Release", "pgy_release", "pgy_secure_release", "pgy_release_device" },
        { "Write", "pgy_write", "pgy_secure_write", "pgy_device_write" },
    };
    const SlotRuntimeFnSpec *spec;
    const char *prefix;
    int written;

    if (op_name == NULL || inner_name == NULL || out == NULL || out_size == 0)
        return false;

    spec = bsearch(op_name,
                   specs,
                   sizeof(specs) / sizeof(specs[0]),
                   sizeof(specs[0]),
                   slot_runtime_fn_spec_compare);
    if (spec == NULL)
        return false;

    prefix = is_device_slot ? spec->device_prefix
           : is_secure_slot ? spec->secure_prefix
                            : spec->plain_prefix;
    written = snprintf(out, out_size, "%s_%s", prefix, inner_name);
    return written >= 0 && (size_t)written < out_size;
}
