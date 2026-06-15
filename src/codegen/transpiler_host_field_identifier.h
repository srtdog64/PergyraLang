#ifndef PERGYRA_TRANSPILER_HOST_FIELD_IDENTIFIER_H
#define PERGYRA_TRANSPILER_HOST_FIELD_IDENTIFIER_H

#include "transpiler.h"

bool transpiler_current_function_has_self_receiver(const TranspilerCtx *ctx);

bool transpiler_identifier_is_current_true_local(TranspilerCtx *ctx,
                                                const char *id_name);

bool transpiler_identifier_is_stale_host_field_snapshot(TranspilerCtx *ctx,
                                                        const char *id_name);

char *transpiler_emit_current_host_field_identifier(TranspilerCtx *ctx,
                                                   const char *id_name);

#endif /* PERGYRA_TRANSPILER_HOST_FIELD_IDENTIFIER_H */
