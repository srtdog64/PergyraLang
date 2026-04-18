static uint32_t
effect_delta_from_baseline(uint32_t baseline, uint32_t after)
{
    uint32_t closed_baseline = type_effect_mask_closure(baseline);
    uint32_t closed_after = type_effect_mask_closure(after);
    return closed_after & ~closed_baseline;
}

static void
flow_effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size)
{
    size_t off = 0;
    uint32_t closed = type_effect_mask_closure(mask);

    if (buf == NULL || buf_size == 0)
        return;
    if (closed == EFFECT_NONE) {
        snprintf(buf, buf_size, "local");
        return;
    }

    if (type_effect_mask_has(closed, EFFECT_SECURE))
        off += (size_t)snprintf(buf + off, buf_size > off ? buf_size - off : 0,
                                "%ssecure", off > 0 ? ", " : "");
    if (type_effect_mask_has(closed, EFFECT_REMOTE))
        off += (size_t)snprintf(buf + off, buf_size > off ? buf_size - off : 0,
                                "%sremote", off > 0 ? ", " : "");
    if (type_effect_mask_has(closed, EFFECT_NONDETERMINISTIC))
        off += (size_t)snprintf(buf + off, buf_size > off ? buf_size - off : 0,
                                "%snondeterministic", off > 0 ? ", " : "");
    if (type_effect_mask_has(closed, EFFECT_COLLAPSE))
        off += (size_t)snprintf(buf + off, buf_size > off ? buf_size - off : 0,
                                "%scollapse", off > 0 ? ", " : "");
}

static void
flow_record_branch_effect_conflict_labeled(SemanticContext *ctx,
                                           const ASTNode *node,
                                           uint32_t left_delta,
                                           const char *left_label,
                                           uint32_t right_delta,
                                           const char *right_label)
{
    char left_buf[128];
    char right_buf[128];
    const char *lhs = left_label != NULL ? left_label : "left path";
    const char *rhs = right_label != NULL ? right_label : "right path";

    if (ctx == NULL || node == NULL)
        return;
    if (!type_effect_mask_conflicts(left_delta, right_delta))
        return;

    flow_effect_mask_to_string(left_delta, left_buf, sizeof(left_buf));
    flow_effect_mask_to_string(right_delta, right_buf, sizeof(right_buf));
    semantic_warning(ctx, node,
        "Control-flow branch/join combines conflicting effect classes (%s vs %s).\n"
        "Reason:\n"
        "- this control-flow join merges effect deltas from multiple paths\n"
        "- %s contributes '%s'\n"
        "- %s contributes '%s'\n"
        "- authority-sensitive and remote/resource-boundary work currently converge at the same join edge\n"
        "Fix:\n"
        "- split secure/remote work into separate helper routines or branches\n"
        "- or narrow the branch effects before they rejoin",
        left_buf, right_buf, lhs, left_buf, rhs, right_buf);
}

static void
flow_record_branch_effect_conflict(SemanticContext *ctx, const ASTNode *node,
                                   uint32_t left_delta, uint32_t right_delta)
{
    flow_record_branch_effect_conflict_labeled(
        ctx, node, left_delta, "left path", right_delta, "right path");
}

static void
flow_merge_effect_delta(SemanticContext *ctx, const ASTNode *node,
                        uint32_t *merged_delta,
                        uint32_t *previous_delta,
                        bool *have_previous_delta,
                        uint32_t current_delta)
{
    if (merged_delta == NULL || previous_delta == NULL
        || have_previous_delta == NULL)
        return;

    if (*merged_delta != EFFECT_NONE) {
        flow_record_branch_effect_conflict(ctx, node, *merged_delta, current_delta);
    } else if (*have_previous_delta) {
        flow_record_branch_effect_conflict(ctx, node, *previous_delta, current_delta);
    }

    *merged_delta = type_effect_mask_join(*merged_delta, current_delta);
    *previous_delta = current_delta;
    *have_previous_delta = true;
}
