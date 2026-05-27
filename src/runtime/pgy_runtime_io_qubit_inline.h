/* =================================================================
 * I/O Built-ins (platform-independent via C stdio)
 *
 * File handles use an internal table mapping Int fd → FILE*.
 * fd 0/1/2 are reserved for stdin/stdout/stderr.
 * ================================================================= */

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

/* FileOpen(path, mode) → fd (-1 on error) */
static inline int32_t
pgy_file_open(const char *path, const char *mode)
{
    char *resolved;
    bool for_write = false;
    int fd = -1;

    if (mode == NULL)
        return -1;
    for (const char *p = mode; *p != '\0'; p++) {
        if (*p == 'w' || *p == 'a' || *p == '+') {
            for_write = true;
            break;
        }
    }

    resolved = pgy_runtime_resolve_file_path(path, for_write);
    if (resolved == NULL)
        return -1;

    FILE *fp = fopen(resolved, mode);
    free(resolved);
    if (fp == NULL) return -1;
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
        return -1;
    }
    _pgy_ftable[fd] = fp;
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    return (int32_t)fd;
}

/* FileRead(fd) → read one line (heap-allocated copy) */
static inline char *
pgy_file_read(int32_t fd)
{
    char tmp[4096];
    tmp[0] = '\0';
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_strdup("");
    }
    if (fgets(tmp, sizeof(tmp), _pgy_ftable[fd]) == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return pgy_runtime_strdup("");
    }
    pthread_mutex_unlock(&_pgy_ftable_mutex);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_strdup(tmp);
}

/* FileWrite(fd, data) */
static inline void
pgy_file_write(int32_t fd, const char *data)
{
    pthread_mutex_lock(&_pgy_ftable_mutex);
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) {
        pthread_mutex_unlock(&_pgy_ftable_mutex);
        return;
    }
    if (data != NULL)
        fwrite(data, 1, strlen(data), _pgy_ftable[fd]);
    pthread_mutex_unlock(&_pgy_ftable_mutex);
}

/* FileClose(fd) */
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

/* ReadFile(path) → entire file as heap-allocated string */
static inline char *
pgy_read_file(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_strdup("");
    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        free(resolved);
        return pgy_runtime_strdup("");
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_strdup("");
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_strdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_strdup("");
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        free(resolved);
        return pgy_runtime_strdup("");
    }
    size_t read_len = fread(buf, 1, (size_t)len, fp);
    if (read_len != (size_t)len) {
        fclose(fp);
        free(resolved);
        free(buf);
        return pgy_runtime_strdup("");
    }
    buf[read_len] = '\0';
    fclose(fp);
    free(resolved);
    return buf;
}

static inline bool
pgy_file_exists(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return false;

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) {
        free(resolved);
        return false;
    }

    fclose(fp);
    free(resolved);
    return true;
}

/* WriteFile(path, data) → write entire string to file */
static inline void
pgy_write_file(const char *path, const char *data)
{
    char *resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return;
    FILE *fp = fopen(resolved, "wb");
    if (fp == NULL) {
        free(resolved);
        return;
    }
    if (data != NULL) {
        size_t len = strlen(data);
        (void)fwrite(data, 1, len, fp);
    }
    fclose(fp);
    free(resolved);
}

/* Input(prompt) → read line from stdin */
static inline char *
pgy_input(const char *prompt)
{
    char tmp[4096];
    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);
    tmp[0] = '\0';
    if (fgets(tmp, sizeof(tmp), stdin) == NULL)
        return pgy_runtime_strdup("");
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_strdup(tmp);
}

/* Print(msg) → stdout without newline */
static inline void
pgy_print(const char *msg)
{
    if (msg != NULL) printf("%s", msg);
    fflush(stdout);
}

static inline int32_t
pgy_now_ms(void)
{
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
    exit((int)code);
}

/* =================================================================
 * String Built-ins
 * ================================================================= */

/* StringContains(haystack, needle) → Bool */
static inline bool
StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL) return false;
    return strstr(haystack, needle) != NULL;
}

static inline int32_t
StringIndexOf(const char *haystack, const char *needle)
{
    const char *match;

    if (haystack == NULL || needle == NULL)
        return -1;
    match = strstr(haystack, needle);
    if (match == NULL)
        return -1;
    return (int32_t)(match - haystack);
}

