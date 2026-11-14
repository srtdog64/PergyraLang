#ifndef PGY_SELF_HOST_ARTIFACT_PROCESS_OWNER_H
#define PGY_SELF_HOST_ARTIFACT_PROCESS_OWNER_H

#include <stdbool.h>

int driver_run_self_host_artifact_process(
    const char *const argv[], bool verbose, bool emit_json_diagnostic);

#endif /* PGY_SELF_HOST_ARTIFACT_PROCESS_OWNER_H */
