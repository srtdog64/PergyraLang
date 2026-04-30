/* =================================================================
 * Result Type (Error Handling)
 * ================================================================= */

typedef enum {
    PgyResultOk,
    PgyResultErr
} PgyResultTag;

#define PGY_RESULT_DEFINE(SuffixName, CType, ErrType) \
\
typedef struct { \
    PgyResultTag tag; \
    union { \
        CType ok; \
        ErrType err; \
    }; \
} PgyResult_##SuffixName; \
\
static inline PgyResult_##SuffixName \
pgy_result_ok_##SuffixName(CType value) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultOk; \
    r.ok = value; \
    return r; \
} \
\
static inline PgyResult_##SuffixName \
pgy_result_err_##SuffixName(ErrType err) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultErr; \
    r.err = err; \
    return r; \
} \
\
static inline bool \
pgy_result_is_ok_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    return r->tag == PgyResultOk; \
} \
\
static inline CType \
pgy_result_unwrap_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultOk) { \
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR); \
    } \
    return r->ok; \
} \
\
static inline ErrType \
pgy_result_unwrap_err_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultErr) { \
        PGY_PANIC("Result unwrap_err on Ok value"); \
    } \
    return r->err; \
}

/* Result types for common error types */
typedef const char* PgyError;

PGY_RESULT_DEFINE(Int, int32_t, PgyError)
PGY_RESULT_DEFINE(Bool, bool, PgyError)
PGY_RESULT_DEFINE(String, char*, PgyError)

/* Convenience wrappers for Pergyra language syntax:
 *   Ok(val), Err(msg), IsOk(r), IsErr(r), Unwrap(r), UnwrapOr(r, fallback) */
#define Ok_Int(v)           pgy_result_ok_Int(v)
#define Err_Int(m)          pgy_result_err_Int(m)
#define IsOk_Int(r)         ((r).tag == PgyResultOk)
#define IsErr_Int(r)        ((r).tag == PgyResultErr)
#define Unwrap_Int(r)       pgy_result_unwrap_Int(&(PgyResult_Int){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Int(r, f)  ((r).tag == PgyResultOk ? (r).ok : (f))

#define Ok_Bool(v)          pgy_result_ok_Bool(v)
#define Err_Bool(m)         pgy_result_err_Bool(m)
#define IsOk_Bool(r)        ((r).tag == PgyResultOk)
#define IsErr_Bool(r)       ((r).tag == PgyResultErr)
#define Unwrap_Bool(r)      pgy_result_unwrap_Bool(&(PgyResult_Bool){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Bool(r, f) ((r).tag == PgyResultOk ? (r).ok : (f))

/* Result helper macros (similar to Rust's ? operator)
 * ResultType: the concrete result struct type (e.g. PgyResult_Int)
 */
#define PGY_RESULT_TRY(ResultType, result_expr, ok_var, err_handler) \
    do { \
        ResultType pgy__try_tmp_ = (result_expr); \
        if (pgy__try_tmp_.tag != PgyResultOk) { \
            err_handler(pgy__try_tmp_.err); \
        } \
        (ok_var) = pgy__try_tmp_.ok; \
    } while (0)

/* RemoteFuture<T> -> Result<T>: wraps the raw await pointer in a
 * PgyResult. NULL result -> Err("remote operation failed"),
 * non-NULL -> Ok(value). SuffixName must match PGY_RESULT_DEFINE
 * (e.g. Int, Bool, String). RawCType is the C storage type. */
#define pgy_await_result_take(handle, SuffixName, RawCType) \
    ({ \
        void *_pgy_raw = pgy_await((handle)); \
        PgyResult_##SuffixName _pgy_r; \
        if (_pgy_raw != NULL) { \
            _pgy_r.tag = PgyResultOk; \
            _pgy_r.ok  = *(RawCType *)_pgy_raw; \
            free(_pgy_raw); \
        } else { \
            _pgy_r.tag = PgyResultErr; \
            _pgy_r.err = "remote operation failed"; \
        } \
        _pgy_r; \
    })

/* =================================================================
 * Option Type (Nullable Values)
 * ================================================================= */

typedef enum {
    PgyOptionSome,
    PgyOptionNone
} PgyOptionTag;

#define PGY_OPTION_DEFINE(SuffixName, CType) \
\
typedef struct { \
    PgyOptionTag tag; \
    CType value; \
} PgyOption_##SuffixName; \
\
static inline PgyOption_##SuffixName \
pgy_option_some_##SuffixName(CType value) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionSome; \
    o.value = value; \
    return o; \
} \
\
static inline PgyOption_##SuffixName \
pgy_option_none_##SuffixName(void) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionNone; \
    return o; \
} \
\
static inline bool \
pgy_option_is_some_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    return o->tag == PgyOptionSome; \
} \
\
static inline CType \
pgy_option_unwrap_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    if (o->tag != PgyOptionSome) { \
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE); \
    } \
    return o->value; \
}

PGY_OPTION_DEFINE(Int, int32_t)
PGY_OPTION_DEFINE(Bool, bool)
PGY_OPTION_DEFINE(String, char*)

#define Some_Int(v)             pgy_option_some_Int(v)
#define None_Int()              pgy_option_none_Int()
#define IsSome_Int(o)           ((o).tag == PgyOptionSome)
#define IsNone_Int(o)           ((o).tag == PgyOptionNone)
#define UnwrapOption_Int(o)     pgy_option_unwrap_Int(&(PgyOption_Int){(o).tag, (o).value})

#define Some_Bool(v)            pgy_option_some_Bool(v)
#define None_Bool()             pgy_option_none_Bool()
#define IsSome_Bool(o)          ((o).tag == PgyOptionSome)
#define IsNone_Bool(o)          ((o).tag == PgyOptionNone)
#define UnwrapOption_Bool(o)    pgy_option_unwrap_Bool(&(PgyOption_Bool){(o).tag, (o).value})

#define Some_String(v)          pgy_option_some_String(v)
#define None_String()           pgy_option_none_String()
#define IsSome_String(o)        ((o).tag == PgyOptionSome)
#define IsNone_String(o)        ((o).tag == PgyOptionNone)
#define UnwrapOption_String(o)  pgy_option_unwrap_String(&(PgyOption_String){(o).tag, (o).value})
