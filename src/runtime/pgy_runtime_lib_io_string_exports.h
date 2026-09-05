static char *
pgy_runtime_lib_strdup(const char *src)
{
    if (src == NULL)
        src = "";
    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}
#include "pgy_runtime_io_status.h"
#include "pgy_runtime_file_mode_capability.h"
#include "pgy_runtime_process_exit.h"
#include "pgy_runtime_strview_inline.h"
#include "pgy_runtime_secure_open.h"
/* =================================================================
 * File I/O and string helpers needed by LLVM backend
 * ================================================================= */
#define PGY_MAX_OPEN_FILES 256
static FILE *pgy_runtime_ftable[PGY_MAX_OPEN_FILES];
static pthread_mutex_t pgy_runtime_ftable_mutex = PTHREAD_MUTEX_INITIALIZER;
static void
pgy_runtime_io_init_locked(void)
{
    pgy_runtime_ftable[0] = stdin;
    pgy_runtime_ftable[1] = stdout;
    pgy_runtime_ftable[2] = stderr;
}
PgyRuntimeIoIntResult pgy_try_file_open_result(const char *path,
                                               const char *mode)
{
    char *resolved;
    uint32_t capability_mask;
    bool for_write;
    int fd = -1;
    if (mode == NULL)
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_NULL_MODE, "io-boundary", "file-open"));
    capability_mask = pgy_file_mode_capability_mask(mode);
    for_write = (capability_mask & PGY_CAP_IO_WRITE) != 0u;
    if (capability_mask == PGY_CAP_IO_WRITE) {
        pgy_cap_require_export(PGY_CAP_IO_WRITE, "file-open-write");
    } else if (capability_mask == PGY_CAP_IO_READ) {
        pgy_cap_require_export(PGY_CAP_IO_READ, "file-open-read");
    } else {
        pgy_cap_require_export(capability_mask, "file-open-read-write");
    }
    resolved = pgy_runtime_resolve_file_path(path, for_write);
    if (resolved == NULL)
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED, "io-boundary", "file-open"));
    FILE *fp = pgy_runtime_secure_fopen(resolved, mode);
    free(resolved);
    if (fp == NULL)
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_OPEN_FAILED, "io-boundary", "file-open"));
    pthread_mutex_lock(&pgy_runtime_ftable_mutex);
    if (pgy_runtime_ftable[0] == NULL)
        pgy_runtime_io_init_locked();
    for (int i = 3; i < PGY_MAX_OPEN_FILES; i++) {
        if (pgy_runtime_ftable[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
        fclose(fp);
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_NO_FREE_HANDLE,
            "io-boundary", "file-open"));
    }
    pgy_runtime_ftable[fd] = fp;
    pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
    return pgy_runtime_io_int_ok((int32_t)fd);
}
int32_t pgy_file_open(const char *path, const char *mode)
{
    PgyRuntimeIoIntResult result = pgy_try_file_open_result(path, mode);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK ? result.ok : -1;
}
PgyRuntimeIoStringResult pgy_try_file_read_result(int32_t fd)
{
    char tmp[4096];
    pgy_cap_require_export(PGY_CAP_IO_READ, "file-read");
    tmp[0] = '\0';
    pthread_mutex_lock(&pgy_runtime_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL) {
        pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_INVALID_HANDLE,
            "io-boundary", "file-read"));
    }
    if (fgets(tmp, sizeof(tmp), pgy_runtime_ftable[fd]) == NULL) {
        pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_EOF, "io-boundary", "file-read"));
    }
    pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    char *copy = pgy_runtime_lib_strdup(tmp);
    if (copy == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "file-read"));
    return pgy_runtime_io_string_ok(copy);
}
char *pgy_file_read(int32_t fd)
{
    PgyRuntimeIoStringResult result = pgy_try_file_read_result(fd);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_lib_strdup("");
}
PgyRuntimeIoVoidResult pgy_try_file_write_result(int32_t fd, const char *data)
{
    size_t len;
    size_t written;
    pgy_cap_require_export(PGY_CAP_IO_WRITE, "file-write");
    pthread_mutex_lock(&pgy_runtime_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL) {
        pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_INVALID_HANDLE,
            "io-boundary", "file-write"));
    }
    if (data == NULL) {
        pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
        return pgy_runtime_io_void_ok();
    }
    len = strlen(data);
    written = fwrite(data, 1, len, pgy_runtime_ftable[fd]);
    pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
    if (written != len)
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_WRITE_FAILED,
            "io-boundary", "file-write"));
    return pgy_runtime_io_void_ok();
}
void pgy_file_write(int32_t fd, const char *data)
{
    (void)pgy_try_file_write_result(fd, data);
}
void pgy_file_close(int32_t fd)
{
    FILE *fp = NULL;
    pthread_mutex_lock(&pgy_runtime_ftable_mutex);
    if (fd >= 3 && fd < PGY_MAX_OPEN_FILES && pgy_runtime_ftable[fd] != NULL) {
        fp = pgy_runtime_ftable[fd];
        pgy_runtime_ftable[fd] = NULL;
    }
    pthread_mutex_unlock(&pgy_runtime_ftable_mutex);
    if (fp == NULL)
        return;
    fclose(fp);
}
#define PGY_COMPILER_ARTIFACT_TXN_API
#include "pgy_runtime_artifact_transaction_core.h"
PgyRuntimeIoStringResult pgy_try_read_file_result(const char *path)
{
    char *resolved;
    pgy_cap_require_export(PGY_CAP_IO_READ, "read-file");
    resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED, "io-boundary", "read-file"));
    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        free(resolved);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_OPEN_FAILED, "io-boundary", "read-file"));
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_SEEK_FAILED, "io-boundary", "read-file"));
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            len < 0 ? PGY_RUNTIME_IO_STATUS_TELL_FAILED
                    : PGY_RUNTIME_IO_STATUS_TOO_LARGE,
            "io-boundary", "read-file"));
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_SEEK_FAILED, "io-boundary", "read-file"));
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "read-file"));
    }
    size_t read_len = fread(buf, 1, (size_t)len, fp);
    if (read_len != (size_t)len) {
        fclose(fp);
        free(resolved);
        free(buf);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_READ_FAILED, "io-boundary", "read-file"));
    }
    buf[read_len] = '\0';
    fclose(fp);
    free(resolved);
    return pgy_runtime_io_string_ok(buf);
}
char *pgy_read_file(const char *path)
{
    PgyRuntimeIoStringResult result = pgy_try_read_file_result(path);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_lib_strdup("");
}
PgyRuntimeIoStringResult pgy_try_read_stdin_result(int32_t max_bytes)
{
    pgy_cap_require_export(PGY_CAP_IO_READ, "read-stdin");
    if (max_bytes <= 0)
        return pgy_runtime_io_string_ok(pgy_runtime_lib_strdup(""));
    if ((uint32_t)max_bytes > PGY_RUNTIME_MAX_FILE_BYTES)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_TOO_LARGE, "io-boundary", "read-stdin"));
    char *buf = (char *)malloc((size_t)max_bytes + 1);
    if (buf == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "read-stdin"));
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        free(buf);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_READ_FAILED, "io-boundary", "read-stdin"));
    }
