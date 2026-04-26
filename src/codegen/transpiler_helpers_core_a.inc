/* transpiler_helpers_core_a split into sub-1000 LOC include chunks.
 * Keep this shim for the existing include order. */
static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
static char *escape_c_string_literal(const char *src);
static char *render_type_name(ASTNode *type_node);
static char *render_ability_ref_vtable_tag(ASTNode *ability_ref);
static bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                                      ASTNode *type_node);
static bool transpiler_can_forward_declare_func_early(TranspilerCtx *ctx,
                                                      ASTNode *func);
static bool transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                            ASTNode *func);
static bool resolve_world_zone_subject_receiver(TranspilerCtx *ctx,
                                                ASTNode *receiver,
                                                const char **zone_slot_name_out,
                                                const char **zone_type_name_out,
                                                const char **slot_name_out,
                                                const char **type_name_out);
static TranspilerCtx *g_type_render_ctx = NULL;
static char *strdup_fmt(const char *fmt, ...);

#include "transpiler_overlay_projection.h"
#include "transpiler_helpers_core_a_part_c.inc"
