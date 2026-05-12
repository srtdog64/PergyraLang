/* =================================================================
 * Lock-free SPSC Channel (Single-Producer Single-Consumer)
 *
 * For the common case where one producer feeds one consumer, this
 * avoids all mutex overhead. Uses atomic load/store with appropriate
 * memory ordering on head (consumer) and tail (producer).
 * ================================================================= */

#ifndef __STDC_NO_ATOMICS__
#include <stdatomic.h>

#define PGY_CHANNEL_SPSC_DEFINE(SuffixName, CType) \
\
typedef struct \
{ \
    CType          *buf; \
    size_t          cap; \
    atomic_size_t   head;        /* consumer reads here  */ \
    atomic_size_t   tail;        /* producer writes here */ \
    atomic_bool     closed; \
} PgyChannelSPSC_##SuffixName; \
\
static inline void \
pgy_spsc_init_##SuffixName(PgyChannelSPSC_##SuffixName *ch, size_t capacity) \
{ \
    if (ch == NULL) { \
        pgy_runtime_warn_invalid_channel("spsc_init_" #SuffixName, "null channel"); \
        return; \
    } \
    capacity = pgy_runtime_channel_capacity_or_default("spsc_init_" #SuffixName, capacity); \
    if (capacity > SIZE_MAX / sizeof(CType)) { \
        pgy_runtime_warn_invalid_channel("spsc_init_" #SuffixName, "capacity overflows buffer size"); \
        ch->buf = NULL; \
        ch->cap = 0; \
        return; \
    } \
    ch->buf = (CType *)calloc(capacity, sizeof(CType)); \
    if (ch->buf == NULL) { \
        pgy_runtime_warn_invalid_channel("spsc_init_" #SuffixName, "buffer allocation failed"); \
        ch->cap = 0; \
        return; \
    } \
    ch->cap = capacity; \
    atomic_init(&ch->head, 0); \
    atomic_init(&ch->tail, 0); \
    atomic_init(&ch->closed, false); \
} \
\
static inline void \
pgy_spsc_destroy_##SuffixName(PgyChannelSPSC_##SuffixName *ch) \
{ \
    if (ch == NULL) \
        return; \
    free(ch->buf); \
    ch->buf = NULL; \
    ch->cap = 0; \
} \
\
static inline void \
pgy_spsc_close_##SuffixName(PgyChannelSPSC_##SuffixName *ch) \
{ \
    if (ch == NULL) \
        return; \
    atomic_store_explicit(&ch->closed, true, memory_order_release); \
} \
\
static inline bool \
pgy_spsc_try_send_##SuffixName(PgyChannelSPSC_##SuffixName *ch, CType val) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("spsc_try_send_" #SuffixName, \
            ch == NULL ? "null channel" : "channel is not initialized"); \
        return false; \
    } \
    if (atomic_load_explicit(&ch->closed, memory_order_acquire)) \
        return false; \
    size_t t = atomic_load_explicit(&ch->tail, memory_order_relaxed); \
    size_t h = atomic_load_explicit(&ch->head, memory_order_acquire); \
    if (t - h >= ch->cap) \
        return false; \
    ch->buf[t % ch->cap] = val; \
    atomic_store_explicit(&ch->tail, t + 1, memory_order_release); \
    return true; \
} \
\
static inline bool \
pgy_spsc_send_##SuffixName(PgyChannelSPSC_##SuffixName *ch, CType val) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return false; \
    for (;;) { \
        if (pgy_spsc_try_send_##SuffixName(ch, val)) \
            return true; \
        if (atomic_load_explicit(&ch->closed, memory_order_acquire)) \
            return false; \
        if (pgy_async_in_coroutine()) \
            pgy_async_yield(); \
        else \
            sched_yield(); \
    } \
} \
\
static inline bool \
pgy_spsc_try_recv_##SuffixName(PgyChannelSPSC_##SuffixName *ch, CType *out) \
{ \
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("spsc_try_recv_" #SuffixName, \
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized")); \
        return false; \
    } \
    size_t h = atomic_load_explicit(&ch->head, memory_order_relaxed); \
    size_t t = atomic_load_explicit(&ch->tail, memory_order_acquire); \
    if (h >= t) \
        return false; \
    *out = ch->buf[h % ch->cap]; \
    atomic_store_explicit(&ch->head, h + 1, memory_order_release); \
    return true; \
} \
\
static inline bool \
pgy_spsc_recv_##SuffixName(PgyChannelSPSC_##SuffixName *ch, CType *out) \
{ \
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) \
        return false; \
    for (;;) { \
        if (pgy_spsc_try_recv_##SuffixName(ch, out)) \
            return true; \
        if (atomic_load_explicit(&ch->closed, memory_order_acquire)) { \
            return pgy_spsc_try_recv_##SuffixName(ch, out); \
        } \
        if (pgy_async_in_coroutine()) \
            pgy_async_yield(); \
        else \
            sched_yield(); \
    } \
} \
\
static inline size_t \
pgy_spsc_space_##SuffixName(PgyChannelSPSC_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return 0; \
    size_t t = atomic_load_explicit(&ch->tail, memory_order_relaxed); \
    size_t h = atomic_load_explicit(&ch->head, memory_order_acquire); \
    size_t used = (t >= h) ? (t - h) : 0; \
    return (ch->cap > used) ? (ch->cap - used) : 0; \
} \
\
static inline bool \
pgy_spsc_closed_##SuffixName(PgyChannelSPSC_##SuffixName *ch) \
{ \
    if (ch == NULL) \
        return true; \
    return atomic_load_explicit(&ch->closed, memory_order_acquire); \
}

PGY_CHANNEL_SPSC_DEFINE(Int, int32_t)
PGY_CHANNEL_SPSC_DEFINE(String, char*)

#endif /* __STDC_NO_ATOMICS__ */
