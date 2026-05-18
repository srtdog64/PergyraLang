#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_EMIT_DECLS_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_EMIT_DECLS_H

/* MIR SSA lookup/entry helpers are compiled owners; expose the seams early. */
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_signature.h"

static int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                            const char *label);
static bool transpiler_emit_mir_block_statements(CodeBuf *buf,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine *mir_routine,
                                                 const MIRBasicBlock *block,
                                                 TranspilerCtx *ctx,
                                                 TranspilerSSANameMap *out_ssa_map,
                                                 char *reason,
                                                 size_t reason_cap);
static bool transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                                  const ASTNode *func_decl,
                                                  const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                             const ASTNode *func_decl,
                                                             const MIRRoutine **mir_routine_out,
                                                             char *reason,
                                                             size_t reason_cap);
static bool transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                                        const ASTNode *intent_decl,
                                                        const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                                  const ASTNode *intent_decl,
                                                                  const MIRRoutine **mir_routine_out,
                                                                  char *reason,
                                                                  size_t reason_cap);
static bool transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                                        const char *routine_name,
                                                        bool require_cleanup,
                                                        char *reason,
                                                        size_t reason_cap);
static void emit_func_decl_from_mir_named(ASTNode *node,
                                          const MIRRoutine *mir_routine,
                                          const char *emitted_name,
                                          CodeBuf *buf,
                                          TranspilerCtx *ctx);
static char *transpiler_render_effective_local_type_name(TranspilerCtx *ctx,
                                                         ASTNode *type_node);
/* Forward declarations for generic class monomorphization */
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

#include "transpiler_mir_inventory_ssa_emitters.h"
#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_EMIT_DECLS_H */
