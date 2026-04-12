/* =================================================================
 * I/O Built-ins (platform-independent via C stdio)
 *
 * File handles use an internal table mapping Int fd → FILE*.
 * fd 0/1/2 are reserved for stdin/stdout/stderr.
 * ================================================================= */

#define PGY_MAX_OPEN_FILES 256

static FILE *_pgy_ftable[PGY_MAX_OPEN_FILES];
static int   _pgy_ftable_next = 3; /* 0=stdin,1=stdout,2=stderr */

static inline void
_pgy_io_init(void)
{
    _pgy_ftable[0] = stdin;
    _pgy_ftable[1] = stdout;
    _pgy_ftable[2] = stderr;
}

/* FileOpen(path, mode) → fd (-1 on error) */
static inline int32_t
pgy_file_open(const char *path, const char *mode)
{
    if (_pgy_ftable[0] == NULL) _pgy_io_init();
    FILE *fp = fopen(path, mode);
    if (fp == NULL) return -1;
    if (_pgy_ftable_next >= PGY_MAX_OPEN_FILES) { fclose(fp); return -1; }
    int fd = _pgy_ftable_next++;
    _pgy_ftable[fd] = fp;
    return (int32_t)fd;
}

/* FileRead(fd) → read one line (heap-allocated copy) */
static inline char *
pgy_file_read(int32_t fd)
{
    char tmp[4096];
    tmp[0] = '\0';
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL)
        return pgy_runtime_strdup("");
    if (fgets(tmp, sizeof(tmp), _pgy_ftable[fd]) == NULL)
        return pgy_runtime_strdup("");
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_strdup(tmp);
}

/* FileWrite(fd, data) */
static inline void
pgy_file_write(int32_t fd, const char *data)
{
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), _pgy_ftable[fd]);
}

/* FileClose(fd) */
static inline void
pgy_file_close(int32_t fd)
{
    if (fd < 3 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) return;
    fclose(_pgy_ftable[fd]);
    _pgy_ftable[fd] = NULL;
}

/* ReadFile(path) → entire file as heap-allocated string */
static inline char *
pgy_read_file(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_strdup("");
    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL) return pgy_runtime_strdup("");
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return pgy_runtime_strdup("");
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        return pgy_runtime_strdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return pgy_runtime_strdup("");
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) { fclose(fp); return pgy_runtime_strdup(""); }
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

/* WriteFile(path, data) → write entire string to file */
static inline void
pgy_write_file(const char *path, const char *data)
{
    char *resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return;
    FILE *fp = fopen(resolved, "wb");
    if (fp == NULL) return;
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
    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);
    static char _pgy_input_buf[4096];
    _pgy_input_buf[0] = '\0';
    if (fgets(_pgy_input_buf, sizeof(_pgy_input_buf), stdin) == NULL)
        return _pgy_input_buf;
    size_t len = strlen(_pgy_input_buf);
    if (len > 0 && _pgy_input_buf[len - 1] == '\n')
        _pgy_input_buf[len - 1] = '\0';
    return _pgy_input_buf;
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

/* StringLength is already defined as pgy_string_length or similar */

