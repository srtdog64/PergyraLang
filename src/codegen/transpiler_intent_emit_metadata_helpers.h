#ifndef PGY_SRC_CODEGEN_TRANSPILER_INTENT_EMIT_METADATA_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_INTENT_EMIT_METADATA_HELPERS_H

#include "transpiler.h"

void transpiler_free_intent_emit_metadata(ASTNode **mir_steps,
                                          const char **participant_aliases,
                                          const char **participant_types,
                                          const char **mir_step_names);

ASTNode *transpiler_find_intent_step_source_by_name(ASTNode *intent,
                                                    const char *step_name);

ASTNode **transpiler_build_mir_intent_step_sources(ASTNode *intent,
                                                   const char **step_names,
                                                   size_t step_count);

#define PGY_MIR_INTENT_CARRIER_FAIL(MSG) \
    do { \
        transpiler_set_backend_error_with_hints(ctx, \
            PGY_CODE_MIR_INTENT_CARRIER_MISSING, \
            PGY_CAUSE_MIR_INTENT_CARRIER_MISSING, \
            PGY_FIX_CHECK_INTENT_STEP_LOWERING, \
            (MSG)); \
        goto intent_emit_fail; \
    } while (0)
#define PGY_BIND_INTENT_STEP_CONTEXT(STEP_ZONE_DECL, STEP_ZONE_ALIAS) \
    do { \
        if ((STEP_ZONE_DECL) != NULL) \
            transpiler_bind_current_host_decl_local(ctx, (STEP_ZONE_DECL)); \
        if ((STEP_ZONE_ALIAS) != NULL) \
            ctx->current_overlay_receiver_expr = (STEP_ZONE_ALIAS); \
    } while (0)
#define PGY_RESTORE_INTENT_STEP_CONTEXT(SAVED_HOST_DECL, SAVED_OVERLAY_RECEIVER) \
    do { \
        transpiler_bind_current_host_decl_local(ctx, (SAVED_HOST_DECL)); \
        ctx->current_overlay_receiver_expr = (SAVED_OVERLAY_RECEIVER); \
    } while (0)
#endif /* PGY_SRC_CODEGEN_TRANSPILER_INTENT_EMIT_METADATA_HELPERS_H */
