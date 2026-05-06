#include "transpiler_mir_resource_name_helpers.h"

#include <stdio.h>
#include <string.h>

typedef struct SlotRuntimeFnSpec {
    const char *op_name;
    const char *plain_prefix;
    const char *secure_prefix;
    const char *device_prefix;
} SlotRuntimeFnSpec;

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
        {"Claim", "pgy_claim", "pgy_claim_secure", "pgy_claim_device"},
        {"Read", "pgy_read", "pgy_secure_read", "pgy_device_read"},
        {"Write", "pgy_write", "pgy_secure_write", "pgy_device_write"},
        {"Release", "pgy_release", "pgy_secure_release", "pgy_release_device"},
    };

    if (op_name == NULL || inner_name == NULL || out == NULL || out_size == 0)
        return false;

    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        if (strcmp(op_name, specs[i].op_name) != 0)
            continue;

        const char *prefix = is_device_slot ? specs[i].device_prefix
                           : is_secure_slot ? specs[i].secure_prefix
                                            : specs[i].plain_prefix;
        snprintf(out, out_size, "%s_%s", prefix, inner_name);
        return true;
    }

    return false;
}
