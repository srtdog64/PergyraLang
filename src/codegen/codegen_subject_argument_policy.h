#ifndef PGY_CODEGEN_SUBJECT_ARGUMENT_POLICY_H
#define PGY_CODEGEN_SUBJECT_ARGUMENT_POLICY_H

#include "../compiler/mir.h"

/* A constructor call owns fresh subject storage through the consuming call.
 * An arbitrary function returning the same nominal type does not prove this. */
bool pgy_codegen_is_subject_constructor(const MIRProgram *program, ASTNode *arg);

#endif
