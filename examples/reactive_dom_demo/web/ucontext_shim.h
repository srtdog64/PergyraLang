#ifndef _PGY_WASM_UCONTEXT_SHIM
#define _PGY_WASM_UCONTEXT_SHIM

/*
 * Minimal ucontext.h shim for wasm32-wasi builds of the reactive demo.
 * Exposed as ucontext.h on the include path. The Pergyra default runtime
 * includes ucontext.h for its coroutine scheduler, which wasm32-wasi does not
 * provide. The reactive demo never enters the scheduler, so these stubs only
 * need to satisfy the include; they are dead-code eliminated.
 */

#include <stddef.h>

typedef struct { void *ss_sp; size_t ss_size; int ss_flags; } pgy_stack_t;

typedef struct ucontext_t {
    struct ucontext_t *uc_link;
    pgy_stack_t uc_stack;
    void *__pad[32];
} ucontext_t;

static inline int getcontext(ucontext_t *u) { (void)u; return 0; }
static inline int setcontext(const ucontext_t *u) { (void)u; return 0; }
static inline void makecontext(ucontext_t *u, void (*f)(void), int a, ...) {
    (void)u; (void)f; (void)a;
}
static inline int swapcontext(ucontext_t *o, const ucontext_t *n) {
    (void)o; (void)n; return 0;
}

#endif
