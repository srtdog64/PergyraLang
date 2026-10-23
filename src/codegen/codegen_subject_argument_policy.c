#include "codegen_subject_argument_policy.h"

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

bool
pgy_codegen_is_subject_constructor(const MIRProgram *program, ASTNode *arg)
{
    if (arg == NULL || arg->type != AST_CALL
        || ast_call_semantic_callee_value_binding_id(arg) != 0)
        return false;
    ASTNode *callee = ast_call_callee(arg);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    const MIRDeclHeader *header = mir_find_decl_header(
        program, ast_identifier_name(callee));
    uint32_t target = ast_call_semantic_callee_decl_id(arg);
    return header != NULL
        && mir_decl_header_nominal_kind_or(header, NOMINAL_DECL_CLASS)
            == NOMINAL_DECL_SUBJECT
        && (target == 0 || target == mir_decl_header_source_syntax_id(header));
}
