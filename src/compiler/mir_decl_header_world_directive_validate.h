#ifndef PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_VALIDATE_H
#define PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_VALIDATE_H

#include "mir_decl.h"

bool mir_decl_header_validate_world_directives(
    const MIRDeclHeader *header,
    size_t header_index,
    char **error_message);

#endif /* PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_VALIDATE_H */
