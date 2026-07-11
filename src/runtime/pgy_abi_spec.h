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
 * LAST UPDATED: 2026-04-26
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
 * 1. Slot<T> - checked canonical ABI
 *
 * Layout: { value: CType, occupied: bool } + compiler padding
 *
 * The 'occupied' flag enables runtime safety checks (double-free,
 * use-after-release detection). It is present in EVERY build profile
 * (fail-closed: safety must not silently depend on the build profile). The
 * primary use-after-release protection is the static own/ref boundary contract
 * + interprocedural release tracking, which is always on; this runtime flag is
 * the defense-in-depth backstop (FFI/corruption-supplied handles, static-analyzer
 * gaps). Raw/value-only storage must use a distinct ABI owner; it must not
 * reuse PgySlot_* names or change this checked Slot<T> layout.
 * ================================================================ */

/* --- Slot<T> canonical checked ABI --- */
typedef struct { int32_t  value; bool occupied; } pgy_abi_slot_int;
typedef struct { int64_t  value; bool occupied; } pgy_abi_slot_long;
typedef struct { float    value; bool occupied; } pgy_abi_slot_float;
typedef struct { double   value; bool occupied; } pgy_abi_slot_double;
typedef struct { bool     value; bool occupied; } pgy_abi_slot_bool;
typedef struct { char    *value; bool occupied; } pgy_abi_slot_string;

/* ================================================================
 * 2. SecureSlot<T> — Token-Based Access Control
 *
 * Layout: { value: CType, occupied: bool, [padding], token: uint64_t }
 *
 * The token is a capability that gates read/write/release operations.
 * SecureSlot<T> keeps the same token layout and hard-fail checks across
 * debug/release builds. Plain Slot<T> uses the same single-owner ABI rule.
 * Wrong token causes PGY_PANIC.
 * ================================================================ */

/* --- SecureSlot<T> canonical checked ABI --- */
typedef struct { int32_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_int;
typedef struct { int64_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_long;
typedef struct { float   value; bool occupied; uint64_t token; } pgy_abi_secure_slot_float;
typedef struct { double  value; bool occupied; uint64_t token; } pgy_abi_secure_slot_double;
typedef struct { bool    value; bool occupied; uint64_t token; } pgy_abi_secure_slot_bool;
typedef struct { char   *value; bool occupied; uint64_t token; } pgy_abi_secure_slot_string;

/* Debug/release mode is a build policy, not an ABI type-name dimension. */

/* --- Capability Token canonical ABI --- */
typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int;

/* --- Pin/lease view handles (block-scoped runtime views) --- */
typedef struct {
    pgy_abi_slot_int *slot;
    bool active;
    bool can_write;
} pgy_abi_pinned_slot_view_int;

typedef struct {
    pgy_abi_secure_slot_int *slot;
    const pgy_abi_token_int *token;
    bool active;
    bool can_write;
} pgy_abi_pinned_secure_slot_view_int;

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
 * tag and value as needed for alignment. Rust-style niche encoding
 * is not part of this stable ABI; MIRTypeLayout records these rows as
 * MIR_ABI_REPR_EXPLICIT_TAG, not as a backend-inferred niche. See
 * docs/136_abi_niche_and_explicit_layout.md.
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
 * 6. ZoneChannel<T> / WorldChannel<T> — Opaque Channel Handles
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
 * Ordinary Channel<T> lowering in the current beta implementation still uses
 * the legacy local PgyChannel_* runtime storage. It must not be copied into or
 * default-zeroed inside aggregate fields. Opaque channel handles are the
 * ZoneChannel/WorldChannel ABI target and the future path for movable
 * channel-handle lowering.
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
 * Ctrl Block Layout: { strong_count: uint32_t, weak_count: uint32_t,
 * alive: bool, data: T }
 *
 * Weak Layout: { ctrl: PgyRcCtrl* }
 * ================================================================ */

/*
 * Current runtime ABI note:
 * - Rc/Weak handles are pointer-sized wrappers.
 * - The control block uses uint32 strong/weak counts plus an alive bit.
 * - The beta-stable shared ownership subset is single-thread
 *   Int/Long/Float/Double/Bool/String with C/LLVM lifecycle parity.
 */
typedef struct {
    uint32_t strong_count;
    uint32_t weak_count;
    bool     alive;
    int32_t  data;
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
 * Layout: { data: CType*, length: size_t, capacity: size_t,
 *           allocator: void* }
 *
 * The allocator pointer is part of the stable runtime shape. Earlier ABI rows
 * treated Array<T> as a 3-field carrier, but the real PgyArray_* runtime type
 * owns allocator provenance as a fourth field. MIR must model that physical
 * shape instead of letting C/LLVM rediscover it locally.
 * ================================================================ */

typedef struct {
    int32_t *data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_int;

typedef struct {
    int64_t *data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_long;

typedef struct {
    float   *data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_float;

typedef struct {
    double  *data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_double;

typedef struct {
    bool    *data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_bool;

typedef struct {
    char   **data;
    size_t   length;
    size_t   capacity;
    void    *allocator;
} pgy_abi_array_string;

typedef struct {
    int32_t *data;
    size_t   length;
} pgy_abi_slice_int;

typedef struct {
    int64_t *data;
    size_t   length;
} pgy_abi_slice_long;

typedef struct {
    float   *data;
    size_t   length;
} pgy_abi_slice_float;

typedef struct {
    double  *data;
    size_t   length;
} pgy_abi_slice_double;

typedef struct {
    bool    *data;
    size_t   length;
} pgy_abi_slice_bool;

typedef struct {
    char   **data;
    size_t   length;
} pgy_abi_slice_string;

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
    PGY_ABI_ALLOC_SYSTEM     = 0,
    PGY_ABI_ALLOC_TRACING    = 1,
    PGY_ABI_ALLOC_DEBUG      = 2,
    PGY_ABI_ALLOC_POOL       = 3,
    PGY_ABI_ALLOC_SCRATCH    = 4,
    PGY_ABI_ALLOC_RESULT     = 5,
    PGY_ABI_ALLOC_PERSISTENT = 6
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

/* TextBuilder owns its intermediate allocation. It is consumed exactly once
 * by Finish or Drop at the typed language boundary. */
typedef struct {
    char   *data;
    size_t  length;
    size_t  capacity;
    bool    finished;
} pgy_abi_text_builder;

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

#include "pgy_abi_spec_asserts.h"

#endif /* PERGYRA_ABI_SPEC_H */
