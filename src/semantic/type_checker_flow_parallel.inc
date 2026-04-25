bool
type_check_defer_body_flow(ASTNode *body, SemanticContext *ctx)
{
    if (body != NULL) {
        ResourceConsumeSnapshot before_defer = snapshot_resource_states(ctx);
        (void)type_check_block_flow(body, ctx, NULL);
        restore_resource_states(&before_defer);
        destroy_resource_snapshot(&before_defer);
    }
    return ctx == NULL || !ctx->has_error;
}

bool
type_check_parallel_block_flow(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel;
    ResourceConsumeSnapshot base = {0};
    ResourceConsumeSnapshot joined = {0};
    bool has_joined = false;

    if (node == NULL || ctx == NULL || node->type != AST_PARALLEL_BLOCK)
        return ctx == NULL || !ctx->has_error;

    prev_parallel = ctx->in_parallel;
    ctx->in_parallel = true;
    base = snapshot_resource_states(ctx);

    for (size_t i = 0; i < node->data.parallel.task_count; i++) {
        ResourceConsumeSnapshot task_snap = {0};
        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        (void)type_check_statement_flow(node->data.parallel.tasks[i], ctx, NULL);
        task_snap = snapshot_resource_states(ctx);
        scope_exit(&ctx->scope);
        if (has_joined) {
            const Symbol *conflict = NULL;
            if (resource_snapshot_has_parallel_conflict(&base, &joined,
                    &task_snap, &conflict)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    node->data.parallel.tasks[i],
                    "Parallel tasks cannot consume the same resource/boundary '%s'.\n"
                    "Reason:\n"
                    "- each parallel task is checked from the same entry ownership snapshot\n"
                    "- more than one task moves or releases the same ownership-bearing value\n"
                    "- the compiler cannot prove a single owner after the parallel join\n"
                    "Fix:\n"
                    "- move the value before entering parallel and pass disjoint handles\n"
                    "- or serialize the ownership-consuming work outside the parallel block",
                    conflict != NULL && conflict->name != NULL
                        ? conflict->name
                        : "<resource>");
            }
        }
        merge_resource_snapshots_or(&joined, &has_joined, &task_snap);
        destroy_resource_snapshot(&task_snap);
    }

    if (has_joined)
        restore_resource_states(&joined);
    else
        restore_resource_states(&base);

    ctx->in_parallel = prev_parallel;
    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&joined);
    return !ctx->has_error;
}
