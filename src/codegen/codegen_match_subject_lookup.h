#ifndef PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H
#define PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H

#include "../parser/ast_api.h"

/*
 * Compatibility owner for MIR match-case subject lookup.
 *
 * MIR branch instructions currently carry the case node as their source
 * payload, but not the owning match subject. Keep the fallback body walk behind
 * one codegen seam so C/LLVM backends do not each own the AST rescan.
 */
ASTNode *pgy_codegen_match_subject_for_case(ASTNode *func_decl,
                                            ASTNode *case_node);

#endif /* PERGYRA_CODEGEN_MATCH_SUBJECT_LOOKUP_H */
