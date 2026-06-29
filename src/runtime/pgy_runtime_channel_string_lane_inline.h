/* =================================================================
 * Channel<String> execution-lane facade.
 *
 * String channel storage and ownership remain in pgy_runtime_channel_string_inline.h.
 * This owner only accepts/rejects the consumed ExecutionLane fact.
 * ================================================================= */

#ifndef PGY_RUNTIME_CHANNEL_STRING_LANE_INLINE_H
#define PGY_RUNTIME_CHANNEL_STRING_LANE_INLINE_H

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_lane_channel_rejected_result_String(const char *operation)
{
    PgyRuntimeChannelStringResult result;
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
    result.err = pgy_runtime_channel_failure_from_status(
        PGY_RUNTIME_CHANNEL_STATUS_REJECTED, operation);
    return result;
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_send_String(PgyExecutionLane lane,
                             PgyChannel_String *ch, char *value)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_send_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_send_String(ch, value);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_try_send_String(PgyExecutionLane lane,
                                 PgyChannel_String *ch, char *value)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_try_send_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_try_send_String(ch, value);
}

PGY_CH_STR_STORAGE PgyOption_Bool
pgy_lane_channel_try_send_status_String(PgyExecutionLane lane,
                                        PgyChannel_String *ch, char *value)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return Some_Bool(false);
    return pgy_channel_try_send_status_String(ch, value);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_send_timeout_String(PgyExecutionLane lane,
                                     PgyChannel_String *ch, char *value,
                                     uint64_t timeout_ns)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_send_timeout_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_send_timeout_String(ch, value, timeout_ns);
}

PGY_CH_STR_STORAGE PgyOption_Bool
pgy_lane_channel_send_timeout_status_String(PgyExecutionLane lane,
                                            PgyChannel_String *ch,
                                            char *value,
                                            uint64_t timeout_ns)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return Some_Bool(false);
    return pgy_channel_send_timeout_status_String(ch, value, timeout_ns);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_recv_String(PgyExecutionLane lane,
                             PgyChannel_String *ch, char **out)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_recv_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_recv_String(ch, out);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_recv_timeout_String(PgyExecutionLane lane,
                                     PgyChannel_String *ch, char **out,
                                     uint64_t timeout_ns)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_recv_timeout_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_recv_timeout_String(ch, out, timeout_ns);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_try_recv_String(PgyExecutionLane lane,
                                 PgyChannel_String *ch, char **out)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) {
        pgy_runtime_warn_invalid_channel("lane_try_recv_String",
            "execution lane rejected channel boundary");
        return false;
    }
    return pgy_channel_try_recv_String(ch, out);
}

PGY_CH_STR_STORAGE bool
pgy_lane_channel_ready_String(PgyExecutionLane lane, PgyChannel_String *ch)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return false;
    return pgy_channel_ready_String(ch);
}

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_lane_channel_recv_result_String(PgyExecutionLane lane,
                                    PgyChannel_String *ch)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return pgy_lane_channel_rejected_result_String("lane_recv_String");
    return pgy_channel_recv_result_String(ch);
}

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_lane_channel_try_recv_result_String(PgyExecutionLane lane,
                                        PgyChannel_String *ch)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return pgy_lane_channel_rejected_result_String("lane_try_recv_String");
    return pgy_channel_try_recv_result_String(ch);
}

PGY_CH_STR_STORAGE PgyRuntimeChannelStringResult
pgy_lane_channel_recv_timeout_result_String(PgyExecutionLane lane,
                                            PgyChannel_String *ch,
                                            uint64_t timeout_ns)
{
    if (!pgy_runtime_channel_lane_accepts_boundary(lane))
        return pgy_lane_channel_rejected_result_String(
            "lane_recv_timeout_String");
    return pgy_channel_recv_timeout_result_String(ch, timeout_ns);
}

PGY_CH_STR_STORAGE char *
pgy_lane_channel_recv_val_String(PgyExecutionLane lane, PgyChannel_String *ch)
{
    PgyRuntimeChannelStringResult result =
        pgy_lane_channel_recv_result_String(lane, ch);
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK ? result.ok : NULL;
}

#endif /* PGY_RUNTIME_CHANNEL_STRING_LANE_INLINE_H */
