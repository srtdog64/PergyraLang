/* =================================================================
 * Channel execution-lane facade macro.
 *
 * The generic bounded-channel owner defines storage and base operations.
 * This owner is only the lane boundary: it accepts/rejects a captured
 * ExecutionLane fact, then forwards to the base operation.
 * ================================================================= */

#ifndef PGY_RUNTIME_CHANNEL_LANE_INLINE_H
#define PGY_RUNTIME_CHANNEL_LANE_INLINE_H

#define PGY_CHANNEL_LANE_DEFINE(SuffixName, CType, PGY_CH_STORAGE) \
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_lane_channel_rejected_result_##SuffixName(const char *operation) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
    result.err = pgy_runtime_channel_failure_from_status( \
        PGY_RUNTIME_CHANNEL_STATUS_REJECTED, operation); \
    return result; \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_send_##SuffixName(PgyExecutionLane lane, \
                                   PgyChannel_##SuffixName *ch, CType value) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_send_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_send_##SuffixName(ch, value); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_try_send_##SuffixName(PgyExecutionLane lane, \
                                       PgyChannel_##SuffixName *ch, CType value) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_try_send_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_try_send_##SuffixName(ch, value); \
} \
\
PGY_CH_STORAGE PgyOption_Bool \
pgy_lane_channel_try_send_status_##SuffixName(PgyExecutionLane lane, \
                                              PgyChannel_##SuffixName *ch, \
                                              CType value) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return Some_Bool(false); \
    return pgy_channel_try_send_status_##SuffixName(ch, value); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_send_timeout_##SuffixName(PgyExecutionLane lane, \
                                           PgyChannel_##SuffixName *ch, \
                                           CType value, uint64_t timeout_ns) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_send_timeout_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_send_timeout_##SuffixName(ch, value, timeout_ns); \
} \
\
PGY_CH_STORAGE PgyOption_Bool \
pgy_lane_channel_send_timeout_status_##SuffixName(PgyExecutionLane lane, \
                                                  PgyChannel_##SuffixName *ch, \
                                                  CType value, \
                                                  uint64_t timeout_ns) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return Some_Bool(false); \
    return pgy_channel_send_timeout_status_##SuffixName(ch, value, \
        timeout_ns); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_recv_##SuffixName(PgyExecutionLane lane, \
                                   PgyChannel_##SuffixName *ch, CType *out) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_recv_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_recv_##SuffixName(ch, out); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_recv_timeout_##SuffixName(PgyExecutionLane lane, \
                                           PgyChannel_##SuffixName *ch, \
                                           CType *out, uint64_t timeout_ns) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_recv_timeout_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_recv_timeout_##SuffixName(ch, out, timeout_ns); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_try_recv_##SuffixName(PgyExecutionLane lane, \
                                       PgyChannel_##SuffixName *ch, CType *out) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) { \
        pgy_runtime_warn_invalid_channel("lane_try_recv_" #SuffixName, \
            "execution lane rejected channel boundary"); \
        return false; \
    } \
    return pgy_channel_try_recv_##SuffixName(ch, out); \
} \
\
PGY_CH_STORAGE bool \
pgy_lane_channel_ready_##SuffixName(PgyExecutionLane lane, \
                                    PgyChannel_##SuffixName *ch) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return false; \
    return pgy_channel_ready_##SuffixName(ch); \
} \
\
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_lane_channel_recv_result_##SuffixName(PgyExecutionLane lane, \
                                          PgyChannel_##SuffixName *ch) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return pgy_lane_channel_rejected_result_##SuffixName( \
            "lane_recv_" #SuffixName); \
    return pgy_channel_recv_result_##SuffixName(ch); \
} \
\
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_lane_channel_try_recv_result_##SuffixName(PgyExecutionLane lane, \
                                              PgyChannel_##SuffixName *ch) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return pgy_lane_channel_rejected_result_##SuffixName( \
            "lane_try_recv_" #SuffixName); \
    return pgy_channel_try_recv_result_##SuffixName(ch); \
} \
\
PGY_CH_STORAGE PgyRuntimeChannel##SuffixName##Result \
pgy_lane_channel_recv_timeout_result_##SuffixName(PgyExecutionLane lane, \
                                                  PgyChannel_##SuffixName *ch, \
                                                  uint64_t timeout_ns) \
{ \
    if (!pgy_runtime_channel_lane_accepts_boundary(lane)) \
        return pgy_lane_channel_rejected_result_##SuffixName( \
            "lane_recv_timeout_" #SuffixName); \
    return pgy_channel_recv_timeout_result_##SuffixName(ch, timeout_ns); \
} \
\
PGY_CH_STORAGE CType \
pgy_lane_channel_recv_val_##SuffixName(PgyExecutionLane lane, \
                                       PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result = \
        pgy_lane_channel_recv_result_##SuffixName(lane, ch); \
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK \
        ? result.ok : (CType)0; \
}

#endif /* PGY_RUNTIME_CHANNEL_LANE_INLINE_H */
