#include "worker_boundary_storage_policy.h"

#include <string.h>

typedef struct PgyWorkerBoundaryStorageSpec {
    const char *constructor_name;
    const char *type_prefix;
    PgyWorkerBoundaryStorageKind kind;
    bool requires_channel;
    bool requires_array_slice_alias;
} PgyWorkerBoundaryStorageSpec;

static const PgyWorkerBoundaryStorageSpec
pgy_worker_boundary_storage_specs[] = {
    {"Array", "Array<", PGY_WORKER_BOUNDARY_STORAGE_ARRAY, false, false},
    {"Array/Slice", NULL, PGY_WORKER_BOUNDARY_STORAGE_ARRAY_SLICE, false, true},
    {"Channel", "Channel<", PGY_WORKER_BOUNDARY_STORAGE_CHANNEL, true, false},
    {"HashMap", "HashMap<", PGY_WORKER_BOUNDARY_STORAGE_HASHMAP, false, false},
    {"List", "List<", PGY_WORKER_BOUNDARY_STORAGE_LIST, false, false},
    {"Queue", "Queue<", PGY_WORKER_BOUNDARY_STORAGE_QUEUE, false, false},
    {"Set", "Set<", PGY_WORKER_BOUNDARY_STORAGE_SET, false, false},
    {"Slice", "Slice<", PGY_WORKER_BOUNDARY_STORAGE_SLICE, false, false},
};

static bool
pgy_worker_boundary_type_name_matches(
    const char *type_name,
    const PgyWorkerBoundaryStorageSpec *spec)
{
    size_t prefix_len;

    if (type_name == NULL || spec == NULL || spec->constructor_name == NULL)
        return false;
    if (strcmp(type_name, spec->constructor_name) == 0)
        return true;
    if (spec->type_prefix == NULL)
        return false;
    prefix_len = strlen(spec->type_prefix);
    return strncmp(type_name, spec->type_prefix, prefix_len) == 0;
}

static bool
pgy_worker_boundary_storage_spec_enabled(
    const PgyWorkerBoundaryStorageSpec *spec,
    bool include_channel,
    bool include_array_slice_alias)
{
    if (spec == NULL)
        return false;
    if (spec->requires_channel && !include_channel)
        return false;
    if (spec->requires_array_slice_alias && !include_array_slice_alias)
        return false;
    return true;
}

const char *
pgy_worker_boundary_storage_kind_name(PgyWorkerBoundaryStorageKind kind)
{
    switch (kind) {
    case PGY_WORKER_BOUNDARY_STORAGE_ARRAY:
        return "Array";
    case PGY_WORKER_BOUNDARY_STORAGE_ARRAY_SLICE:
        return "Array/Slice";
    case PGY_WORKER_BOUNDARY_STORAGE_SLICE:
        return "Slice";
    case PGY_WORKER_BOUNDARY_STORAGE_LIST:
        return "List";
    case PGY_WORKER_BOUNDARY_STORAGE_QUEUE:
        return "Queue";
    case PGY_WORKER_BOUNDARY_STORAGE_SET:
        return "Set";
    case PGY_WORKER_BOUNDARY_STORAGE_HASHMAP:
        return "HashMap";
    case PGY_WORKER_BOUNDARY_STORAGE_CHANNEL:
        return "Channel";
    case PGY_WORKER_BOUNDARY_STORAGE_NONE:
    default:
        return NULL;
    }
}

PgyWorkerBoundaryStorageKind
pgy_worker_boundary_storage_kind_from_constructor_name(
    const char *constructor_name,
    bool include_channel,
    bool include_array_slice_alias)
{
    size_t spec_count = sizeof(pgy_worker_boundary_storage_specs)
        / sizeof(pgy_worker_boundary_storage_specs[0]);

    if (constructor_name == NULL)
        return PGY_WORKER_BOUNDARY_STORAGE_NONE;

    for (size_t i = 0; i < spec_count; i++) {
        const PgyWorkerBoundaryStorageSpec *spec =
            &pgy_worker_boundary_storage_specs[i];

        if (strcmp(constructor_name, spec->constructor_name) != 0)
            continue;
        if (!pgy_worker_boundary_storage_spec_enabled(
                spec, include_channel, include_array_slice_alias)) {
            return PGY_WORKER_BOUNDARY_STORAGE_NONE;
        }
        return spec->kind;
    }
    return PGY_WORKER_BOUNDARY_STORAGE_NONE;
}

PgyWorkerBoundaryStorageKind
pgy_worker_boundary_storage_kind_from_type_name(const char *type_name,
                                                bool include_channel)
{
    size_t spec_count = sizeof(pgy_worker_boundary_storage_specs)
        / sizeof(pgy_worker_boundary_storage_specs[0]);

    for (size_t i = 0; i < spec_count; i++) {
        const PgyWorkerBoundaryStorageSpec *spec =
            &pgy_worker_boundary_storage_specs[i];

        if (!pgy_worker_boundary_storage_spec_enabled(
                spec, include_channel, false)) {
            continue;
        }
        if (pgy_worker_boundary_type_name_matches(type_name, spec)) {
            return spec->kind;
        }
    }
    return PGY_WORKER_BOUNDARY_STORAGE_NONE;
}