/* Substring(s, start, len) → new string */
static inline char *
Substring(const char *s, int32_t start, int32_t len)
{
    if (s == NULL) return pgy_runtime_strdup("");
    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0) return pgy_runtime_strdup("");
    if (start + len > slen) len = slen - start;
    char *buf = (char *)malloc((size_t)len + 1);
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
    int count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) { count++; p += old_len; }

    size_t result_len = strlen(s) + (size_t)count * (new_len - old_len);
    char *result = (char *)malloc(result_len + 1);
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
    char *buf = (char *)malloc(la + lb + 1);
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
    PgyArray_String result;
    result.data = NULL;
    result.length = 0;
    result.capacity = 0;
    result.allocator = NULL;
    if (s == NULL || delim == NULL || delim[0] == '\0') {
        return result;
    }
    /* Count tokens first */
    const char *tmp = s;
    size_t count = 0;
    while (*tmp) {
        const char *found = strstr(tmp, delim);
        if (found == NULL) {
            if (*tmp) count++;
            break;
        }
        if (found > tmp) count++;
        tmp = found + strlen(delim);
    }
    if (count == 0) {
        /* No delimiter found — return single element */
        result.capacity = 1;
        result.data = (char **)calloc(1, sizeof(char *));
        result.data[0] = pgy_runtime_strdup(s);
        result.length = 1;
        return result;
    }
    result.capacity = count;
    result.data = (char **)calloc(count, sizeof(char *));
    result.length = 0;
    tmp = s;
    while (*tmp && result.length < count) {
        const char *found = strstr(tmp, delim);
        if (found == NULL) {
            result.data[result.length++] = pgy_runtime_strdup(tmp);
            break;
        }
        size_t token_len = (size_t)(found - tmp);
        if (token_len > 0) {
            char *token = (char *)malloc(token_len + 1);
            memcpy(token, tmp, token_len);
            token[token_len] = '\0';
            result.data[result.length++] = token;
        }
        tmp = found + strlen(delim);
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
        if (arr->data[i]) total += strlen(arr->data[i]);
        if (i < arr->length - 1) total += sep_len;
    }
    char *result = (char *)malloc(total + 1);
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

/* =================================================================
 * QubitSlot — Quantum Resource Simulation
 *
 * Demonstrates that Pergyra's Slot model can express quantum
 * resource semantics: no-cloning, measurement collapse, entanglement.
 *
 * States: 0 = |0>, 1 = |1>, 2 = superposition, -1 = collapsed
 * ================================================================= */

#define PGY_QUBIT_MAX 64

typedef struct {
    int32_t state;       /* 0=|0>, 1=|1>, 2=superposition, -1=released */
    int32_t pool_id;     /* entanglement pool id, -1 if none */
    bool    measured;
} PgyQubit;

/* Entanglement pool — N-qubit group that collapses together */
typedef struct {
    int32_t members[PGY_QUBIT_MAX];
    int32_t count;
    bool    active;
} PgyEntanglementPool;

static PgyQubit _pgy_qubits[PGY_QUBIT_MAX];
static int32_t  _pgy_qubit_next = 0;
static bool     _pgy_qubit_rng_init = false;

static PgyEntanglementPool _pgy_qubit_pools[PGY_QUBIT_MAX];
static int32_t _pgy_qubit_pool_next = 0;

/* --- Pool helpers --- */

static inline int32_t
_pgy_alloc_pool(void)
{
    if (_pgy_qubit_pool_next >= PGY_QUBIT_MAX) return -1;
    int32_t id = _pgy_qubit_pool_next++;
    _pgy_qubit_pools[id].count = 0;
    _pgy_qubit_pools[id].active = true;
    return id;
}

static inline void
_pgy_pool_add(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_MAX) return;
    PgyEntanglementPool *pool = &_pgy_qubit_pools[pool_id];
    if (pool->count >= PGY_QUBIT_MAX) return;
    /* Avoid duplicate */
    for (int32_t i = 0; i < pool->count; i++)
        if (pool->members[i] == qubit_id) return;
    pool->members[pool->count++] = qubit_id;
    _pgy_qubits[qubit_id].pool_id = pool_id;
}

static inline void
_pgy_pool_remove(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_MAX) return;
    PgyEntanglementPool *pool = &_pgy_qubit_pools[pool_id];
    for (int32_t i = 0; i < pool->count; i++) {
        if (pool->members[i] == qubit_id) {
            pool->members[i] = pool->members[pool->count - 1];
            pool->count--;
            return;
        }
    }
}

