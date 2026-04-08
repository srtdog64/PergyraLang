/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_abi_spec.h — Pergyra ABI Specification (Single Source of Truth)
 *
 * PURPOSE:
 *   This file defines the EXACT memory layout of every core Pergyra type.
 *   It is the contract between the Pergyra compiler (MIR layer) and
 *   the C/LLVM backends. Neither backend is allowed to invent its own
 *   struct layout — they must use the layouts defined here.
 *
 *   Backends that create their own LLVMStructType or emit their own
 *   C struct names without consulting this file are in violation of
 *   the ABI unification policy.
 *
 * USAGE:
 *   - This header is NOT included by the runtime library (pgy_runtime.h).
 *   - It is included ONLY by test_abi_spec.c and compiler ABI validation.
 *   - The "real" runtime types in pgy_runtime.h MUST match this spec.
 *   - Add new types here FIRST, then update pgy_runtime.h macros.
 *
 * BUILD:
 *   make test-abi   →   compiles and runs src/test_abi_spec.c
 *
 * LAST UPDATED: 2026-04-08
 */

#ifndef PERGYRA_ABI_SPEC_H
#define PERGYRA_ABI_SPEC_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * Static Assert Support
 * ================================================================ */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ABI_STATIC_ASSERT(cond, msg) _Static_assert(cond, #msg)
#elif defined(__cplusplus)
    #define ABI_STATIC_ASSERT(cond, msg) static_assert(cond, #msg)
#else
    #define ABI_STATIC_ASSERT(cond, msg) \
        typedef char abi_static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ================================================================
 * Platform Abstraction Layer
 *
 * The Pergyra compiler must know the size of synchronization
 * primitives to compute struct layouts. We abstract them here
 * so the ABI spec compiles on both Linux and Windows.
 * ================================================================ */

#if defined(_MSC_VER)
    /* MSVC: No pthreads, use Windows SRWLOCK + CONDITION_VARIABLE */
    typedef struct { void *_ptr; uintptr_t _reserved[5]; } pgy_abi_mutex_t;
    typedef struct { void *_ptr; uintptr_t _reserved[4]; } pgy_abi_condvar_t;
#else
    /* POSIX / MinGW-w64 winpthreads: use real pthread types */
    #include <pthread.h>
    typedef pthread_mutex_t pgy_abi_mutex_t;
    typedef pthread_cond_t  pgy_abi_condvar_t;
#endif

/* ================================================================
 * 1. Slot<T> — Debug Mode (PGY_WITH_SLOT_CHECKS or PGY_DEBUG)
 *
 * Layout: { value: CType, occupied: bool } + compiler padding
 *
 * The 'occupied' flag enables runtime safety checks (double-free,
 * use-after-release detection). In release mode this field is
 * eliminated for zero overhead.
 * ================================================================ */

/* --- Slot<T> Debug --- */
typedef struct { int32_t  value; bool occupied; } pgy_abi_slot_int_dbg;
typedef struct { int64_t  value; bool occupied; } pgy_abi_slot_long_dbg;
typedef struct { float    value; bool occupied; } pgy_abi_slot_float_dbg;
typedef struct { double   value; bool occupied; } pgy_abi_slot_double_dbg;
typedef struct { bool     value; bool occupied; } pgy_abi_slot_bool_dbg;
typedef struct { char    *value; bool occupied; } pgy_abi_slot_string_dbg;

/* --- Slot<T> Release (zero-overhead) --- */
typedef struct { int32_t  value; } pgy_abi_slot_int_rel;
typedef struct { int64_t  value; } pgy_abi_slot_long_rel;
typedef struct { float    value; } pgy_abi_slot_float_rel;
typedef struct { double   value; } pgy_abi_slot_double_rel;
typedef struct { bool     value; } pgy_abi_slot_bool_rel;
typedef struct { char    *value; } pgy_abi_slot_string_rel;

/* ================================================================
 * 2. SecureSlot<T> — Token-Based Access Control
 *
 * Layout (Debug): { value: CType, occupied: bool, [padding], token: uint64_t }
 * Layout (Release): same struct, fewer assert checks
 *
 * The token is a capability that gates read/write/release operations.
 * Wrong token → PGY_PANIC.
 * ================================================================ */

/* --- SecureSlot<T> Debug --- */
typedef struct { int32_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_int_dbg;
typedef struct { int64_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_long_dbg;
typedef struct { float   value; bool occupied; uint64_t token; } pgy_abi_secure_slot_float_dbg;
typedef struct { double  value; bool occupied; uint64_t token; } pgy_abi_secure_slot_double_dbg;
typedef struct { bool    value; bool occupied; uint64_t token; } pgy_abi_secure_slot_bool_dbg;
typedef struct { char   *value; bool occupied; uint64_t token; } pgy_abi_secure_slot_string_dbg;

/* --- SecureSlot<T> Release --- */
typedef struct { int32_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_int_rel;
typedef struct { char   *value; bool occupied; uint64_t token; } pgy_abi_secure_slot_string_rel;

/* --- Capability Token --- */
typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int_dbg;
typedef struct { uint64_t id; } pgy_abi_token_int_rel;

/* ================================================================
 * 3. DeviceSlot<T> — Anchored External Resource Cell
 *
 * Layout: { value: CType, claimed: bool } + padding
 *
 * Same ownership surface as Slot<T> but used for device/remote
 * boundaries. Supports async read submission (returns PgyTaskHandle).
 * ================================================================ */

typedef struct { int32_t value; bool claimed; } pgy_abi_device_slot_int;
typedef struct { int64_t value; bool claimed; } pgy_abi_device_slot_long;
typedef struct { float   value; bool claimed; } pgy_abi_device_slot_float;
typedef struct { double  value; bool claimed; } pgy_abi_device_slot_double;
typedef struct { bool    value; bool claimed; } pgy_abi_device_slot_bool;
typedef struct { char   *value; bool claimed; } pgy_abi_device_slot_string;

/* ================================================================
 * 4. Option<T> — Nullable Value Type
 *
 * Layout: { tag: int32_t (Some=0 / None=1), value: CType } + padding
 *
 * Tag is ALWAYS int32_t (4 bytes). Value follows at the next
 * naturally aligned offset. The compiler inserts padding between
 * tag and value as needed for alignment.
 * ================================================================ */

typedef enum {
    PgyAbiOptionSome = 0,
    PgyAbiOptionNone = 1
} PgyAbiOptionTag;

typedef struct { int32_t tag; int32_t  value; } pgy_abi_option_int;
typedef struct { int32_t tag; int64_t  value; } pgy_abi_option_long;
typedef struct { int32_t tag; float    value; } pgy_abi_option_float;
typedef struct { int32_t tag; double   value; } pgy_abi_option_double;
typedef struct { int32_t tag; bool     value; } pgy_abi_option_bool;
typedef struct { int32_t tag; char    *value; } pgy_abi_option_string;

/* ================================================================
 * 5. Result<T, E> — Error Handling Type
 *
 * Layout: { tag: int32_t (Ok=0 / Err=1), union { T ok; E err; } } + padding
 *
 * The union's size is max(sizeof(T), sizeof(E)).
 * The overall struct size is sizeof(tag) + sizeof(union) + padding.
 *
 * Default error type: PgyError = const char* (error message string).
 * ================================================================ */

typedef enum {
    PgyAbiResultOk  = 0,
    PgyAbiResultErr = 1
} PgyAbiResultTag;

typedef const char* PgyAbiError;

typedef struct {
    int32_t tag;
    union { int32_t  ok;  PgyAbiError err; };
} pgy_abi_result_int;

typedef struct {
    int32_t tag;
    union { bool     ok;  PgyAbiError err; };
} pgy_abi_result_bool;

typedef struct {
    int32_t tag;
    union { char    *ok;  PgyAbiError err; };
} pgy_abi_result_string;

/* ================================================================
 * 6. Channel<T> — Thread-Safe Bounded Ring Buffer
 *
 * ⚠️  DESIGN DECISION: Opaque Handle (NOT a full struct)
 *
 * Channel은 mutex, condvar, ring buffer 등을 포함하므로
 * 플랫폼마다 크기가 완전히 다르다 (Linux ~176바이트, Windows ~72바이트).
 * 이를 ABI로 고정하는 것은 오히려 역효과다.
 *
 * 대신 Channel을 opaque handle (uint32_t)로 추상화하고,
 * 실제 body는 Zone Arena에 숨긴다.
 *
 * Ownership Model (2-Tier):
 *   ZoneChannelHandle  — Zone 내부 전용, Zone 종료 시 자동 정리
 *   WorldChannelHandle — 다중 Zone 공유, World 종료 시 정리
 *
 * 컴파일러 규칙:
 *   - ZoneChannel은 Zone 밖으로 escape 불가
 *   - WorldChannel만 cross-zone 공유 가능
 *   - Stack 할당 금지. 항상 handle만 스택에.
 * ================================================================ */

/* --- Zone Channel (기본) --- */
typedef uint32_t pgy_abi_zone_channel_handle;

/* --- World Channel (고급 — cross-zone) --- */
typedef uint32_t pgy_abi_world_channel_handle;

/*
 * Runtime API (명시적 함수, 매크로 아님):
 *
 *   pgy_abi_zone_channel_handle pgy_zone_channel_create_int(PgyArena* arena, size_t cap);
 *   bool  pgy_zone_channel_send_int(pgy_abi_zone_channel_handle h, int32_t v);
 *   int32_t pgy_zone_channel_recv_int(pgy_abi_zone_channel_handle h);
 *   void  pgy_zone_channel_close_int(pgy_abi_zone_channel_handle h);
 *   [destroy는 Zone Arena 일괄 정리 -- 개별 호출 불필요]
 *
 *   pgy_abi_world_channel_handle pgy_world_channel_create_int(PgyArena* arena, size_t cap);
 *   bool  pgy_world_channel_send_int(pgy_abi_world_channel_handle h, int32_t v);
 *   int32_t pgy_world_channel_recv_int(pgy_abi_world_channel_handle h);
 *   void  pgy_world_channel_close_int(pgy_abi_world_channel_handle h);
 *   void  pgy_world_channel_destroy_int(pgy_abi_world_channel_handle h);
 *
 * PgyChannel_Int (legacy runtime type) is kept for backward compat only;
 * new MIR lowering uses opaque handles exclusively.
 */

/* ================================================================
 * 7. Box<T> — Owned Heap Allocation (Move Semantics)
 *
 * Layout: { ptr: CType* }
 *
 * Simply a typed pointer wrapper. Size == sizeof(pointer).
 * Move semantics are implemented by nullifying the source pointer.
 * ================================================================ */

typedef struct { int32_t  *ptr; } pgy_abi_box_int;
typedef struct { int64_t  *ptr; } pgy_abi_box_long;
typedef struct { float    *ptr; } pgy_abi_box_float;
typedef struct { double   *ptr; } pgy_abi_box_double;
typedef struct { bool     *ptr; } pgy_abi_box_bool;
typedef struct { char     *ptr; } pgy_abi_box_string;

/* ================================================================
 * 8. Rc<T> / Weak<T> — Reference Counted + Weak References
 *
 * Rc Layout: { ctrl: PgyRcCtrl*, value: (none — ctrl holds data) }
 *   Actually: Rc is just a pointer to control block + data.
 *
 * Ctrl Block Layout: { strong: int64_t, weak: int64_t, data: T }
 *
 * Weak Layout: { ctrl: PgyRcCtrl* }
 * ================================================================ */

typedef struct {
    int64_t strong_count;
    int64_t weak_count;
    int32_t data;  /* flexible array member follows in real impl */
} pgy_abi_rc_ctrl_int;

typedef struct {
    pgy_abi_rc_ctrl_int *ctrl;
} pgy_abi_rc_int;

typedef struct {
    pgy_abi_rc_ctrl_int *ctrl;
} pgy_abi_weak_int;

/* ================================================================
 * 9. Array<T> — Dynamic Array (Heap)
 *
 * Layout: { data: CType*, len: size_t, cap: size_t }
 * ================================================================ */

typedef struct {
    int32_t *data;
    size_t   len;
    size_t   cap;
} pgy_abi_array_int;

typedef struct {
    char   **data;
    size_t   len;
    size_t   cap;
} pgy_abi_array_string;

/* ================================================================
 * 10. QubitSlot — Quantum Resource Simulation
 *
 * Layout: { state: int32_t, pool_id: int32_t, measured: bool } + padding
 *
 * States: 0=|0>, 1=|1>, 2=superposition, -1=collapsed/released
 * ================================================================ */

typedef struct {
    int32_t state;
    int32_t pool_id;
    bool    measured;
} pgy_abi_qubit;

/* ================================================================
 * 11. TaskHandle — Async/Spawn Handle
 *
 * Layout: { id: int32_t, valid: bool } + padding
 * ================================================================ */

typedef struct {
    int32_t id;
    bool    valid;
} pgy_abi_task_handle;

/* ================================================================
 * 12. Timer — Simple Countdown Timer
 *
 * Layout: { duration: int32_t, remaining: int32_t, done: bool } + padding
 * ================================================================ */

typedef struct {
    int32_t duration;
    int32_t remaining;
    bool    done;
} pgy_abi_timer;

/* ================================================================
 * 13. Arena — Frame-Based Allocator
 *
 * Layout: { buffer: char*, capacity: size_t, offset: size_t }
 * ================================================================ */

typedef struct {
    char   *buffer;
    size_t  capacity;
    size_t  offset;
} pgy_abi_arena;

/* ================================================================
 * 14. Allocator — Tracing/Pool Allocator Descriptor
 *
 * Layout: depends on PgyAllocatorKind enum + counters
 * ================================================================ */

typedef enum {
    PGY_ABI_ALLOC_SYSTEM  = 0,
    PGY_ABI_ALLOC_TRACING = 1,
    PGY_ABI_ALLOC_DEBUG   = 2,
    PGY_ABI_ALLOC_POOL    = 3
} PgyAbiAllocatorKind;

typedef struct {
    char                *buffer;
    size_t               capacity;
    size_t               offset;
} PgyAbiPoolAllocatorState;

typedef struct {
    PgyAbiAllocatorKind   kind;
    bool                  trace_enabled;
    bool                  debug_enabled;
    size_t                allocations;
    size_t                deallocations;
    size_t                bytes_in_use;
    size_t                peak_bytes;
    PgyAbiPoolAllocatorState *pool;
} pgy_abi_allocator;

/* =================================================================
 * 15. Future<T> / RemoteFuture<T> — Async Result Handles
 *
 * Layout: { handle: int32_t, ready: bool } + padding
 *
 * RemoteFuture carries additional trace metadata for distributed
 * operation tracking.
 * ================================================================= */

typedef struct {
    int32_t handle;
    bool    ready;
} pgy_abi_future;

typedef struct {
    int32_t  handle;
    bool     ready;
    int32_t  trace_id;
    char    *trace_data;
} pgy_abi_remote_future;

/* =================================================================
 * STATIC ASSERTIONS — Slot<T> Debug
 * ================================================================= */

/* Slot<Int> Debug: value@0, occupied after value, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int_dbg, value) == 0,
                  slot_int_dbg_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int_dbg, occupied) >= 4,
                  slot_int_dbg_occupied_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_int_dbg) >= 8,
                  slot_int_dbg_min_size_8);

/* Slot<Long> Debug: value@0, size >= 16 (8 + 1 + 7 padding) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_long_dbg, value) == 0,
                  slot_long_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_long_dbg) >= 16,
                  slot_long_dbg_min_size_16);

/* Slot<Float> Debug: value@0, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_float_dbg, value) == 0,
                  slot_float_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_float_dbg) >= 8,
                  slot_float_dbg_min_size_8);

/* Slot<Double> Debug: value@0, size >= 16 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_double_dbg, value) == 0,
                  slot_double_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_double_dbg) >= 16,
                  slot_double_dbg_min_size_16);

/* Slot<Bool> Debug: size >= 2 */
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_bool_dbg) >= 2,
                  slot_bool_dbg_min_size_2);

/* Slot<String> Debug: value@0, size >= 16 (8 + 1 + 7 padding on LP64) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_string_dbg, value) == 0,
                  slot_string_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_string_dbg) >= 16,
                  slot_string_dbg_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — Slot<T> Release
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_int_rel) == 4,
                  slot_int_rel_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_long_rel) == 8,
                  slot_long_rel_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_float_rel) == 4,
                  slot_float_rel_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_double_rel) == 8,
                  slot_double_rel_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_bool_rel) == 1,
                  slot_bool_rel_size_1);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_string_rel) == sizeof(char*),
                  slot_string_rel_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — SecureSlot<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int_dbg, value) == 0,
                  secure_slot_int_dbg_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int_dbg, token) > 4,
                  secure_slot_int_dbg_token_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int_dbg) > sizeof(pgy_abi_slot_int_dbg),
                  secure_slot_int_dbg_larger_than_slot);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int_dbg) >= 16,
                  secure_slot_int_dbg_min_size_16);

ABI_STATIC_ASSERT(sizeof(pgy_abi_token_int_dbg) >= 16,
                  token_int_dbg_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — DeviceSlot<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_device_slot_int, value) == 0,
                  device_slot_int_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_device_slot_int) >= 8,
                  device_slot_int_min_size_8);

/* =================================================================
 * STATIC ASSERTIONS — Option<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_int, tag) == 0,
                  option_int_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_int, value) == 4,
                  option_int_value_at_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_int) == 8,
                  option_int_size_8);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_long, tag) == 0,
                  option_long_tag_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_long) >= 16,
                  option_long_min_size_16);

ABI_STATIC_ASSERT(sizeof(pgy_abi_option_string) >= 16,
                  option_string_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — Result<T, E>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_result_int, tag) == 0,
                  result_int_tag_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_result_int) >= 16,
                  result_int_min_size_16);

ABI_STATIC_ASSERT(offsetof(pgy_abi_result_bool, tag) == 0,
                  result_bool_tag_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_result_bool) >= 16,
                  result_bool_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — Channel<T> (opaque handles — platform-independent)
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_zone_channel_handle) == 4,
                  zone_channel_handle_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_world_channel_handle) == 4,
                  world_channel_handle_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_zone_channel_handle) == sizeof(uint32_t),
                  zone_channel_handle_is_u32);
ABI_STATIC_ASSERT(sizeof(pgy_abi_world_channel_handle) == sizeof(uint32_t),
                  world_channel_handle_is_u32);

/* =================================================================
 * STATIC ASSERTIONS — Box<T>
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_box_int) == sizeof(void*),
                  box_int_size_ptr);
ABI_STATIC_ASSERT(sizeof(pgy_abi_box_string) == sizeof(void*),
                  box_string_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — Rc/Weak
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_rc_ctrl_int) >= 20,
                  rc_ctrl_int_min_size);
ABI_STATIC_ASSERT(sizeof(pgy_abi_rc_int) == sizeof(void*),
                  rc_int_size_ptr);
ABI_STATIC_ASSERT(sizeof(pgy_abi_weak_int) == sizeof(void*),
                  weak_int_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — Array<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_array_int, data) == 0,
                  array_int_data_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_int) >= 24,
                  array_int_min_size_24);

/* =================================================================
 * STATIC ASSERTIONS — Miscellaneous
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_qubit) >= 12,
                  qubit_min_size_12);
ABI_STATIC_ASSERT(sizeof(pgy_abi_task_handle) >= 8,
                  task_handle_min_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_timer) >= 12,
                  timer_min_size_12);
ABI_STATIC_ASSERT(sizeof(pgy_abi_arena) >= 24,
                  arena_min_size_24);
ABI_STATIC_ASSERT(sizeof(pgy_abi_allocator) >= 48,
                  allocator_min_size_48);
ABI_STATIC_ASSERT(sizeof(pgy_abi_future) >= 8,
                  future_min_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_remote_future) >= 24,
                  remote_future_min_size_24);

#endif /* PERGYRA_ABI_SPEC_H */
