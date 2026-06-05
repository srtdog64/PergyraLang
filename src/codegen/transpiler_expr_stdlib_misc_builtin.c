#include "transpiler_expr_stdlib_misc_builtin.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"

static char *
transpiler_misc_strdup_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: misc builtin expression formatting failed");
        return NULL;
    }

    char *s = malloc((size_t)n + 1);
    if (s == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: misc builtin expression allocation failed");
        return NULL;
    }

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

typedef enum {
    TRANSPILER_MISC_OP_NONE = 0,
    TRANSPILER_MISC_OP_COOLDOWN_NEW,
    TRANSPILER_MISC_OP_COOLDOWN_READY,
    TRANSPILER_MISC_OP_COOLDOWN_TICK,
    TRANSPILER_MISC_OP_COOLDOWN_TRIGGER,
    TRANSPILER_MISC_OP_FSM_ADD_STATE,
    TRANSPILER_MISC_OP_FSM_CURRENT,
    TRANSPILER_MISC_OP_FSM_CURRENT_NAME,
    TRANSPILER_MISC_OP_FSM_NEW,
    TRANSPILER_MISC_OP_FSM_STEP,
    TRANSPILER_MISC_OP_FSM_TRANSITION,
    TRANSPILER_MISC_OP_MAP_GET_STR,
    TRANSPILER_MISC_OP_MAP_SET_STR,
    TRANSPILER_MISC_OP_TIMER_DONE,
    TRANSPILER_MISC_OP_TIMER_NEW,
    TRANSPILER_MISC_OP_TIMER_REMAINING,
    TRANSPILER_MISC_OP_TIMER_RESET,
    TRANSPILER_MISC_OP_TIMER_TICK,
} TranspilerMiscOp;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerMiscOp op;
} TranspilerMiscSpec;

static const TranspilerMiscSpec kTranspilerMiscSpecs[] = {
    {"CooldownNew", 1, TRANSPILER_MISC_OP_COOLDOWN_NEW},
    {"CooldownReady", 1, TRANSPILER_MISC_OP_COOLDOWN_READY},
    {"CooldownTick", 2, TRANSPILER_MISC_OP_COOLDOWN_TICK},
    {"CooldownTrigger", 1, TRANSPILER_MISC_OP_COOLDOWN_TRIGGER},
    {"FsmAddState", 2, TRANSPILER_MISC_OP_FSM_ADD_STATE},
    {"FsmCurrent", 1, TRANSPILER_MISC_OP_FSM_CURRENT},
    {"FsmCurrentName", 1, TRANSPILER_MISC_OP_FSM_CURRENT_NAME},
    {"FsmNew", (size_t)-1, TRANSPILER_MISC_OP_FSM_NEW},
    {"FsmStep", 2, TRANSPILER_MISC_OP_FSM_STEP},
    {"FsmTransition", 4, TRANSPILER_MISC_OP_FSM_TRANSITION},
    {"MapGetStr", 2, TRANSPILER_MISC_OP_MAP_GET_STR},
    {"MapSetStr", 3, TRANSPILER_MISC_OP_MAP_SET_STR},
    {"TimerDone", 1, TRANSPILER_MISC_OP_TIMER_DONE},
    {"TimerNew", 1, TRANSPILER_MISC_OP_TIMER_NEW},
    {"TimerRemaining", 1, TRANSPILER_MISC_OP_TIMER_REMAINING},
    {"TimerReset", 1, TRANSPILER_MISC_OP_TIMER_RESET},
    {"TimerTick", 2, TRANSPILER_MISC_OP_TIMER_TICK},
};

static int
transpiler_misc_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerMiscSpec *spec = (const TranspilerMiscSpec *)entry;
    return strcmp(name, spec->name);
}

static TranspilerMiscOp
transpiler_misc_lookup(const char *fn, size_t argc)
{
    const TranspilerMiscSpec *spec;

    if (fn == NULL)
        return TRANSPILER_MISC_OP_NONE;
    spec = (const TranspilerMiscSpec *)bsearch(
        fn,
        kTranspilerMiscSpecs,
        sizeof(kTranspilerMiscSpecs) / sizeof(kTranspilerMiscSpecs[0]),
        sizeof(kTranspilerMiscSpecs[0]),
        transpiler_misc_spec_compare);
    if (spec == NULL)
        return TRANSPILER_MISC_OP_NONE;
    if (spec->argc != (size_t)-1 && spec->argc != argc)
        return TRANSPILER_MISC_OP_NONE;
    return spec->op;
}

