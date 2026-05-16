#ifndef PGY_COMPILER_PROCESS_H
#define PGY_COMPILER_PROCESS_H

#include <stdbool.h>

int pgy_exec_probe_argv_silent(const char *const argv[]);
int pgy_exec_argv(const char *const argv[], bool verbose);

#ifdef _WIN32
void pgy_win32_normalize_exec_path(const char *path, char *dst, size_t dst_cap);
#endif

#endif /* PGY_COMPILER_PROCESS_H */
