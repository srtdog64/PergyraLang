#ifndef PGY_SELF_HOST_DRIVER_H
#define PGY_SELF_HOST_DRIVER_H

#include <stdbool.h>

char *driver_resolve_self_host_binary(const char *launcher_path);
int driver_run_self_host_command(const char *launcher_path,
                                 int argc,
                                 char *argv[]);
int driver_run_self_host_mir_json(const char *launcher_path,
                                  const char *source_path);
int driver_materialize_self_host_c_artifact(const char *launcher_path,
                                            const char *source_path,
                                            const char *output_path,
                                            bool verbose);
int driver_run_self_host_c_emit_artifact(const char *launcher_path,
                                         const char *source_path,
                                         const char *output_path,
                                         bool verbose);
#endif /* PGY_SELF_HOST_DRIVER_H */
