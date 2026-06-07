#ifndef PGY_WORKER_BOUNDARY_STORAGE_POLICY_H
#define PGY_WORKER_BOUNDARY_STORAGE_POLICY_H

#include <stdbool.h>

typedef enum PgyWorkerBoundaryStorageKind {
    PGY_WORKER_BOUNDARY_STORAGE_NONE = 0,
    PGY_WORKER_BOUNDARY_STORAGE_ARRAY,
    PGY_WORKER_BOUNDARY_STORAGE_ARRAY_SLICE,
    PGY_WORKER_BOUNDARY_STORAGE_SLICE,
    PGY_WORKER_BOUNDARY_STORAGE_LIST,
    PGY_WORKER_BOUNDARY_STORAGE_QUEUE,
    PGY_WORKER_BOUNDARY_STORAGE_SET,
    PGY_WORKER_BOUNDARY_STORAGE_HASHMAP,
    PGY_WORKER_BOUNDARY_STORAGE_CHANNEL
} PgyWorkerBoundaryStorageKind;

const char *pgy_worker_boundary_storage_kind_name(
    PgyWorkerBoundaryStorageKind kind);

PgyWorkerBoundaryStorageKind
pgy_worker_boundary_storage_kind_from_constructor_name(
    const char *constructor_name,
    bool include_channel,
    bool include_array_slice_alias);

PgyWorkerBoundaryStorageKind
pgy_worker_boundary_storage_kind_from_type_name(const char *type_name,
                                                bool include_channel);

#endif
