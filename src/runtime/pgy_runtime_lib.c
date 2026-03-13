/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_lib.c — Non-inline runtime symbols for LLVM linking
 *
 * pgy_runtime.h uses static inline functions that are invisible to
 * the LLVM linker. This file provides real (extern) symbol definitions
 * with the EXACT names that LLVM IR references, so the linker can
 * resolve them.
 *
 * We do NOT include pgy_runtime.h here to avoid name collisions
 * between the static inline versions and our extern definitions.
 *
 * Only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =================================================================
 * Log functions
 * ================================================================= */

void pgy_log_int(int32_t v)    { printf("%d\n", v); }
void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
void pgy_log_float(float v)    { printf("%f\n", v); }
void pgy_log_double(double v)  { printf("%lf\n", v); }
void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
void pgy_log_string(char *v)   { printf("%s\n", v ? v : "(null)"); }

/* =================================================================
 * Slot types (must match pgy_runtime.h layout)
 * ================================================================= */

typedef struct {
    int32_t value;
    bool    claimed;
} PgySlot_Int;

typedef struct {
    int64_t value;
    bool    claimed;
} PgySlot_Long;

typedef struct {
    float   value;
    bool    claimed;
} PgySlot_Float;

typedef struct {
    double  value;
    bool    claimed;
} PgySlot_Double;

typedef struct {
    bool    value;
    bool    claimed;
} PgySlot_Bool;

typedef struct {
    char   *value;
    bool    claimed;
} PgySlot_String;

/* =================================================================
 * Slot operations — Int
 * ================================================================= */

PgySlot_Int pgy_claim_Int(void)
{
    PgySlot_Int s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Int(PgySlot_Int *s, int32_t v)
{
    if (s != NULL)
        s->value = v;
}

int32_t pgy_read_Int(PgySlot_Int *s)
{
    if (s != NULL)
        return s->value;
    return 0;
}

void pgy_release_Int(PgySlot_Int *s)
{
    if (s != NULL) {
        s->value = 0;
        s->claimed = false;
    }
}

/* =================================================================
 * Slot operations — Long
 * ================================================================= */

PgySlot_Long pgy_claim_Long(void)
{
    PgySlot_Long s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Long(PgySlot_Long *s, int64_t v)
{
    if (s != NULL) s->value = v;
}

int64_t pgy_read_Long(PgySlot_Long *s)
{
    if (s != NULL) return s->value;
    return 0;
}

void pgy_release_Long(PgySlot_Long *s)
{
    if (s != NULL) { s->value = 0; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Float
 * ================================================================= */

PgySlot_Float pgy_claim_Float(void)
{
    PgySlot_Float s;
    s.value = 0.0f;
    s.claimed = true;
    return s;
}

void pgy_write_Float(PgySlot_Float *s, float v)
{
    if (s != NULL) s->value = v;
}

float pgy_read_Float(PgySlot_Float *s)
{
    if (s != NULL) return s->value;
    return 0.0f;
}

void pgy_release_Float(PgySlot_Float *s)
{
    if (s != NULL) { s->value = 0.0f; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Double
 * ================================================================= */

PgySlot_Double pgy_claim_Double(void)
{
    PgySlot_Double s;
    s.value = 0.0;
    s.claimed = true;
    return s;
}

void pgy_write_Double(PgySlot_Double *s, double v)
{
    if (s != NULL) s->value = v;
}

double pgy_read_Double(PgySlot_Double *s)
{
    if (s != NULL) return s->value;
    return 0.0;
}

void pgy_release_Double(PgySlot_Double *s)
{
    if (s != NULL) { s->value = 0.0; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Bool
 * ================================================================= */

PgySlot_Bool pgy_claim_Bool(void)
{
    PgySlot_Bool s;
    s.value = false;
    s.claimed = true;
    return s;
}

void pgy_write_Bool(PgySlot_Bool *s, bool v)
{
    if (s != NULL) s->value = v;
}

bool pgy_read_Bool(PgySlot_Bool *s)
{
    if (s != NULL) return s->value;
    return false;
}

void pgy_release_Bool(PgySlot_Bool *s)
{
    if (s != NULL) { s->value = false; s->claimed = false; }
}

/* =================================================================
 * Slot operations — String
 * ================================================================= */

PgySlot_String pgy_claim_String(void)
{
    PgySlot_String s;
    s.value = NULL;
    s.claimed = true;
    return s;
}

void pgy_write_String(PgySlot_String *s, char *v)
{
    if (s != NULL)
        s->value = v;
}

char *pgy_read_String(PgySlot_String *s)
{
    if (s != NULL)
        return s->value;
    return NULL;
}

void pgy_release_String(PgySlot_String *s)
{
    if (s != NULL) {
        s->value = NULL;
        s->claimed = false;
    }
}

/* =================================================================
 * Channel — Int (thread-safe with mutex + condvar)
 * ================================================================= */

#include <pthread.h>

typedef struct {
    int32_t        *buffer;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    bool            closed;
    pthread_mutex_t mutex;
    pthread_cond_t  cond_not_full;
    pthread_cond_t  cond_not_empty;
} PgyChannel_Int_RT;

void pgy_channel_init_Int(PgyChannel_Int_RT *ch, size_t cap)
{
    if (ch == NULL) return;
    ch->buffer   = (int32_t *)calloc(cap, sizeof(int32_t));
    ch->capacity = cap;
    ch->head     = 0;
    ch->tail     = 0;
    ch->count    = 0;
    ch->closed   = false;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_not_full, NULL);
    pthread_cond_init(&ch->cond_not_empty, NULL);
}

void pgy_channel_destroy_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_Int(PgyChannel_Int_RT *ch, int32_t v)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed)
        pthread_cond_wait(&ch->cond_not_empty, &ch->mutex);
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_recv_val_Int(PgyChannel_Int_RT *ch)
{
    int32_t out = 0;
    pgy_channel_recv_Int(ch, &out);
    return out;
}

/* =================================================================
 * Thread pool runtime (real pthread-based concurrency)
 *
 * These are non-inline exports of the pgy_parallel.h functions.
 * The LLVM backend links against these symbols.
 * ================================================================= */

#include "runtime/pgy_parallel.h"

/* Force non-inline symbol exports for the linker */
void pgy_pool_init_export(size_t n)    { pgy_pool_init(n); }
void pgy_pool_shutdown_export(void)    { pgy_pool_shutdown(); }

PgyTaskHandle pgy_spawn_export(void *(*fn)(void *), void *arg)
{
    return pgy_spawn(fn, arg);
}

void *pgy_await_export(PgyTaskHandle h)
{
    return pgy_await(h);
}

#endif /* PGY_LLVM_ENABLED */
