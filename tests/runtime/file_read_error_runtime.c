#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PGY_TEST_EXPORT_RUNTIME
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
#else
#include "runtime/pgy_runtime.h"
#endif

static int expect_failure(int32_t fd, PgyRuntimeIoStatus status)
{
    PgyRuntimeIoStringResult result = pgy_try_file_read_result(fd);
    if (result.tag != PGY_RUNTIME_IO_RESULT_ERR || result.err.status != status
        || strcmp(result.err.name, pgy_runtime_io_status_name(status)) != 0
        || strcmp(result.err.stage, "io-boundary") != 0
        || strcmp(result.err.operation, "file-read") != 0
        || !result.err.recoverable) {
        fprintf(stderr, "file-read did not return %s\n",
                pgy_runtime_io_status_name(status));
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    PgyRuntimeIoVoidResult written = pgy_try_write_file_result(argv[1], "first\nlast");
    if (written.tag != PGY_RUNTIME_IO_RESULT_OK) return 2;
    PgyRuntimeIoIntResult opened = pgy_try_file_open_result(argv[1], "rb");
    if (opened.tag != PGY_RUNTIME_IO_RESULT_OK) return 3;
    int32_t fd = opened.ok;
    const char *lines[] = {"first", "last"};
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        PgyRuntimeIoStringResult line = pgy_try_file_read_result(fd);
        if (line.tag != PGY_RUNTIME_IO_RESULT_OK || strcmp(line.ok, lines[i]) != 0)
            return 4;
        free(line.ok);
    }
    if (expect_failure(fd, PGY_RUNTIME_IO_STATUS_EOF)
        || expect_failure(fd, PGY_RUNTIME_IO_STATUS_EOF)) return 5;
    pgy_file_close(fd);
    if (expect_failure(fd, PGY_RUNTIME_IO_STATUS_INVALID_HANDLE)) return 6;

    /* A real write-only stream sets its error indicator when fgets reads it. */
    opened = pgy_try_file_open_result(argv[1], "wb");
    if (opened.tag != PGY_RUNTIME_IO_RESULT_OK) return 7;
    fd = opened.ok;
    if (expect_failure(fd, PGY_RUNTIME_IO_STATUS_READ_FAILED)
        || expect_failure(fd, PGY_RUNTIME_IO_STATUS_READ_FAILED)) return 8;
    char *legacy = pgy_file_read(fd);
    if (legacy == NULL || strcmp(legacy, "") != 0) return 9;
    free(legacy);
    pgy_file_close(fd);

    opened = pgy_try_file_open_result(argv[1], "rb");
    if (opened.tag != PGY_RUNTIME_IO_RESULT_OK) return 10;
    fd = opened.ok;
    if (expect_failure(fd, PGY_RUNTIME_IO_STATUS_EOF)) return 11;
    pgy_file_close(fd);
    puts("file read error runtime: ok");
    return 0;
}
