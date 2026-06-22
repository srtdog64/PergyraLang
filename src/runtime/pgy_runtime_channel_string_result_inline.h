/* =================================================================
 * Channel<String> Result inline adapters.
 * ================================================================= */

#ifndef PGY_RUNTIME_CHANNEL_STRING_RESULT_INLINE_H
#define PGY_RUNTIME_CHANNEL_STRING_RESULT_INLINE_H

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_channel_recv_result_String(PgyChannel_String *ch)
{
    PgyRuntimeChannelStringResult result;
    char *out = NULL;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_String");
        return result;
    }
    if (ch->buf == NULL || ch->cap == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_String");
        return result;
    }
    if (!pgy_channel_recv_String(ch, &out)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY, "recv_String");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_channel_try_recv_result_String(PgyChannel_String *ch)
{
    PgyRuntimeChannelStringResult result;
    char *out = NULL;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "try_recv_String");
        return result;
    }
    if (ch->buf == NULL || ch->cap == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "try_recv_String");
        return result;
    }
    if (!pgy_channel_try_recv_String(ch, &out)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            pgy_channel_closed_String(ch)
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY
                : PGY_RUNTIME_CHANNEL_STATUS_EMPTY,
            "try_recv_String");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_channel_recv_timeout_result_String(PgyChannel_String *ch,
                                       uint64_t timeout_ns)
{
    PgyRuntimeChannelStringResult result;
    char *out = NULL;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_timeout_String");
        return result;
    }
    if (ch->buf == NULL || ch->cap == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_timeout_String");
        return result;
    }
    if (!pgy_channel_recv_timeout_String(ch, &out, timeout_ns)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            pgy_channel_closed_String(ch)
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY
                : PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT,
            "recv_timeout_String");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

PGY_CH_STR_STORAGE char *
pgy_channel_recv_val_String(PgyChannel_String *ch)
{
    PgyRuntimeChannelStringResult result = pgy_channel_recv_result_String(ch);
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK ? result.ok : NULL;
}

#endif /* PGY_RUNTIME_CHANNEL_STRING_RESULT_INLINE_H */
