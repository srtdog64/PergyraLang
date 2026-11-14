#ifndef PERGYRA_COMPILER_MIR_DECL_HEADER_ROLE_VALIDATE_H
#define PERGYRA_COMPILER_MIR_DECL_HEADER_ROLE_VALIDATE_H

#include "mir_decl_headers.h"

bool mir_validate_decl_role_impl_metadata(const MIRDeclHeader *header,
                                          size_t header_index,
                                          char **error_message);
bool mir_validate_decl_role_include_metadata(const MIRDeclHeader *header,
                                             size_t header_index,
                                             char **error_message);

#endif /* PERGYRA_COMPILER_MIR_DECL_HEADER_ROLE_VALIDATE_H */
