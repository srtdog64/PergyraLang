#ifndef PGY_TRANSPILER_SLOT_RUNTIME_ROW_H
#define PGY_TRANSPILER_SLOT_RUNTIME_ROW_H

#include <stdbool.h>

#include "transpiler.h"

const char *transpiler_slot_runtime_expected_call_shape(bool secure,
                                                        const char *operation);

const char *transpiler_slot_runtime_fn(TranspilerCtx *ctx,
                                       bool secure,
                                       const char *inner_type,
                                       const char *operation);
const char *transpiler_slot_runtime_fn_for_decl_claim(
    TranspilerCtx *ctx,
    const MIRDeclFieldClaim *claim);

const MIRResourceRuntimeRow *transpiler_slot_runtime_row_for_operation(
    TranspilerCtx *ctx,
    bool secure,
    const char *inner_type,
    const char *operation);
const MIRResourceRuntimeRow *transpiler_slot_runtime_row_for_source_operation(
    TranspilerCtx *ctx,
    const ASTNode *source_call,
    bool secure,
    const char *inner_type,
    const char *operation);

void transpiler_emit_nominal_container_runtime_rows(CodeBuf *dst,
                                                    const char *type_name,
                                                    bool include_intro_comment);

#endif /* PGY_TRANSPILER_SLOT_RUNTIME_ROW_H */
