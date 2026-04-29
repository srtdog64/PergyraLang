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

/* =================================================================
 * File I/O and string helpers needed by LLVM backend
 * ================================================================= */

#define PGY_MAX_OPEN_FILES 256

static FILE *pgy_runtime_ftable[PGY_MAX_OPEN_FILES];
static int   pgy_runtime_ftable_next = 3;

static void
pgy_runtime_io_init(void)
{
    pgy_runtime_ftable[0] = stdin;
    pgy_runtime_ftable[1] = stdout;
    pgy_runtime_ftable[2] = stderr;
}

int32_t pgy_file_open(const char *path, const char *mode)
{
    if (pgy_runtime_ftable[0] == NULL)
        pgy_runtime_io_init();

    FILE *fp = fopen(path, mode);
    if (fp == NULL)
        return -1;

    int fd = -1;
    for (int i = 3; i < PGY_MAX_OPEN_FILES; i++) {
        if (pgy_runtime_ftable[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        fclose(fp);
        return -1;
    }

    if (fd >= pgy_runtime_ftable_next)
        pgy_runtime_ftable_next = fd + 1;
    pgy_runtime_ftable[fd] = fp;
    return (int32_t)fd;
}

char *pgy_file_read(int32_t fd)
{
    char tmp[4096];

    tmp[0] = '\0';
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return pgy_runtime_lib_strdup("");
    if (fgets(tmp, sizeof(tmp), pgy_runtime_ftable[fd]) == NULL)
        return pgy_runtime_lib_strdup("");

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_lib_strdup(tmp);
}

void pgy_file_write(int32_t fd, const char *data)
{
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), pgy_runtime_ftable[fd]);
}

void pgy_file_close(int32_t fd)
{
    if (fd < 3 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    fclose(pgy_runtime_ftable[fd]);
    pgy_runtime_ftable[fd] = NULL;
}

char *pgy_read_file(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_lib_strdup("");

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL)
        return pgy_runtime_lib_strdup("");

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    size_t read_len = fread(buf, 1, (size_t)len, fp);
    if (read_len != (size_t)len) {
        fclose(fp);
        free(resolved);
        free(buf);
        return pgy_runtime_lib_strdup("");
    }
    buf[read_len] = '\0';
    fclose(fp);
    free(resolved);
    return buf;
}

void pgy_write_file(const char *path, const char *data)
{
    char *resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return;
    FILE *fp = fopen(resolved, "wb");
    if (fp == NULL)
        return;
    if (data != NULL) {
        size_t len = strlen(data);
        (void)fwrite(data, 1, len, fp);
    }
    fclose(fp);
    free(resolved);
}

char *pgy_input(const char *prompt)
{
    char tmp[4096];

    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);

    tmp[0] = '\0';
    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
        char *empty = (char *)malloc(1);
        if (empty != NULL) empty[0] = '\0';
        return empty;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';

    len = strlen(tmp);
    char *result = (char *)malloc(len + 1);
    if (result != NULL)
        memcpy(result, tmp, len + 1);
    return result;
}

bool StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}

char *Substring(const char *s, int32_t start, int32_t len)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0)
        return pgy_runtime_lib_strdup("");
    if (start + len > slen)
        len = slen - start;

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}

char *StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_lib_strdup(s != NULL ? s : "");

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0)
        return pgy_runtime_lib_strdup(s);

    int count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    size_t result_len = strlen(s) + (size_t)count * (new_len - old_len);
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

/* StringSplit(str, delim) ??Array<String> (caller-allocated PgyArray_String) */
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
