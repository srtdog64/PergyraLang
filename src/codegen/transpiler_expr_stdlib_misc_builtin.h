#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H

/* C backend misc stdlib call lowering owner. */
static char *
emit_call_stdlib_misc_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    if (strcmp(fn, "FsmNew") == 0)
        return pergyra_strdup("pgy_fsm_new()");
    if (strcmp(fn, "FsmAddState") == 0 && call->data.call.arg_count == 2) {
        char *f = emit_expression(call->data.call.arguments[0], ctx);
        char *n = emit_expression(call->data.call.arguments[1], ctx);
        char *r = strdup_fmt("pgy_fsm_add_state(&%s, %s)", f, n);
        free(f); free(n); return r;
    }
    if (strcmp(fn, "FsmTransition") == 0 && call->data.call.arg_count == 4) {
        char *f = emit_expression(call->data.call.arguments[0], ctx);
        char *from = emit_expression(call->data.call.arguments[1], ctx);
        char *inp = emit_expression(call->data.call.arguments[2], ctx);
        char *to = emit_expression(call->data.call.arguments[3], ctx);
        char *r = strdup_fmt("pgy_fsm_add_transition(&%s, %s, %s, %s)", f, from, inp, to);
        free(f); free(from); free(inp); free(to); return r;
    }
    if (strcmp(fn, "FsmStep") == 0 && call->data.call.arg_count == 2) {
        char *f = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        char *r = strdup_fmt("pgy_fsm_step(&%s, %s)", f, i);
        free(f); free(i); return r;
    }
    if (strcmp(fn, "FsmCurrent") == 0 && call->data.call.arg_count == 1) {
        char *f = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_fsm_current(&%s)", f);
        free(f); return r;
    }
    if (strcmp(fn, "FsmCurrentName") == 0 && call->data.call.arg_count == 1) {
        char *f = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_fsm_current_name(&%s)", f);
        free(f); return r;
    }
    if (strcmp(fn, "TimerNew") == 0 && call->data.call.arg_count == 1) {
        char *d = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_timer_new(%s)", d);
        free(d); return r;
    }
    if (strcmp(fn, "TimerTick") == 0 && call->data.call.arg_count == 2) {
        char *t = emit_expression(call->data.call.arguments[0], ctx);
        char *d = emit_expression(call->data.call.arguments[1], ctx);
        char *r = strdup_fmt("pgy_timer_tick(&%s, %s)", t, d);
        free(t); free(d); return r;
    }
    if (strcmp(fn, "TimerRemaining") == 0 && call->data.call.arg_count == 1) {
        char *t = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_timer_remaining(&%s)", t);
        free(t); return r;
    }
    if (strcmp(fn, "TimerDone") == 0 && call->data.call.arg_count == 1) {
        char *t = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_timer_done(&%s)", t);
        free(t); return r;
    }
    if (strcmp(fn, "TimerReset") == 0 && call->data.call.arg_count == 1) {
        char *t = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_timer_reset(&%s)", t);
        free(t); return r;
    }
    if (strcmp(fn, "CooldownNew") == 0 && call->data.call.arg_count == 1) {
        char *c = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_cooldown_new(%s)", c);
        free(c); return r;
    }
    if (strcmp(fn, "CooldownTick") == 0 && call->data.call.arg_count == 2) {
        char *c = emit_expression(call->data.call.arguments[0], ctx);
        char *d = emit_expression(call->data.call.arguments[1], ctx);
        char *r = strdup_fmt("pgy_cooldown_tick(&%s, %s)", c, d);
        free(c); free(d); return r;
    }
    if (strcmp(fn, "CooldownReady") == 0 && call->data.call.arg_count == 1) {
        char *c = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_cooldown_ready(&%s)", c);
        free(c); return r;
    }
    if (strcmp(fn, "CooldownTrigger") == 0 && call->data.call.arg_count == 1) {
        char *c = emit_expression(call->data.call.arguments[0], ctx);
        char *r = strdup_fmt("pgy_cooldown_trigger(&%s)", c);
        free(c); return r;
    }
    if (strcmp(fn, "MapSetStr") == 0 && call->data.call.arg_count == 3) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        char *v = emit_expression(call->data.call.arguments[2], ctx);
        char *result = strdup_fmt("pgy_map_set_string(&%s, %s, %s)", m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (strcmp(fn, "MapGetStr") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_map_get_string(&%s, %s)", m, k);
        free(m); free(k);
        return result;
    }
    return NULL;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H */
