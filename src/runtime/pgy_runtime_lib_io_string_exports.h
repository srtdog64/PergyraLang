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
#include "pgy_runtime_process_exit.h"
#include "pgy_runtime_strview_inline.h"
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
    FILE *fp = fopen(resolved, mode);
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
char *pgy_read_stdin(int32_t max_bytes)
{
    PgyRuntimeIoStringResult result = pgy_try_read_stdin_result(max_bytes);
    return result.tag == PGY_RUNTIME_IO_RESULT_OK
        ? result.ok
        : pgy_runtime_lib_strdup("");
}
PgyRuntimeIoIntResult pgy_try_file_exists_result(const char *path)
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
    FILE *fp = fopen(resolved, "wb");
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
bool StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}
int32_t StringIndexOf(const char *haystack, const char *needle)
{
    const char *match;
    if (haystack == NULL || needle == NULL)
        return -1;
    match = strstr(haystack, needle);
    if (match == NULL)
        return -1;
    return (int32_t)(match - haystack);
}
char *Substring(const char *s, int32_t start, int32_t len)
{
    size_t raw_len;
    int32_t slen;
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    raw_len = strlen(s);
    if (raw_len > (size_t)INT32_MAX)
        return pgy_runtime_lib_strdup("");
    slen = (int32_t)raw_len;
    if (start < 0 || start >= slen || len <= 0)
        return pgy_runtime_lib_strdup("");
    if (len > slen - start)
        len = slen - start;
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}
/* Allocation-free StringIndexOf(Substring(s, start, len), needle): index of
 * `needle` within s[start .. start+len) relative to `start`, or -1. */
int32_t SubIndexOf(const char *s, int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle);
}
int32_t SubIndexOfWithLen(const char *s, int32_t source_len,
                          int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle);
}
/* Allocation-free Substring(s, start, len) == other. */
bool SubEquals(const char *s, int32_t start, int32_t len, const char *other)
{
    return pgy_strview_equals(pgy_strview(s, start, len), other);
}
bool SubEqualsWithLen(const char *s, int32_t source_len,
                      int32_t start, int32_t len, const char *other)
{
    return pgy_strview_equals(
        pgy_strview_with_len(s, source_len, start, len), other);
}
/* Allocation-free: `needle` occurs within s[start .. start+len). */
bool SubContains(const char *s, int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle) >= 0;
}
bool SubContainsWithLen(const char *s, int32_t source_len,
                        int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle) >= 0;
}
/* Allocation-free: the suffix s[start..] begins with `prefix`. */
bool SubStartsWith(const char *s, int32_t start, const char *prefix)
{
    return pgy_strview_starts_with(s, start, prefix);
}
bool SubStartsWithLen(const char *s, int32_t source_len,
                      int32_t start, const char *prefix)
{
    size_t prefix_len;
    if (s == NULL || prefix == NULL || source_len < 0)
        return false;
    if (start < 0 || start > source_len)
        return false;
    prefix_len = strlen(prefix);
    if (prefix_len == 0)
        return true;
    if (prefix_len > (size_t)INT32_MAX)
        return false;
    if ((size_t)(source_len - start) < prefix_len)
        return false;
    return pgy_strview_equals(
        pgy_strview_with_len(s, source_len, start, (int32_t)prefix_len),
        prefix);
}
/* O(1) 1-char access with a caller-supplied length (no strlen). Out-of-range
 * yields "". See the inline header for the rationale. */
char *CharAtN(const char *s, int32_t len, int32_t i)
{
    char *r;
    if (s == NULL || i < 0 || i >= len)
        return pgy_runtime_lib_strdup("");
    r = (char *)malloc(2);
    if (r == NULL)
        return pgy_runtime_lib_strdup("");
    r[0] = s[i];
    r[1] = '\0';
    return r;
}
/* O(1) byte-at-index as an int (0..255), -1 out of range. No allocation. */
int32_t CharCode(const char *s, int32_t len, int32_t i)
{
    if (s == NULL || i < 0 || i >= len)
        return -1;
    return (int32_t)(unsigned char)s[i];
}
char *StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_lib_strdup(s != NULL ? s : "");
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0)
        return pgy_runtime_lib_strdup(s);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }
    size_t source_len = strlen(s);
    size_t result_len;
    if (new_len > old_len) {
        size_t delta = new_len - old_len;
        if (count > (((size_t)-1) - source_len) / delta)
            return pgy_runtime_lib_strdup("");
        result_len = source_len + count * delta;
    } else if (new_len == old_len) {
        result_len = source_len;
    } else {
        size_t delta = old_len - new_len;
        result_len = source_len - count * delta;
    }
    char *result = (char *)malloc(result_len + 1);
    char *dst = result;
    if (result == NULL)
        return pgy_runtime_lib_strdup("");
    p = s;
    while (*p) {
        if (strncmp(p, old_str, old_len) == 0) {
            memcpy(dst, new_str, new_len);
            dst += new_len;
            p += old_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}
char *StringTrim(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len > 0
           && (s[len - 1] == ' ' || s[len - 1] == '\t'
               || s[len - 1] == '\n' || s[len - 1] == '\r'))
        len--;
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}
char *ToUpper(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    return buf;
}
char *ToLower(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    return buf;
}
char *StringConcat(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la > ((size_t)-1) - lb || la + lb > ((size_t)-1) - 1)
        return pgy_runtime_lib_strdup("");
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}
bool pgy_string_equals(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    return strcmp(a, b) == 0;
}
/* -----------------------------------------------------------------
 * StringSplit / StringJoin / ToInt / ToFloat / Math
 * ----------------------------------------------------------------- */
/* StringSplit(str, delim) -> Array<String> (caller-allocated PgyArray_String) */
PgyArray_String StringSplit(const char *s, const char *delim)
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || *delim == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(s));
        return result;
    }
    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) { memcpy(part, p, seg); part[seg] = '\0'; }
        pgy_array_push_String(&result, part != NULL ? part : pgy_runtime_lib_strdup(""));
        p = found + dlen;
    }
    return result;
}
