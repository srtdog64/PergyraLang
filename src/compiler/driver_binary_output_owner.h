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

/* A command line that failed to parse still drops the binary its source
 * and target name; returns the exit status 1. */
int driver_binary_output_refuse_invalid_args(const DriverFlags *flags);

#endif
