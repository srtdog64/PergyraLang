#ifndef PGY_SRC_COMPILER_FMT_IO_H
#define PGY_SRC_COMPILER_FMT_IO_H

#include <stdbool.h>

char *fmt_read_file(const char *path);
bool fmt_source_is_parseable(const char *source);

#endif /* PGY_SRC_COMPILER_FMT_IO_H */
