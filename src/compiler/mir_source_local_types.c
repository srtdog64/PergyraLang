#include "mir_source_local_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "mir_type_helpers.h"

#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT 8
#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE 128

typedef struct MIRSourceLocalTypeScratch
{
    char buffers[MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT]
                [MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
    size_t next;
} MIRSourceLocalTypeScratch;

static char *
mir_source_local_type_scratch_next(MIRSourceLocalTypeScratch *scratch)
{
    char *buffer;

    if (scratch == NULL)
        return NULL;
    buffer = scratch->buffers[scratch->next
        % MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT];
    scratch->next++;
    buffer[0] = '\0';
    return buffer;
}

static const char *
mir_source_local_type_scratch_format(MIRSourceLocalTypeScratch *scratch,
                                     const char *outer,
                                     const char *inner)
{
    char *buffer = mir_source_local_type_scratch_next(scratch);

    if (buffer == NULL || outer == NULL || inner == NULL || inner[0] == '\0')
        return NULL;
    if (snprintf(buffer, MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE, "%s<%s>",
            outer, inner) >= MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
}

void
mir_routine_source_local_type_names_clear(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    if (routine->source_local_types != NULL) {
        for (size_t i = 0; i < routine->source_local_type_count; i++) {
            free(routine->source_local_types[i].name);
            free(routine->source_local_types[i].type_name);
        }
    }
    free(routine->source_local_types);
    routine->source_local_types = NULL;
    routine->source_local_type_count = 0;
    routine->source_local_type_capacity = 0;
}

static bool
mir_source_local_type_append_name(const MIRProgram *program,
                                  MIRRoutine *routine,
                                  const char *name,
                                  const char *type_name)
{
    const char *effective_type_name = type_name;

    if (routine == NULL || name == NULL || type_name == NULL
        || type_name[0] == '\0') {
        return true;
    }
    if (program != NULL) {
        const char *alias_target =
            mir_decl_header_resolve_type_alias_target_type_name(
                program, type_name);
        if (alias_target != NULL)
            effective_type_name = alias_target;
    }
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        if (routine->source_local_types[i].name != NULL
            && strcmp(routine->source_local_types[i].name, name) == 0) {
            return true;
        }
    }
    if (routine->source_local_type_count
        == routine->source_local_type_capacity) {
        size_t next = routine->source_local_type_capacity == 0
            ? 8
            : routine->source_local_type_capacity * 2;
        if (next < routine->source_local_type_capacity
            || next > SIZE_MAX / sizeof(MIRSourceLocalType)) {
            return false;
        }
        MIRSourceLocalType *grown = realloc(routine->source_local_types,
            next * sizeof(MIRSourceLocalType));
        if (grown == NULL)
            return false;
        routine->source_local_types = grown;
        routine->source_local_type_capacity = next;
    }

    char *type_name_copy = mir_capture_type_name(NULL, effective_type_name);
    if (type_name_copy == NULL)
        return false;
    char *name_copy = pergyra_strdup(name);
    if (name_copy == NULL) {
        free(type_name_copy);
        return false;
    }
    routine->source_local_types[routine->source_local_type_count].name =
        name_copy;
    routine->source_local_types[routine->source_local_type_count].type_name =
        type_name_copy;
    routine->source_local_type_count++;
    return true;
}

static bool
mir_source_local_type_append(const MIRProgram *program,
                             MIRRoutine *routine,
                             const char *name,
                             ASTNode *type_node)
{
    char *rendered;
    bool ok;

    if (routine == NULL || name == NULL || type_node == NULL
        || type_node->type != AST_TYPE) {
        return true;
    }

    rendered = mir_capture_type_name(type_node, NULL);
    if (rendered == NULL)
        return false;
    ok = mir_source_local_type_append_name(program, routine, name, rendered);
    free(rendered);
    return ok;
}

static const MIRDeclHeader *
mir_source_local_decl_header(const MIRProgram *program, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < program->decl_header_count; i++) {
        const MIRDeclHeader *header = &program->decl_headers[i];
        if (header->name != NULL && strcmp(header->name, name) == 0)
            return header;
    }
    return NULL;
}

