#ifndef PGY_SELF_HOST_MIR_ARTIFACT_OWNER_H
#define PGY_SELF_HOST_MIR_ARTIFACT_OWNER_H

#include <stdbool.h>

int driver_materialize_self_host_mir_artifact(const char *launcher_path,
                                              const char *source_path,
                                              const char *output_path,
                                              bool verbose);

#endif /* PGY_SELF_HOST_MIR_ARTIFACT_OWNER_H */