/* StringLength is already defined as pgy_string_length or similar */

/* Substring(s, start, len) → new string */
static inline char *
Substring(const char *s, int32_t start, int32_t len)
{
    size_t raw_len;
    int32_t slen;

    if (s == NULL) return pgy_runtime_strdup("");
    raw_len = strlen(s);
    if (raw_len > (size_t)INT32_MAX)
        return pgy_runtime_strdup("");
    slen = (int32_t)raw_len;
    if (start < 0 || start >= slen || len <= 0) return pgy_runtime_strdup("");
    if (len > slen - start) len = slen - start;
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}

/* StringReplace(s, old, new) → new string with all occurrences replaced */
static inline char *
StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL) return pgy_runtime_strdup(s ? s : "");
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0) return pgy_runtime_strdup(s);

    /* Count occurrences */
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) { count++; p += old_len; }

    size_t source_len = strlen(s);
    size_t result_len;
    if (new_len > old_len) {
        size_t delta = new_len - old_len;
        if (count > (((size_t)-1) - source_len) / delta)
            return pgy_runtime_strdup("");
        result_len = source_len + count * delta;
    } else if (new_len == old_len) {
        result_len = source_len;
    } else {
        size_t delta = old_len - new_len;
        result_len = source_len - count * delta;
    }
    char *result = (char *)malloc(result_len + 1);
    if (result == NULL) return pgy_runtime_strdup("");
    char *dst = result;
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

/* StringTrim(s) → new string with leading/trailing whitespace removed */
static inline char *
StringTrim(const char *s)
{
    if (s == NULL) return pgy_runtime_strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r'))
        len--;
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ToUpper(s) → new uppercase string */
static inline char *
ToUpper(const char *s)
{
    if (s == NULL) return pgy_runtime_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    return buf;
}

/* ToLower(s) → new lowercase string */
static inline char *
ToLower(const char *s)
{
    if (s == NULL) return pgy_runtime_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    return buf;
}

/* StringConcat(a, b) → new concatenated string */
static inline char *
StringConcat(const char *a, const char *b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    size_t la = strlen(a), lb = strlen(b);
    if (la > ((size_t)-1) - lb || la + lb > ((size_t)-1) - 1)
        return pgy_runtime_strdup("");
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}

/* =================================================================
 * StringSplit / StringJoin — string manipulation
 *
 * StringSplit(s, delim) → Array<String>
 * StringJoin(arr, sep)  → String
 *
 * Note: These must be defined AFTER PGY_ARRAY_DEFINE(String, char*)
 * which generates PgyArray_String.
 * ================================================================= */

static inline PgyArray_String
StringSplit(const char *s, const char *delim)
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || delim[0] == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_strdup(s));
        return result;
    }

    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) {
            memcpy(part, p, seg);
            part[seg] = '\0';
        }
        pgy_array_push_String(&result,
            part != NULL ? part : pgy_runtime_strdup(""));
        p = found + dlen;
    }
    return result;
}

static inline char *
StringJoin(PgyArray_String *arr, const char *sep)
{
    if (arr == NULL || arr->data == NULL || arr->length == 0) {
        return pgy_runtime_strdup("");
    }
    if (sep == NULL) sep = "";
    size_t sep_len = strlen(sep);
    /* Calculate total length */
    size_t total = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i]) {
            size_t sl = strlen(arr->data[i]);
            if (sl > ((size_t)-1) - total)
                return pgy_runtime_strdup("");
            total += sl;
        }
        if (i < arr->length - 1) {
            if (sep_len > ((size_t)-1) - total)
                return pgy_runtime_strdup("");
            total += sep_len;
        }
    }
    if (total == (size_t)-1)
        return pgy_runtime_strdup("");
    char *result = (char *)malloc(total + 1);
    if (result == NULL)
        return pgy_runtime_strdup("");
    size_t pos = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i]) {
            size_t sl = strlen(arr->data[i]);
            memcpy(result + pos, arr->data[i], sl);
            pos += sl;
        }
        if (i < arr->length - 1) {
            memcpy(result + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    result[pos] = '\0';
    return result;
}

static inline bool
pgy_string_equals(const char *a, const char *b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    return strcmp(a, b) == 0;
}

#include "pgy_runtime_qubit_inline.h"