static bool
mir_source_local_unwrap_slot_like_type(const char *type_name,
                                       char *out,
                                       size_t out_size)
{
    static const char *prefixes[] = {
        "Slot<",
        "SecureSlot<",
        "DeviceSlot<",
        "PinnedSlotView<",
        "PinnedSecureSlotView<",
        "ReadView<",
        "WriteView<",
    };
    const char *open;
    const char *close;
    size_t len;

    if (type_name == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t prefix_len = strlen(prefixes[i]);
        if (strncmp(type_name, prefixes[i], prefix_len) != 0)
            continue;
        open = type_name + prefix_len;
        close = strrchr(open, '>');
        if (close == NULL || close <= open)
            return false;
        len = (size_t)(close - open);
        if (len >= out_size)
            return false;
        memcpy(out, open, len);
        out[len] = '\0';
        return out[0] != '\0';
    }
    return false;
}

static const MIRDeclHeader *
mir_source_local_decl_header_for_value_type(const MIRProgram *program,
                                            const char *type_name)
{
    char unwrapped[128];
    const MIRDeclHeader *header =
        mir_source_local_decl_header(program, type_name);
    if (header != NULL)
        return header;
    if (!mir_source_local_unwrap_slot_like_type(type_name, unwrapped,
            sizeof(unwrapped))) {
        return NULL;
    }
    return mir_source_local_decl_header(program, unwrapped);
}

static const MIRDeclField *
mir_source_local_header_field(const MIRDeclHeader *header, const char *name)
{
    if (header == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < header->field_metadata_count; i++) {
        const MIRDeclField *field = &header->field_metadata[i];
        if (field->name != NULL && strcmp(field->name, name) == 0)
            return field;
    }
    return NULL;
}

static const MIRDeclMethod *
mir_source_local_header_method(const MIRDeclHeader *header, const char *name)
{
    if (header == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        if (method->name != NULL && strcmp(method->name, name) == 0)
            return method;
    }
    return NULL;
}

static const char *
mir_source_local_param_type_name(const MIRRoutine *routine, const char *name)
{
    if (routine == NULL || name == NULL || !routine->has_signature)
        return NULL;
    for (size_t i = 0; i < routine->param_count; i++) {
        FuncParam *param = routine->params != NULL ? routine->params[i] : NULL;
        if (param != NULL && param->name != NULL
            && strcmp(param->name, name) == 0) {
            return routine->param_type_names != NULL
                ? routine->param_type_names[i]
                : NULL;
        }
    }
    return NULL;
}

static const char *
mir_source_local_routine_owner_name(const MIRRoutine *routine)
{
    if (routine == NULL)
        return NULL;
    if (routine->owner_name != NULL)
        return routine->owner_name;
    return routine->hir_routine != NULL ? routine->hir_routine->owner_name
                                        : NULL;
}

static const char *
mir_source_local_owner_field_type_name(const MIRProgram *program,
                                       const MIRRoutine *routine,
                                       const char *name)
{
    const MIRDeclHeader *owner;
    const MIRDeclField *field;
    const char *owner_name = mir_source_local_routine_owner_name(routine);

    if (routine == NULL || name == NULL || owner_name == NULL)
        return NULL;
    owner = mir_source_local_decl_header(program, owner_name);
    field = mir_source_local_header_field(owner, name);
    return field != NULL ? field->type_name : NULL;
}

static const char *
mir_source_local_owner_method_return_type_name(const MIRProgram *program,
                                               const MIRRoutine *routine,
                                               const char *name)
{
    const MIRDeclHeader *owner;
    const MIRDeclMethod *method;
    const char *owner_name = mir_source_local_routine_owner_name(routine);

    if (routine == NULL || name == NULL || owner_name == NULL)
        return NULL;
    owner = mir_source_local_decl_header(program, owner_name);
    method = mir_source_local_header_method(owner, name);
    return method != NULL ? method->return_type_name : NULL;
}

static const char *
mir_source_local_identifier_type_name(const MIRProgram *program,
                                      const MIRRoutine *routine,
                                      const char *name)
{
    const char *type_name;

    type_name = mir_source_local_param_type_name(routine, name);
    if (type_name != NULL)
        return type_name;
    type_name = mir_routine_source_local_type_name(routine, name);
    if (type_name != NULL)
        return type_name;
    return mir_source_local_owner_field_type_name(program, routine, name);
}

static const char *
mir_source_local_expr_type_name(const MIRProgram *program,
                                const MIRRoutine *routine,
                                MIRSourceLocalTypeScratch *scratch,
                                ASTNode *expr);

static bool
mir_source_local_builtin_returns_first_arg_type(const char *callee_name)
{
    return callee_name != NULL
        && (strcmp(callee_name, "Abs") == 0
            || strcmp(callee_name, "Clamp") == 0
            || strcmp(callee_name, "Clone") == 0
            || strcmp(callee_name, "Max") == 0
            || strcmp(callee_name, "Min") == 0);
}

