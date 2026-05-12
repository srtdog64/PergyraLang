/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "arena.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */

static PgyArenaBlock *
arena_new_block(size_t capacity)
{
    if (capacity > SIZE_MAX - sizeof(PgyArenaBlock))
        return NULL;

    PgyArenaBlock *blk = malloc(sizeof(PgyArenaBlock) + capacity);
    if (blk == NULL)
        return NULL;
    blk->next     = NULL;
    blk->used     = 0;
    blk->capacity = capacity;
    return blk;
}

static bool
arena_ensure(PgyArena *arena, size_t n)
{
    if (arena == NULL)
        return false;

    if (arena->current != NULL
        && n <= arena->current->capacity - arena->current->used)
        return true;

    /* Need a new block. Size is max(block_size, n) to handle oversized requests. */
    size_t cap = arena->block_size;
    if (n > cap) cap = n;

    PgyArenaBlock *blk = arena_new_block(cap);
    if (blk == NULL)
        return false;

    blk->next      = arena->current;
    arena->current = blk;
    return true;
}

/* ------------------------------------------------------------------ */

void
pgy_arena_init(PgyArena *arena, size_t block_size)
{
    arena->current         = NULL;
    arena->block_size      = block_size > 0 ? block_size : PGY_ARENA_DEFAULT_BLOCK_SIZE;
    arena->total_allocated = 0;
}

void
pgy_arena_destroy(PgyArena *arena)
{
    PgyArenaBlock *blk = arena->current;
    while (blk != NULL) {
        PgyArenaBlock *next = blk->next;
        free(blk);
        blk = next;
    }
    arena->current         = NULL;
    arena->total_allocated = 0;
}

void *
pgy_arena_alloc(PgyArena *arena, size_t n)
{
    /* Align to pointer size */
    if (n > SIZE_MAX - (sizeof(void *) - 1))
        return NULL;
    n = (n + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    if (!arena_ensure(arena, n))
        return NULL;

    void *ptr = arena->current->data + arena->current->used;
    arena->current->used += n;
    if (arena->total_allocated <= SIZE_MAX - n)
        arena->total_allocated += n;
    else
        arena->total_allocated = SIZE_MAX;
    return ptr;
}

void *
pgy_arena_calloc(PgyArena *arena, size_t n)
{
    void *ptr = pgy_arena_alloc(arena, n);
    if (ptr != NULL)
        memset(ptr, 0, n);
    return ptr;
}

char *
pgy_arena_strdup(PgyArena *arena, const char *s)
{
    if (s == NULL)
        return NULL;

    size_t len = strlen(s);
    if (len > SIZE_MAX - 1)
        return NULL;
    char *dup = pgy_arena_alloc(arena, len + 1);
    if (dup != NULL)
        memcpy(dup, s, len + 1);
    return dup;
}

char *
pgy_arena_vfmt(PgyArena *arena, const char *fmt, va_list args)
{
    /* Measure first */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0)
        return NULL;

    char *buf = pgy_arena_alloc(arena, (size_t)needed + 1);
    if (buf == NULL)
        return NULL;

    vsnprintf(buf, (size_t)needed + 1, fmt, args);
    return buf;
}

char *
pgy_arena_fmt(PgyArena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *result = pgy_arena_vfmt(arena, fmt, args);
    va_end(args);
    return result;
}
