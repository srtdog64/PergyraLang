#ifndef PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H
#define PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H

#include "transpiler_format.h"

/* transpiler_helpers_core_a split into sub-1000 LOC include chunks.
 * Keep this shim for the existing include order. */
static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
char *render_type_name(ASTNode *type_node);
char *render_type_name_in_ctx(TranspilerCtx *ctx, ASTNode *type_node);
void append_mangled_type_name(CodeBuf *buf, const char *type_name);
bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                               ASTNode *type_node);
bool transpiler_can_forward_declare_func_early(TranspilerCtx *ctx,
                                               ASTNode *func);
bool transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                     ASTNode *func);
static bool resolve_world_zone_subject_receiver(TranspilerCtx *ctx,
                                                ASTNode *receiver,
                                                const char **zone_slot_name_out,
                                                const char **zone_type_name_out,
                                                const char **slot_name_out,
                                                const char **type_name_out);
bool transpiler_mir_intent_has_stmt(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name,
                                    const char *arg0);
bool transpiler_emit_mir_resource_op(TranspilerCtx *ctx,
                                     CodeBuf *out,
                                     int indent,
                                     const MIRInstruction *inst,
                                     const MIRTypeLayout *layout,
                                     const char *ssa_result_name);
const char *transpiler_contextual_option_type_name(TranspilerCtx *ctx);
bool transpiler_contextual_option_inner_type_copy(TranspilerCtx *ctx,
                                                  char *out,
                                                  size_t out_size);
char *transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site);
TranspilerCtx *transpiler_type_render_ctx_current(void);
void transpiler_type_render_ctx_bind(TranspilerCtx *ctx);
TranspilerCtx *transpiler_type_render_ctx_push(TranspilerCtx *ctx);
void transpiler_type_render_ctx_restore(TranspilerCtx *saved);

#include "transpiler_overlay_projection.h"
#include "transpiler_projection_sync_helpers.h"
#endif /* PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H */
