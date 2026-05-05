#ifndef PGY_MIR_DECL_HEADERS_H
#define PGY_MIR_DECL_HEADERS_H

#include "mir.h"

bool mir_record_decl_header(MIRProgram *mir, ASTNode *decl);
void mir_link_decl_method_routines(MIRProgram *mir);

#endif /* PGY_MIR_DECL_HEADERS_H */