static char *
transpiler_misc_emit_arg(TranspilerCtx *ctx,
                         ASTNode *arg,
                         const char *builtin_name,
                         const char *role)
{
    char *rendered = emit_expression(arg, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: misc builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
}

char *
emit_call_stdlib_misc_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    TranspilerMiscOp op = transpiler_misc_lookup(fn, argc);
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);
    ASTNode *a3 = ast_call_argument(call, 3);

    if (op == TRANSPILER_MISC_OP_FSM_NEW)
        return pergyra_strdup("pgy_fsm_new()");
    if (op == TRANSPILER_MISC_OP_FSM_ADD_STATE) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "FsmAddState", "FSM"))
            return NULL;
        char *f = transpiler_misc_emit_arg(ctx, a0, "FsmAddState", "fsm");
        char *n = f != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "FsmAddState", "name")
            : NULL;
        if (f == NULL || n == NULL) {
            free(f);
            free(n);
            return NULL;
        }
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_fsm_add_state(&%s, %s)", f, n);
        free(f); free(n); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_TRANSITION) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "FsmTransition", "FSM"))
            return NULL;
        char *f = transpiler_misc_emit_arg(ctx, a0, "FsmTransition", "fsm");
        char *from = f != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "FsmTransition", "from")
            : NULL;
        char *inp = from != NULL
            ? transpiler_misc_emit_arg(ctx, a2, "FsmTransition", "input")
            : NULL;
        char *to = inp != NULL
            ? transpiler_misc_emit_arg(ctx, a3, "FsmTransition", "to")
            : NULL;
        if (f == NULL || from == NULL || inp == NULL || to == NULL) {
            free(f);
            free(from);
            free(inp);
            free(to);
            return NULL;
        }
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_fsm_add_transition(&%s, %s, %s, %s)", f, from, inp, to);
        free(f); free(from); free(inp); free(to); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_STEP) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "FsmStep", "FSM"))
            return NULL;
        char *f = transpiler_misc_emit_arg(ctx, a0, "FsmStep", "fsm");
        char *i = f != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "FsmStep", "input")
            : NULL;
        if (f == NULL || i == NULL) {
            free(f);
            free(i);
            return NULL;
        }
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_fsm_step(&%s, %s)", f, i);
        free(f); free(i); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_CURRENT) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "FsmCurrent", "FSM"))
            return NULL;
        char *f = transpiler_misc_emit_arg(ctx, a0, "FsmCurrent", "fsm");
        if (f == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_fsm_current(&%s)", f);
        free(f); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_CURRENT_NAME) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "FsmCurrentName", "FSM"))
            return NULL;
        char *f = transpiler_misc_emit_arg(ctx, a0, "FsmCurrentName", "fsm");
        if (f == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_fsm_current_name(&%s)", f);
        free(f); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_NEW) {
        char *d = transpiler_misc_emit_arg(ctx, a0, "TimerNew", "duration");
        if (d == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_timer_new(%s)", d);
        free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_TICK) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "TimerTick", "Timer"))
            return NULL;
        char *t = transpiler_misc_emit_arg(ctx, a0, "TimerTick", "timer");
        char *d = t != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "TimerTick", "delta")
            : NULL;
        if (t == NULL || d == NULL) {
            free(t);
            free(d);
            return NULL;
        }
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_timer_tick(&%s, %s)", t, d);
        free(t); free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_REMAINING) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "TimerRemaining", "Timer"))
            return NULL;
        char *t = transpiler_misc_emit_arg(ctx, a0, "TimerRemaining", "timer");
        if (t == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_timer_remaining(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_DONE) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "TimerDone", "Timer"))
            return NULL;
        char *t = transpiler_misc_emit_arg(ctx, a0, "TimerDone", "timer");
        if (t == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_timer_done(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_RESET) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "TimerReset", "Timer"))
            return NULL;
        char *t = transpiler_misc_emit_arg(ctx, a0, "TimerReset", "timer");
        if (t == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_timer_reset(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_NEW) {
        char *c = transpiler_misc_emit_arg(ctx, a0, "CooldownNew", "duration");
        if (c == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_cooldown_new(%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_TICK) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "CooldownTick", "Cooldown"))
            return NULL;
        char *c = transpiler_misc_emit_arg(ctx, a0, "CooldownTick",
            "cooldown");
        char *d = c != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "CooldownTick", "delta")
            : NULL;
        if (c == NULL || d == NULL) {
            free(c);
            free(d);
            return NULL;
        }
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_cooldown_tick(&%s, %s)", c, d);
        free(c); free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_READY) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "CooldownReady", "Cooldown"))
            return NULL;
        char *c = transpiler_misc_emit_arg(ctx, a0, "CooldownReady",
            "cooldown");
        if (c == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_cooldown_ready(&%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_TRIGGER) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "CooldownTrigger", "Cooldown"))
            return NULL;
        char *c = transpiler_misc_emit_arg(ctx, a0, "CooldownTrigger",
            "cooldown");
        if (c == NULL)
            return NULL;
        char *r = transpiler_misc_strdup_fmt(ctx, "pgy_cooldown_trigger(&%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_MAP_SET_STR) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "MapSetStr", "HashMap"))
            return NULL;
        char *m = transpiler_misc_emit_arg(ctx, a0, "MapSetStr", "map");
        char *k = m != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "MapSetStr", "key")
            : NULL;
        char *v = k != NULL
            ? transpiler_misc_emit_arg(ctx, a2, "MapSetStr", "value")
            : NULL;
        if (m == NULL || k == NULL || v == NULL) {
            free(m);
            free(k);
            free(v);
            return NULL;
        }
        char *result = transpiler_misc_strdup_fmt(ctx, "pgy_map_set_string(&%s, %s, %s)", m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (op == TRANSPILER_MISC_OP_MAP_GET_STR) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "MapGetStr", "HashMap"))
            return NULL;
        char *m = transpiler_misc_emit_arg(ctx, a0, "MapGetStr", "map");
        char *k = m != NULL
            ? transpiler_misc_emit_arg(ctx, a1, "MapGetStr", "key")
            : NULL;
        if (m == NULL || k == NULL) {
            free(m);
            free(k);
            return NULL;
        }
        char *result = transpiler_misc_strdup_fmt(ctx, "pgy_map_get_string(&%s, %s)", m, k);
        free(m); free(k);
        return result;
    }
    return NULL;
}
