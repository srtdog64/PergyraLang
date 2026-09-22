#ifndef PGY_DRIVER_BINARY_OUTPUT_OWNER_H
#define PGY_DRIVER_BINARY_OUTPUT_OWNER_H

#include "driver_app.h"

/* Resolve the binary actually published by a plain C/LLVM compile request. */
char *driver_binary_output_resolve(const DriverFlags *flags);

/* Invalidate a previous binary before any source admission or backend work. */
bool driver_binary_output_prepare(const DriverFlags *flags);

/* Prepare only when the request publishes a plain C or LLVM binary; true
 * when there is nothing to invalidate. */
bool driver_binary_output_prepare_for_target(const DriverFlags *flags);

/* A binary is built at a private staging path next to its destination and
 * published by one rename only after the toolchain succeeded; on any failure
 * no new binary appears (the old one was already dropped by prepare). */
typedef struct DriverBinaryPublication {
    char *staging_path; /* where the toolchain writes */
    char *target_path;  /* what a success publishes */
} DriverBinaryPublication;

bool driver_binary_output_publication_begin(const char *requested_path,
                                            DriverBinaryPublication *out);
/* Rename staging over the target. On failure the staging file is removed,
 * a stage diagnostic is emitted and false is returned. */
bool driver_binary_output_publication_commit(const DriverFlags *flags,
                                             DriverBinaryPublication *publication);
/* Remove whatever the toolchain left at the staging path. */
void driver_binary_output_publication_abort(DriverBinaryPublication *publication);
void driver_binary_output_publication_free(DriverBinaryPublication *publication);

/* A command line that failed to parse still drops the binary its source
 * and target name; returns the exit status 1. */
int driver_binary_output_refuse_invalid_args(const DriverFlags *flags);

#endif
