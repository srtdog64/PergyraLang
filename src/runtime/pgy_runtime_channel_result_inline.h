#ifndef PGY_RUNTIME_CHANNEL_RESULT_INLINE_H
#define PGY_RUNTIME_CHANNEL_RESULT_INLINE_H

/* Failure-as-data projections layered over the channel core operations. */
#define PGY_CHANNEL_RESULT_DEFINE(SuffixName, CType, PGY_CH_STORAGE) \
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_channel_recv_result_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_recv_##SuffixName(ch, &out)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY, "recv_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_channel_try_recv_result_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "try_recv_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "try_recv_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_try_recv_##SuffixName(ch, &out)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            pgy_channel_closed_##SuffixName(ch) \
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY \
                : PGY_RUNTIME_CHANNEL_STATUS_EMPTY, \
            "try_recv_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_channel_recv_timeout_result_##SuffixName(PgyChannel_##SuffixName *ch, \
                                             uint64_t timeout_ns) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_recv_timeout_##SuffixName(ch, &out, timeout_ns)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            pgy_channel_closed_##SuffixName(ch) \
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY \
                : PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
PGY_CH_STORAGE CType \
pgy_channel_recv_val_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result = \
        pgy_channel_recv_result_##SuffixName(ch); \
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK \
        ? result.ok : (CType)0; \
}

#endif /* PGY_RUNTIME_CHANNEL_RESULT_INLINE_H */
