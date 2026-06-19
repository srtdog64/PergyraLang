#ifndef PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H
#define PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H

#include "../compiler/mir.h"
#include "../parser/ast_api.h"

/*
 * Owner for MIR match-case subject lookup.
 *
 * Normal MIR branch instructions carry the owning match subject in expr0.
 * Backend MIR consumers must call this branch API so the subject comes from
 * MIR branch facts, not from source payload shape or a function-body rescan.
 * The MIR terminator validator owns the missing-expr0 fail-closed check.
 */
ASTNode *pgy_codegen_match_subject_for_branch(const MIRInstruction *inst);

#endif /* PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H */
