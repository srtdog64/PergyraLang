#include <string.h>

#include "type_checker_flow_internal.h"
#include "diag_codes.h"

static bool
flow_validate_loop_control(SemanticContext *ctx, ASTNode *node,
                           const char *kind, const char *label)
{
    if (ctx->loop_depth <= 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node,
            "'%s' used outside of loop", kind);
        return false;
    }

    if (label != NULL) {
        for (int i = ctx->loop_depth - 1; i >= 0; i--) {
            if (ctx->loop_labels[i] != NULL
                && strcmp(ctx->loop_labels[i], label) == 0) {
                return true;
            }
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node,
            "Unknown loop label '%s' in %s", label, kind);
        return false;
    }

    return true;
}

FlowFlags
type_check_loop_control_flow(ASTNode *node, SemanticContext *ctx,
                             LoopFlowState *loop_flow, bool is_break)
{
    const char *kind = is_break ? "break" : "continue";
    const char *label = is_break
        ? ast_break_label(node)
        : ast_continue_label(node);
    ResourceConsumeSnapshot snap;

    if (!flow_validate_loop_control(ctx, node, kind, label))
        return FLOW_NONE;

    snap = snapshot_resource_states_from_scope(
        loop_flow != NULL && loop_flow->loop_scope != NULL
            ? loop_flow->loop_scope
            : ctx->scope,
        ctx);
    loop_flow_record(loop_flow, is_break, &snap);
    destroy_resource_snapshot(&snap);

    return is_break ? FLOW_BREAK : FLOW_CONTINUE;
}
