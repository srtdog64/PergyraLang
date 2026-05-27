#ifndef PGY_TRANSPILER_MIR_EMIT_STATE_H
#define PGY_TRANSPILER_MIR_EMIT_STATE_H

#include "transpiler.h"

typedef struct TranspilerMirEmitState {
    int slot_count;
    int typed_count;
    ASTNode *host_decl;
    CodeBuf *out;
    const ASTNode *func_decl;
    char return_type[128];
} TranspilerMirEmitState;

void transpiler_capture_mir_emit_state_local(TranspilerCtx *ctx,
                                             TranspilerMirEmitState *state);
void transpiler_restore_mir_emit_state_from_snapshot_local(
    TranspilerCtx *ctx, const TranspilerMirEmitState *state);
void transpiler_emit_host_method_body_local(TranspilerCtx *ctx,
                                           ASTNode *host_decl,
                                           const char *self_type_name,
                                           ASTNode *method,
                                           CodeBuf *body_out,
                                           bool mark_subject_ref_params);
void transpiler_bind_function_emit_host_local(TranspilerCtx *ctx,
                                             ASTNode *host_decl,
                                             const ASTNode *func_decl);
void transpiler_set_current_return_type_local(TranspilerCtx *ctx,
                                             const char *type_name);
void transpiler_restore_local_binding_counts_local(TranspilerCtx *ctx,
                                                  int saved_slot_count,
                                                  int saved_typed_count,
                                                  int saved_alias_count);

#endif /* PGY_TRANSPILER_MIR_EMIT_STATE_H */
