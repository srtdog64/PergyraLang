#include "pgy_runtime_process_exit.h"
#include "pgy_runtime_strview_inline.h"
#include "pgy_runtime_secure_open.h"
#define PGY_MAX_OPEN_FILES 256
static FILE *_pgy_ftable[PGY_MAX_OPEN_FILES];
static pthread_mutex_t _pgy_ftable_mutex = PTHREAD_MUTEX_INITIALIZER;
static inline void
_pgy_io_init_locked(void)
{
    _pgy_ftable[0] = stdin;
    _pgy_ftable[1] = stdout;
    _pgy_ftable[2] = stderr;
}
static inline PgyRuntimeIoIntResult
pgy_try_file_open_result(const char *path, const char *mode)
{
    char *resolved;
    bool for_write = false;
    int fd = -1;
    if (mode == NULL)
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_NULL_MODE, "io-boundary", "file-open"));
    for (const char *p = mode; *p != '\0'; p++) {
        if (*p == 'w' || *p == 'a' || *p == '+') {
            for_write = true;
            break;
        }
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
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (_pgy_ftable[0] == NULL)
        _pgy_io_init_locked();
    for (int i = 3; i < PGY_MAX_OPEN_FILES; i++) {
        if (_pgy_ftable[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        fclose(fp);
        return pgy_runtime_io_int_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_NO_FREE_HANDLE,
            "io-boundary", "file-open"));
    }
    _pgy_ftable[fd] = fp;
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    return pgy_runtime_io_int_ok((int32_t)fd);
}
static inline int32_t
pgy_file_open(const char *path, const char *mode)
{
    PgyRuntimeIoIntResult result = pgy_try_file_open_result(path, mode);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK ? result.ok : -1;
}
static inline PgyRuntimeIoStringResult
pgy_try_file_read_result(int32_t fd)
{
    char tmp[4096];
    tmp[0] = '\0';
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_INVALID_HANDLE,
            "io-boundary", "file-read"));
    }
    if (fgets(tmp, sizeof(tmp), _pgy_ftable[fd]) == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_EOF, "io-boundary", "file-read"));
    }
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    char *copy = pgy_runtime_strdup(tmp);
    if (copy == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "file-read"));
    return pgy_runtime_io_string_ok(copy);
}
static inline char *
pgy_file_read(int32_t fd)
{
    PgyRuntimeIoStringResult result = pgy_try_file_read_result(fd);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_strdup("");
}
static inline PgyRuntimeIoVoidResult
pgy_try_file_write_result(int32_t fd, const char *data)
{
    size_t len;
    size_t written;
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_INVALID_HANDLE,
            "io-boundary", "file-write"));
    }
    if (data == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_io_void_ok();
    }
    len = strlen(data);
    written = fwrite(data, 1, len, _pgy_ftable[fd]);
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    if (written != len)
        return pgy_runtime_io_void_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_WRITE_FAILED,
            "io-boundary", "file-write"));
    return pgy_runtime_io_void_ok();
}
static inline void
pgy_file_write(int32_t fd, const char *data)
{
    (void)pgy_try_file_write_result(fd, data);
}
static inline void
pgy_file_close(int32_t fd)
{
    FILE *fp = NULL;
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (fd >= 3 && fd < PGY_MAX_OPEN_FILES && _pgy_ftable[fd] != NULL) {
        fp = _pgy_ftable[fd];
        _pgy_ftable[fd] = NULL;
    }
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    if (fp == NULL)
        return;
    fclose(fp);
}
static inline PgyRuntimeIoStringResult
pgy_try_read_file_result(const char *path)
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
static inline char *
pgy_read_file(const char *path)
{
    PgyRuntimeIoStringResult result = pgy_try_read_file_result(path);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_strdup("");
}
static inline PgyRuntimeIoStringResult
pgy_try_read_stdin_result(int32_t max_bytes)
{
    pgy_cap_require_export(PGY_CAP_IO_READ, "read-stdin");
    if (max_bytes <= 0)
        return pgy_runtime_io_string_ok(pgy_runtime_strdup(""));
    if ((uint32_t)max_bytes > PGY_RUNTIME_MAX_FILE_BYTES)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_TOO_LARGE, "io-boundary", "read-stdin"));
    char *buf = (char *)malloc((size_t)max_bytes + 1);
    if (buf == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "read-stdin"));
#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
    size_t read_len = fread(buf, 1, (size_t)max_bytes, stdin);
    if (read_len == 0 && ferror(stdin)) {
        free(buf);
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_READ_FAILED, "io-boundary", "read-stdin"));
    }
    buf[read_len] = '\0';
    return pgy_runtime_io_string_ok(buf);
}
static inline char *
pgy_read_stdin(int32_t max_bytes)
{
    PgyRuntimeIoStringResult result = pgy_try_read_stdin_result(max_bytes);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_strdup("");
}
static inline PgyRuntimeIoIntResult
pgy_try_file_exists_result(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
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
static inline bool
pgy_file_exists(const char *path)
{
    PgyRuntimeIoIntResult result = pgy_try_file_exists_result(path);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK && result.ok != 0;
}
static inline PgyRuntimeIoVoidResult
pgy_try_write_file_result(const char *path, const char *data)
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
static inline void
pgy_write_file(const char *path, const char *data)
{
    (void)pgy_try_write_file_result(path, data);
}
static inline PgyRuntimeIoStringResult
pgy_try_input_result(const char *prompt)
{
    char tmp[4096];
    char *copy;
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
    copy = pgy_runtime_strdup(tmp);
    if (copy == NULL)
        return pgy_runtime_io_string_err(pgy_runtime_io_failure_from_status(
            PGY_RUNTIME_IO_STATUS_ALLOC_FAILED, "io-boundary", "input"));
    return pgy_runtime_io_string_ok(copy);
}
static inline char *
pgy_input(const char *prompt)
{
    PgyRuntimeIoStringResult result;
    pgy_cap_require_export(PGY_CAP_IO_READ, "input");
    result = pgy_try_input_result(prompt);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_strdup("");
}
static inline void
pgy_print(const char *msg)
{
    if (msg != NULL) printf("%s", msg);
    fflush(stdout);
}
static inline int32_t
pgy_now_ms(void)
{
    pgy_cap_require_export(PGY_CAP_CLOCK, "now-ms");
#ifdef _WIN32
    return (int32_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int32_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
#endif
}
static inline void
pgy_sleep_ms(int32_t ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
#endif
}
static inline void
pgy_exit(int32_t code)
{
    pgy_runtime_process_exit(code);
}
#include "pgy_runtime_string_builtin_inline.h"
#include "pgy_runtime_qubit_inline.h"
