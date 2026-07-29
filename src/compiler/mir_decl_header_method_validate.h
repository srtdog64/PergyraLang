#ifndef PGY_MIR_DECL_HEADER_METHOD_VALIDATE_H
#define PGY_MIR_DECL_HEADER_METHOD_VALIDATE_H

#include "mir.h"

bool mir_validate_decl_method_metadata(const MIRProgram *mir,
                                       const MIRDeclHeader *header,
                                       size_t header_index,
                                       char **error_message);

#endif /* PGY_MIR_DECL_HEADER_METHOD_VALIDATE_H */
