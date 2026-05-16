#include "transpiler_expr_stdlib_misc_builtin.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static char *
transpiler_misc_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    char *s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

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
        char *f = emit_expression(a0, ctx);
        char *n = emit_expression(a1, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_fsm_add_state(&%s, %s)", f, n);
        free(f); free(n); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_TRANSITION) {
        char *f = emit_expression(a0, ctx);
        char *from = emit_expression(a1, ctx);
        char *inp = emit_expression(a2, ctx);
        char *to = emit_expression(a3, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_fsm_add_transition(&%s, %s, %s, %s)", f, from, inp, to);
        free(f); free(from); free(inp); free(to); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_STEP) {
        char *f = emit_expression(a0, ctx);
        char *i = emit_expression(a1, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_fsm_step(&%s, %s)", f, i);
        free(f); free(i); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_CURRENT) {
        char *f = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_fsm_current(&%s)", f);
        free(f); return r;
    }
    if (op == TRANSPILER_MISC_OP_FSM_CURRENT_NAME) {
        char *f = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_fsm_current_name(&%s)", f);
        free(f); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_NEW) {
        char *d = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_timer_new(%s)", d);
        free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_TICK) {
        char *t = emit_expression(a0, ctx);
        char *d = emit_expression(a1, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_timer_tick(&%s, %s)", t, d);
        free(t); free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_REMAINING) {
        char *t = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_timer_remaining(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_DONE) {
        char *t = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_timer_done(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_TIMER_RESET) {
        char *t = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_timer_reset(&%s)", t);
        free(t); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_NEW) {
        char *c = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_cooldown_new(%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_TICK) {
        char *c = emit_expression(a0, ctx);
        char *d = emit_expression(a1, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_cooldown_tick(&%s, %s)", c, d);
        free(c); free(d); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_READY) {
        char *c = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_cooldown_ready(&%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_COOLDOWN_TRIGGER) {
        char *c = emit_expression(a0, ctx);
        char *r = transpiler_misc_strdup_fmt("pgy_cooldown_trigger(&%s)", c);
        free(c); return r;
    }
    if (op == TRANSPILER_MISC_OP_MAP_SET_STR) {
        char *m = emit_expression(a0, ctx);
        char *k = emit_expression(a1, ctx);
        char *v = emit_expression(a2, ctx);
        char *result = transpiler_misc_strdup_fmt("pgy_map_set_string(&%s, %s, %s)", m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (op == TRANSPILER_MISC_OP_MAP_GET_STR) {
        char *m = emit_expression(a0, ctx);
        char *k = emit_expression(a1, ctx);
        char *result = transpiler_misc_strdup_fmt("pgy_map_get_string(&%s, %s)", m, k);
        free(m); free(k);
        return result;
    }
    return NULL;
}
