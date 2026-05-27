#ifndef PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H
#define PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H

#include "transpiler_domain_receiver_query.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_mir_intent_query.h"
#include "codegen_slot_type_policy.h"

/* transpiler_helpers_core_a split into sub-1000 LOC include chunks.
 * Keep this shim for the existing include order. */
char *render_type_name(ASTNode *type_node);
char *render_type_name_in_ctx(TranspilerCtx *ctx, ASTNode *type_node);
bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                               ASTNode *type_node);
bool transpiler_can_forward_declare_func_early(TranspilerCtx *ctx,
                                               ASTNode *func);
bool transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                     ASTNode *func);
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

#include "transpiler_overlay_projection.h"
#include "transpiler_projection_method_invalidation.h"
#include "transpiler_projection_sync.h"
#endif /* PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_A_H */
