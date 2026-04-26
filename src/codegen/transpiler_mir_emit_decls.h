static int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                            const char *label);
static bool transpiler_is_implicit_field(TranspilerCtx *ctx,
                                         const char *base_name);
static bool transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                              const ASTNode *expr,
                                              const TranspilerSSANameMap *ssa_map,
                                              const char *routine_name,
                                              char *reason,
                                              size_t reason_cap);
static bool transpiler_emit_mir_phi_copies(CodeBuf *buf,
                                           TranspilerCtx *ctx,
                                           int indent,
                                           size_t pred_block_index,
                                           const MIRBasicBlock *pred_block,
                                           const MIRBasicBlock *target_block);
static const char *transpiler_find_prior_block_ssa_name(const MIRBasicBlock *block,
                                                        size_t limit_inst_index,
                                                        const char *base_name);
static const char *transpiler_find_block_exit_ssa_name(const MIRBasicBlock *block,
                                                       const char *base_name);
static bool transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                                     size_t inst_index,
                                                     const ASTNode *expr,
                                                     TranspilerSSANameMap *ssa_map_out);
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
static bool transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                              CodeBuf *out,
                                              int indent,
                                              const MIRInstruction *inst,
                                              const char *handle_expr,
                                              bool cleanup_hook);
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

static ASTNode *
transpiler_find_let_decl_by_name_in_block(ASTNode *body, const char *name)
{
    if (body == NULL || name == NULL)
        return NULL;

    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_let_decl_by_name_in_block(
                body->data.block.statements[i], name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }

    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, name) == 0) {
        return body;
    }

    if (body->type == AST_WITH_STMT)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.with_stmt.body, name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.then_branch, name);
        if (found != NULL)
            return found;
        return transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.else_branch, name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.while_loop.body, name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.for_loop.body, name);

    return NULL;
}

static ASTNode *
transpiler_find_let_decl_by_name(const ASTNode *func_decl, const char *name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    return transpiler_find_let_decl_by_name_in_block(
        func_decl->data.func_decl.body, name);
}
