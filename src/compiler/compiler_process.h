#ifndef PGY_COMPILER_PROCESS_H
#define PGY_COMPILER_PROCESS_H

#include <stdbool.h>
#include <stddef.h>

int pgy_exec_probe_argv_silent(const char *const argv[]);
int pgy_exec_argv(const char *const argv[], bool verbose);
enum {
    PGY_EXEC_CAPTURE_ERROR = -1,
    PGY_EXEC_CAPTURE_TIMEOUT = -2,
    PGY_EXEC_CAPTURE_OUTPUT_LIMIT = -3,
    PGY_EXEC_CAPTURE_CRASHED = -4
};
int pgy_exec_argv_capture_stdout(const char *const argv[],
                                 size_t max_output_bytes,
                                 unsigned int timeout_millis,
                                 unsigned char **output,
                                 size_t *output_length);

#ifdef _WIN32
void pgy_win32_normalize_exec_path(const char *path, char *dst, size_t dst_cap);
#endif

#endif /* PGY_COMPILER_PROCESS_H */