static const char *
mir_source_local_read_call_type_name(const MIRProgram *program,
                                     const MIRRoutine *routine,
                                     MIRSourceLocalTypeScratch *scratch,
                                     ASTNode *expr)
{
    const char *slot_type;
    char *inner;

    if (ast_call_arg_count(expr) < 1)
        return NULL;
    slot_type = mir_source_local_expr_type_name(program, routine, scratch,
        ast_call_argument(expr, 0));
    inner = mir_source_local_type_scratch_next(scratch);
    if (inner == NULL
        || !mir_source_local_unwrap_slot_like_type(slot_type, inner,
            MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE)) {
        return NULL;
    }
    return inner;
}

static const char *
mir_source_local_view_call_type_name(const MIRProgram *program,
                                     const MIRRoutine *routine,
                                     MIRSourceLocalTypeScratch *scratch,
                                     ASTNode *expr,
                                     const char *view_type_name)
{
    const char *slot_type;
    char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];

    if (ast_call_arg_count(expr) < 1)
        return NULL;
    slot_type = mir_source_local_expr_type_name(program, routine, scratch,
        ast_call_argument(expr, 0));
    if (!mir_source_local_unwrap_slot_like_type(slot_type, inner,
            sizeof(inner))) {
        return NULL;
    }
    return mir_source_local_type_scratch_format(scratch, view_type_name,
        inner);
}

static const char *
mir_source_local_builtin_call_type_name(const MIRProgram *program,
                                        const MIRRoutine *routine,
                                        MIRSourceLocalTypeScratch *scratch,
                                        ASTNode *expr,
                                        const char *callee_name)
{
    if (callee_name != NULL
        && (strcmp(callee_name, "Read") == 0
            || strcmp(callee_name, "DeviceRead") == 0)) {
        return mir_source_local_read_call_type_name(program, routine,
            scratch, expr);
    }
    if (callee_name != NULL && strcmp(callee_name, "ViewRead") == 0) {
        return mir_source_local_view_call_type_name(program, routine, scratch,
            expr, "ReadView");
    }
    if (callee_name != NULL && strcmp(callee_name, "ViewWrite") == 0) {
        return mir_source_local_view_call_type_name(program, routine, scratch,
            expr, "WriteView");
    }
    if (!mir_source_local_builtin_returns_first_arg_type(callee_name))
        return NULL;
    if (ast_call_arg_count(expr) >= 1) {
        const char *arg_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_call_argument(expr, 0));
        if (arg_type != NULL)
            return arg_type;
    }
    return "Int";
}

