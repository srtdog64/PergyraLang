static void
emit_intent_step_condition_failure(CodeBuf *out,
                                   TranspilerCtx *ctx,
                                   const char *condition_expr,
                                   const char *phase,
                                   const char *step_name,
                                   const char *intent_name,
                                   bool emit_cleanup_from_mir,
                                   size_t cleanup_block)
{
    if (out == NULL || ctx == NULL || phase == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "if (!(%s)) { ",
        condition_expr != NULL ? condition_expr : "false");
    codebuf_write(out, "__intent_failed = true; ");
    codebuf_write(out,
        "pgy_intent_trace_fail_export(__intent_handle, \"%s:%s\"); __intent_result = false; ",
        phase,
        step_name != NULL ? step_name : "<step>");
    if (emit_cleanup_from_mir) {
        codebuf_write(out, "goto _pgy_mir_bb_%s_%zu; }\n",
            intent_name != NULL ? intent_name : "intent",
            cleanup_block);
    } else {
        codebuf_write(out, "goto __intent_cleanup; }\n");
    }
}
