#ifndef PERGYRA_COMPILER_RIR_RESOURCE_FLOW_SYMBOLS_H
#define PERGYRA_COMPILER_RIR_RESOURCE_FLOW_SYMBOLS_H

#include "hir.h"
#include "rir.h"

bool rir_resource_kind_has_semantic_flow_identity(RIRResourceKind kind);

bool rir_copy_resource_flow_symbols(RIRScope *scope,
                                    const HIRRoutine *hir_routine,
                                    char **error_message);

#endif