static const char *
mir_source_local_expr_type_name(const MIRProgram *program,
                                const MIRRoutine *routine,
                                MIRSourceLocalTypeScratch *scratch,
                                ASTNode *expr)
{
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return "Long";
        return ast_number_is_float(expr) ? "Float" : "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_IDENTIFIER:
        return mir_source_local_identifier_type_name(program, routine,
            ast_identifier_name(expr));
    case AST_UNARY:
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return "Bool";
        return mir_source_local_expr_type_name(program, routine,
            scratch, ast_unary_operand(expr));
    case AST_BINARY:
        switch (ast_binary_operator(expr).type) {
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR:
            return "Bool";
        default:
            return mir_source_local_expr_type_name(program, routine,
                scratch, ast_binary_left(expr));
        }
    case AST_MEMBER_ACCESS: {
        const char *owner_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_member_object(expr));
        const MIRDeclField *field = mir_source_local_header_field(
            mir_source_local_decl_header_for_value_type(program, owner_type),
            ast_member_name(expr));
        return field != NULL ? field->type_name : NULL;
    }
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(expr);
        if (callee != NULL && callee->type == AST_MEMBER_ACCESS) {
            const char *receiver_type = mir_source_local_expr_type_name(
                program, routine, scratch, ast_member_object(callee));
            const MIRDeclMethod *method = mir_source_local_header_method(
                mir_source_local_decl_header_for_value_type(program,
                    receiver_type),
                ast_member_name(callee));
            return method != NULL ? method->return_type_name : NULL;
        }
        if (callee != NULL && callee->type == AST_IDENTIFIER) {
            const char *callee_name = ast_identifier_name(callee);
            const MIRDeclMethod *fn = mir_source_local_header_method(
                mir_source_local_decl_header(program,
                    callee_name),
                callee_name);
            if (fn != NULL)
                return fn->return_type_name;
            {
                const char *owner_method_return =
                    mir_source_local_owner_method_return_type_name(program,
                        routine, callee_name);
                if (owner_method_return != NULL)
                    return owner_method_return;
            }
            return mir_source_local_builtin_call_type_name(program, routine,
                scratch, expr, callee_name);
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

static bool
mir_source_local_type_capture_node(const MIRProgram *program,
                                   MIRRoutine *routine,
                                   ASTNode *node)
{
    if (routine == NULL || node == NULL)
        return true;
    switch (node->type) {
    case AST_LET_DECL: {
        ASTNode *type_node = ast_let_type(node);
        MIRSourceLocalTypeScratch scratch = { 0 };
        if (type_node != NULL)
            return mir_source_local_type_append(program, routine,
                ast_let_name(node), type_node);
        return mir_source_local_type_append_name(program, routine,
            ast_let_name(node),
            mir_source_local_expr_type_name(program, routine, &scratch,
                ast_let_initializer(node)));
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            if (!mir_source_local_type_capture_node(program, routine,
                    ast_block_statement(node, i))) {
                return false;
            }
        }
        return true;
    }
    case AST_IF_STMT:
        return mir_source_local_type_capture_node(program, routine,
                   ast_if_then_branch(node))
            && mir_source_local_type_capture_node(program, routine,
                   ast_if_else_branch(node));
    case AST_WHILE_LOOP:
        return mir_source_local_type_capture_node(program, routine,
            ast_while_body(node));
    case AST_FOR_LOOP:
        return mir_source_local_type_capture_node(program, routine,
            ast_for_body(node));
    case AST_WITH_STMT: {
        const char *alias = ast_with_alias(node);
        char *claim_type = mir_claim_abi_type_name_from_ast(node);
        bool ok = true;
        if (alias != NULL && claim_type != NULL)
            ok = mir_source_local_type_append_name(program, routine, alias,
                claim_type);
        free(claim_type);
        return ok && mir_source_local_type_capture_node(program, routine,
            ast_with_body(node));
    }
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *c = ast_match_case_at(node, i);
            if (c != NULL && c->type == AST_MATCH_CASE
                && !mir_source_local_type_capture_node(program, routine,
                    ast_match_case_body(c))) {
                return false;
            }
        }
        return true;
    }
    default:
        return true;
    }
}

static const char *
mir_source_local_type_find_in_ast_node(ASTNode *node, const char *local_name)
{
    if (node == NULL || local_name == NULL)
        return NULL;
    switch (node->type) {
    case AST_LET_DECL: {
        const char *name = ast_let_name(node);
        if (name != NULL && strcmp(name, local_name) == 0) {
            ASTNode *type_node = ast_let_type(node);
            if (type_node != NULL && type_node->type == AST_TYPE)
                return ast_type_name(type_node);
        }
        return NULL;
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            const char *type_name = mir_source_local_type_find_in_ast_node(
                ast_block_statement(node, i), local_name);
            if (type_name != NULL)
                return type_name;
        }
        return NULL;
    }
    case AST_IF_STMT: {
        const char *type_name = mir_source_local_type_find_in_ast_node(
            ast_if_then_branch(node), local_name);
        if (type_name != NULL)
            return type_name;
        return mir_source_local_type_find_in_ast_node(
            ast_if_else_branch(node), local_name);
    }
    case AST_WHILE_LOOP:
        return mir_source_local_type_find_in_ast_node(
            ast_while_body(node), local_name);
    case AST_FOR_LOOP:
        return mir_source_local_type_find_in_ast_node(
            ast_for_body(node), local_name);
    case AST_WITH_STMT:
        return mir_source_local_type_find_in_ast_node(
            ast_with_body(node), local_name);
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *match_case = ast_match_case_at(node, i);
            if (match_case == NULL || match_case->type != AST_MATCH_CASE)
                continue;
            const char *type_name = mir_source_local_type_find_in_ast_node(
                ast_match_case_body(match_case), local_name);
            if (type_name != NULL)
                return type_name;
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

const char *
mir_source_local_type_name_in_ast(ASTNode *body, const char *local_name)
{
    return mir_source_local_type_find_in_ast_node(body, local_name);
}

bool
mir_routine_source_local_type_names_capture(const MIRProgram *program,
                                            MIRRoutine *routine)
{
    if (routine == NULL || routine->ast == NULL
        || routine->ast->type != AST_FUNC_DECL) {
        return true;
    }
    return mir_source_local_type_capture_node(program, routine,
        ast_func_body(routine->ast));
}
