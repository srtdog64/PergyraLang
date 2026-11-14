/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared fixed limits and allocation helpers for the LLVM backend internals.
 */

#ifndef PGY_LLVM_LIMITS_INTERNAL_H
#define PGY_LLVM_LIMITS_INTERNAL_H

#include <limits.h>
#include <stdint.h>

/* Fixed limits -- bounded by nesting depth, reasonable for any program. */
#define MAX_SCOPE_DEPTH     256
#define LLVM_SCOPE_INITIAL_CAPACITY 16
#define MAX_CLASS_FIELDS    256
#define MAX_EVENT_PARAMS    8
#define MAX_DEFER_PER_SCOPE 64
#define PGY_EVENT_MAX_HANDLERS 16
#define MAX_TYPE_SUBST      8

/* Usage: PGY_DYNARR_ENSURE(ptr, count, capacity, Type). */
#define PGY_DYNARR_ENSURE(arr, cnt, cap, T)                          \
    do {                                                              \
        if ((cnt) >= (cap)) {                                         \
            if ((cap) < 0 || (cap) > INT_MAX / 2) {                  \
                llvm_set_error(ctx, "capacity overflow growing " #arr); \
                return;                                               \
            }                                                         \
            int _new_cap = (cap) == 0 ? 16 : (cap) * 2;              \
            if ((size_t)_new_cap > SIZE_MAX / sizeof(T)) {           \
                llvm_set_error(ctx, "allocation overflow growing " #arr); \
                return;                                               \
            }                                                         \
            T *_new = realloc((arr), (size_t)_new_cap * sizeof(T));   \
            if (_new == NULL) {                                       \
                llvm_set_error(ctx, "out of memory growing " #arr);   \
                return;                                               \
            }                                                         \
            memset(_new + (cap), 0,                                   \
                   (size_t)(_new_cap - (cap)) * sizeof(T));           \
            (arr) = _new;                                             \
            (cap) = _new_cap;                                         \
        }                                                             \
    } while (0)

/* Variant that returns NULL instead of void. */
#define PGY_DYNARR_ENSURE_RET(arr, cnt, cap, T)                     \
    do {                                                              \
        if ((cnt) >= (cap)) {                                         \
            if ((cap) < 0 || (cap) > INT_MAX / 2) {                  \
                llvm_set_error(ctx, "capacity overflow growing " #arr); \
                return NULL;                                          \
            }                                                         \
            int _new_cap = (cap) == 0 ? 16 : (cap) * 2;              \
            if ((size_t)_new_cap > SIZE_MAX / sizeof(T)) {           \
                llvm_set_error(ctx, "allocation overflow growing " #arr); \
                return NULL;                                          \
            }                                                         \
            T *_new = realloc((arr), (size_t)_new_cap * sizeof(T));   \
            if (_new == NULL) {                                       \
                llvm_set_error(ctx, "out of memory growing " #arr);   \
                return NULL;                                          \
            }                                                         \
            memset(_new + (cap), 0,                                   \
                   (size_t)(_new_cap - (cap)) * sizeof(T));           \
            (arr) = _new;                                             \
            (cap) = _new_cap;                                         \
        }                                                             \
    } while (0)

#endif /* PGY_LLVM_LIMITS_INTERNAL_H */