#endif
#ifdef _WIN32
    int read_len;
    do {
        read_len = _read(_fileno(stdin), buf, (unsigned int)max_bytes);
    } while (read_len < 0 && errno == EINTR);
#else
    ssize_t read_len;
    do {
        read_len = read(STDIN_FILENO, buf, (size_t)max_bytes);
    } while (read_len < 0 && errno == EINTR);
#endif
    if (read_len < 0) {
        free(buf);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_READ_FAILED, "io-boundary", "read-stdin"));
    }
    buf[(size_t)read_len] = '\0';
    return pgy_runtime_io_string_ok(buf);
}
char *pgy_read_stdin(int32_t max_bytes)
{
    PgyRuntimeIoStringResult result = pgy_try_read_stdin_result(max_bytes);
    if (result.tag != PGY_RUNTIME_IO_RESULT_OK)
        abort();
    return result.ok;
}
void pgy_print(const char *msg)
{
    pgy_cap_require_export(PGY_CAP_IO_WRITE, "print");
    if (msg == NULL)
        msg = "";
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        abort();
#endif
    if (fputs(msg, stdout) == EOF)
        abort();
    if (fflush(stdout) == EOF)
        abort();
}
PgyRuntimeIoIntResult pgy_try_file_exists_result(const char *path)
{
    char *resolved;
    pgy_cap_require_export(PGY_CAP_IO_READ, "file-exists");
    resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED,
            "io-boundary", "file-exists"));
    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        free(resolved);
        return pgy_runtime_io_int_ok(0);
    }
    fclose(fp);
    free(resolved);
    return pgy_runtime_io_int_ok(1);
}
bool pgy_file_exists(const char *path)
{
    PgyRuntimeIoIntResult result = pgy_try_file_exists_result(path);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK && result.ok != 0;
}
PgyRuntimeIoVoidResult pgy_try_write_file_result(const char *path,
                                                 const char *data)
{
    char *resolved;
    pgy_cap_require_export(PGY_CAP_IO_WRITE, "write-file");
    resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_RESOLVE_FAILED, "io-boundary", "write-file"));
    FILE *fp = pgy_runtime_secure_fopen(resolved, "wb");
    if (fp == NULL) {
        free(resolved);
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_OPEN_FAILED, "io-boundary", "write-file"));
    }
    if (data != NULL) {
        size_t len = strlen(data);
        size_t written = fwrite(data, 1, len, fp);
        if (written != len) {
            fclose(fp);
            free(resolved);
            return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
                PGY_RUNTIME_IO_STATUS_WRITE_FAILED,
                "io-boundary", "write-file"));
        }
    }
    fclose(fp);
    free(resolved);
    return pgy_runtime_io_void_ok();
}
void pgy_write_file(const char *path, const char *data)
{
    (void)pgy_try_write_file_result(path, data);
}
PgyRuntimeIoStringResult pgy_try_input_result(const char *prompt)
{
    char tmp[4096];
    char *result;
    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);
    tmp[0] = '\0';
    if (fgets(tmp, sizeof(tmp), stdin) == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_EOF, "io-boundary", "input"));
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    len = strlen(tmp);
    result = (char *)malloc(len + 1);
    if (result == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "input"));
    memcpy(result, tmp, len + 1);
    return pgy_runtime_io_string_ok(result);
}
char *pgy_input(const char *prompt)
{
    PgyRuntimeIoStringResult result;
    pgy_cap_require_export(PGY_CAP_IO_READ, "input");
    result = pgy_try_input_result(prompt);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_lib_strdup("");
}
void pgy_exit(int32_t code)
{
    pgy_runtime_process_exit(code);
}
/* String exports live in their own owner; included after the io ABI owner
 * so the shared strdup helper above stays visible to both. */
#include "pgy_runtime_lib_string_exports.h"
