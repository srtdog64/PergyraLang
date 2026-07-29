/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend hosted-method forward declaration emission.
 */

#ifndef PERGYRA_TRANSPILER_FUNC_FORWARD_METADATA_H
#define PERGYRA_TRANSPILER_FUNC_FORWARD_METADATA_H

#include "transpiler.h"

void emit_hosted_method_forward_decl_from_metadata(const char *host_name,
                                                   const MIRDeclMethod *method_meta,
                                                   const GenericBindingEntry *bindings,
                                                   size_t binding_count,
                                                   ASTNode *method,
                                                   bool pointer_self,
                                                   CodeBuf *buf,
                                                   TranspilerCtx *ctx);
void emit_hosted_method_forward_decl_from_metadata_named(
    const char *host_name,
    const char *emitted_name,
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool pointer_self,
    CodeBuf *buf,
    TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_FUNC_FORWARD_METADATA_H */
