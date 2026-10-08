#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PGY_INTENT_IDENTITY_LINKED
#define PGY_LLVM_ENABLED
#include "../src/runtime/pgy_runtime_lib.c"
#define current_handle pgy_intent_current_handle_export
#define find_active pgy_intent_find_active_entry_locked_export
#else
#include "../src/runtime/pgy_runtime.h"
#define current_handle pgy_intent_current_handle
#define find_active pgy_intent_find_active_entry_locked
#endif

static pthread_barrier_t entered;
static pthread_barrier_t checked;
static int subject_a, subject_b;
static int32_t other_handle;

static void *other_thread(void *unused)
{
    void *subjects[] = { &subject_b };
    (void)unused;
    other_handle = pgy_intent_enter_export("unrelated", subjects, 1, false, 0);
    assert(other_handle == INT32_MAX - 1);
    pthread_barrier_wait(&entered);
    pthread_barrier_wait(&checked);
    assert(find_active(other_handle) != NULL);
    assert(!find_active(other_handle)->failed);
    pgy_intent_exit_export(other_handle);
    return NULL;
}

int main(void)
{
    void *a[] = { &subject_a }, *b[] = { &subject_b };
    pthread_t other;
    int32_t root = pgy_intent_enter_export("root", a, 1, false, 0);
    int32_t child = pgy_intent_enter_export("child", a, 1, false, 0);
    PgyIntentActiveEntry *retired_registry_storage = find_active(root);
    assert(root == 1 && child == 2);
    assert(current_handle() == child);
    assert(find_active(child)->parent_handle == root);

    /* Non-LIFO retirement leaves a stale parent edge, not a new authority. */
    pgy_intent_exit_export(root);
    assert(find_active(root) == NULL && find_active(child) != NULL);
    assert(current_handle() == child);
    pgy_intent_next_handle = INT32_MAX - 1;
    assert(pthread_barrier_init(&entered, NULL, 2) == 0);
    assert(pthread_barrier_init(&checked, NULL, 2) == 0);
    assert(pthread_create(&other, NULL, other_thread, NULL) == 0);
    pthread_barrier_wait(&entered);

    /* Registry storage was recycled, but its new public identity is not the
     * retired parent's identity. Neither conflict waiver nor stale exit/trace
     * may treat that unrelated thread as the child's ancestor. */
    assert(other_handle != root);
    assert(find_active(other_handle) == retired_registry_storage);
    assert(pgy_intent_enter_export("conflicting", b, 1, false, 0) == 0);
    pgy_intent_trace_fail_export(root, "stale caller");
    pgy_intent_exit_export(root);
    assert(find_active(other_handle) != NULL);
    pthread_barrier_wait(&checked);
    assert(pthread_join(other, NULL) == 0);

    int32_t last = pgy_intent_enter_export("last", NULL, 0, false, 0);
    assert(last == INT32_MAX);
    assert(find_active(last)->parent_handle == child);
    pgy_intent_exit_export(last);
    assert(pgy_intent_next_handle == 0);
    assert(pgy_intent_enter_export("exhausted", NULL, 0, false, 0) == 0);
    assert(pgy_intent_enter_export("still exhausted", NULL, 0, false, 0) == 0);
    assert(pgy_intent_next_handle == 0);
    assert(find_active(child) != NULL && find_active(root) == NULL);
    pgy_intent_exit_export(child);
    assert(current_handle() == 0);
    assert(pgy_intent_enter_export("empty but exhausted", NULL, 0, false, 0) == 0);
    pthread_barrier_destroy(&entered);
    pthread_barrier_destroy(&checked);
    puts("intent identity: nested admission, non-LIFO reuse, cross-thread denial, stale caller and exhaustion PASS");
    return 0;
}