static inline void
_pgy_pool_merge(int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool) return;
    if (dst_pool < 0 || src_pool < 0) return;
    PgyEntanglementPool *src = &_pgy_qubit_pools[src_pool];
    for (int32_t i = 0; i < src->count; i++) {
        int32_t qid = src->members[i];
        _pgy_pool_add(dst_pool, qid);
        _pgy_qubits[qid].pool_id = dst_pool;
    }
    src->count = 0;
    src->active = false;
}

/* --- Qubit operations --- */

/* ClaimQubit() → allocate a qubit in superposition */
static inline int32_t
ClaimQubit(void)
{
    if (!_pgy_qubit_rng_init) {
        srand((unsigned)time(NULL));
        _pgy_qubit_rng_init = true;
    }
    if (_pgy_qubit_next >= PGY_QUBIT_MAX) return -1;
    int32_t id = _pgy_qubit_next++;
    _pgy_qubits[id].state    = 2; /* superposition */
    _pgy_qubits[id].pool_id  = -1;
    _pgy_qubits[id].measured = false;
    return id;
}

/* Measure(qubit) → collapse superposition, return 0 or 1.
 * Propagates collapse to ALL members of the entanglement pool. */
static inline int32_t
Measure(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return -1;
    PgyQubit *q = &_pgy_qubits[id];

    if (q->measured) {
        return q->state;
    }

    /* Collapse superposition */
    if (q->state == 2) {
        q->state = rand() % 2;
    }
    q->measured = true;

    /* Propagate to entire entanglement pool */
    if (q->pool_id >= 0) {
        PgyEntanglementPool *pool = &_pgy_qubit_pools[q->pool_id];
        for (int32_t i = 0; i < pool->count; i++) {
            int32_t mid = pool->members[i];
            if (mid != id && !_pgy_qubits[mid].measured) {
                _pgy_qubits[mid].state = q->state;
                _pgy_qubits[mid].measured = true;
            }
        }
    }

    return q->state;
}

/* Entangle(a, b) → merge entanglement pools.
 * Supports N-qubit entanglement (GHZ states). */
static inline void
Entangle(int32_t a, int32_t b)
{
    if (a < 0 || a >= PGY_QUBIT_MAX || b < 0 || b >= PGY_QUBIT_MAX) return;
    int32_t pa = _pgy_qubits[a].pool_id;
    int32_t pb = _pgy_qubits[b].pool_id;

    if (pa >= 0 && pb >= 0) {
        if (pa != pb)
            _pgy_pool_merge(pa, pb);
    } else if (pa >= 0) {
        _pgy_pool_add(pa, b);
    } else if (pb >= 0) {
        _pgy_pool_add(pb, a);
    } else {
        int32_t new_pool = _pgy_alloc_pool();
        if (new_pool >= 0) {
            _pgy_pool_add(new_pool, a);
            _pgy_pool_add(new_pool, b);
        }
    }
}

/* H(qubit) → apply Hadamard gate (set to superposition) */
static inline void
H(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return;
    _pgy_qubits[id].state = 2;
    _pgy_qubits[id].measured = false;
}

/* IntoClassical(qubit) → convert collapsed qubit to classical Bool.
 * Returns: 0→false, 1→true. Only valid after Measure(). */
static inline bool
IntoClassical(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return false;
    return _pgy_qubits[id].state == 1;
}

/* QubitState(q) → 0=|0>, 1=|1>, 2=superposition */
static inline int32_t
QubitState(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return -1;
    return _pgy_qubits[id].state;
}

/* IsCollapsed(q) → true if already measured */
static inline bool
IsCollapsed(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return true;
    return _pgy_qubits[id].measured;
}

/* ReleaseQubit(q) → release quantum resource, remove from pool */
static inline void
ReleaseQubit(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return;
    /* Remove from entanglement pool */
    if (_pgy_qubits[id].pool_id >= 0)
        _pgy_pool_remove(_pgy_qubits[id].pool_id, id);
    _pgy_qubits[id].state = -1;
    _pgy_qubits[id].pool_id = -1;
    _pgy_qubits[id].measured = true;
}

#endif /* PGY_RUNTIME_H */
