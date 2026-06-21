#ifndef PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_MIR_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_MIR_EMIT_H

#include <stdbool.h>

#include "../compiler/mir_decl.h"
#include "transpiler_context.h"

bool transpiler_emit_mir_ability_ref_vtable_decl(
    CodeBuf *target,
    TranspilerCtx *ctx,
    const MIRDeclHeader *ability_header,
    const MIRAbilityRef *ability_ref,
    const char *ability_name,
    const char *typedef_name);

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_MIR_EMIT_H */
