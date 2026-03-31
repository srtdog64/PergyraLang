/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyArena — bump-allocator arena for batch allocation/deallocation.
 *
 * Usage:
 *   PgyArena arena;
 *   pgy_arena_init(&arena, 0);   // 0 = default block size (8 KiB)
 *
 *   char *s = pgy_arena_fmt(&arena, "hello %d", 42);
 *   // no need to free(s)
 *
 *   pgy_arena_destroy(&arena);   // frees all blocks at once
 */

#ifndef PGY_ARENA_H
#define PGY_ARENA_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

#define PGY_ARENA_DEFAULT_BLOCK_SIZE 8192

typedef struct PgyArenaBlock
{
    struct PgyArenaBlock *next;
    size_t used;
    size_t capacity;
    /* data follows (flexible array) */
    char data[];
} PgyArenaBlock;

typedef struct
{
    PgyArenaBlock *current;
    size_t         block_size;
    size_t         total_allocated;  /* diagnostic: total bytes requested */
} PgyArena;

/* Initialize an arena. block_size == 0 uses default (8 KiB). */
void pgy_arena_init(PgyArena *arena, size_t block_size);

/* Free all blocks. Arena is reusable after this. */
void pgy_arena_destroy(PgyArena *arena);

/* Allocate n bytes (not zeroed). Returns NULL on OOM. */
void *pgy_arena_alloc(PgyArena *arena, size_t n);

/* Allocate n bytes, zeroed. */
void *pgy_arena_calloc(PgyArena *arena, size_t n);

/* strdup into arena. */
char *pgy_arena_strdup(PgyArena *arena, const char *s);

/* printf-style format into arena-allocated string. */
char *pgy_arena_fmt(PgyArena *arena, const char *fmt, ...);

/* va_list variant. */
char *pgy_arena_vfmt(PgyArena *arena, const char *fmt, va_list args);

#endif /* PGY_ARENA_H */
