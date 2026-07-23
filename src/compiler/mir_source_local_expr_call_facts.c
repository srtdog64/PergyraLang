#include "mir_source_local_expr_call_facts.h"

#include <stdlib.h>
#include <string.h>

#include "mir_source_local_expr_binding_facts.h"
#include "mir_type_helpers.h"
#include "../common/pgy_builtin_type_table.h"
#include "../parser/ast_api.h"

static const char *
mir_source_local_capture_type_to_scratch(MIRSourceLocalTypeScratch *scratch,
                                         ASTNode *type_node)
{
    char *owned;
    char *buffer;
    size_t len;

    if (scratch == NULL || type_node == NULL)
        return NULL;
    owned = mir_capture_type_name(type_node, NULL);
    if (owned == NULL || owned[0] == '\0') {
        free(owned);
        return NULL;
    }
    len = strlen(owned);
    if (len >= MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE) {
        free(owned);
        return NULL;
    }
    buffer = mir_source_local_type_scratch_next(scratch);
    if (buffer == NULL) {
        free(owned);
        return NULL;
    }
    memcpy(buffer, owned, len + 1);
    free(owned);
    return buffer;
}

static const MIRRoutine *
mir_source_local_top_level_routine(const MIRProgram *program, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < program->routine_count; i++) {
        const MIRRoutine *candidate = &program->routines[i];
        if (candidate->name != NULL
            && strcmp(candidate->name, name) == 0
            && candidate->kind == MIR_SCOPE_FUNCTION
            && candidate->has_signature) {
            return candidate;
        }
    }
    return NULL;
}

static const char *
mir_source_local_generic_actual_type_name(const MIRProgram *program,
                                          const MIRRoutine *caller_routine,
                                          MIRSourceLocalTypeScratch *scratch,
                                          ASTNode *call,
                                          const MIRRoutine *callee_routine,
                                          const char *formal_type_name)
{
    if (callee_routine == NULL || call == NULL || formal_type_name == NULL
        || formal_type_name[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < callee_routine->param_count; i++) {
        const char *param_type_name =
            callee_routine->param_type_names != NULL
                ? callee_routine->param_type_names[i]
                : NULL;
        if (param_type_name != NULL
            && strcmp(param_type_name, formal_type_name) == 0
            && i < ast_call_arg_count(call)) {
            return mir_source_local_expr_type_name(program, caller_routine,
                scratch, ast_call_argument(call, i));
        }
    }
    return NULL;
}

static const char *
mir_source_local_call_return_type_name(const MIRProgram *program,
                                       const MIRRoutine *caller_routine,
                                       MIRSourceLocalTypeScratch *scratch,
                                       ASTNode *call,
                                       const char *name)
{
    const MIRRoutine *callee_routine =
        mir_source_local_top_level_routine(program, name);
    const char *return_type_name;
    const char *actual_type_name;

    if (callee_routine == NULL)
        return NULL;
    return_type_name = callee_routine->return_type_name;
    actual_type_name = mir_source_local_generic_actual_type_name(program,
        caller_routine, scratch, call, callee_routine, return_type_name);
    return actual_type_name != NULL ? actual_type_name : return_type_name;
}

static ASTNode *
mir_source_local_extern_function_decl(const MIRProgram *program,
                                      const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < program->extern_count; i++) {
        ASTNode *block = program->externs != NULL ? program->externs[i] : NULL;
        size_t count = 0;
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        (void)ast_extern_block_declarations(block, &count);
        for (size_t j = 0; j < count; j++) {
            ASTNode *decl = ast_extern_block_declaration(block, j);
            const char *decl_name = decl != NULL && decl->type == AST_FUNC_DECL
                ? ast_declaration_name(decl)
                : NULL;
            if (decl_name != NULL && strcmp(decl_name, name) == 0)
                return decl;
        }
    }
    return NULL;
}

static const char *
mir_source_local_extern_return_type_name(const MIRProgram *program,
                                         MIRSourceLocalTypeScratch *scratch,
                                         const char *name)
{
    ASTNode *decl = mir_source_local_extern_function_decl(program, name);
    return decl != NULL
        ? mir_source_local_capture_type_to_scratch(scratch,
            ast_func_return_type(decl))
        : NULL;
}

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
mir_source_local_builtin_fixed_return_type_name(const char *callee_name)
{
    const char *type_name;

    if (callee_name == NULL)
        return NULL;
    type_name = pgy_builtin_simple_return_type(callee_name);
    return type_name != NULL && strcmp(type_name, "Void") != 0
        ? type_name
        : NULL;
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
    const char *fixed_return;

    if (callee_name != NULL && strcmp(callee_name, "ArrayFilter") == 0
        && ast_call_arg_count(expr) >= 1) {
        const char *array_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_call_argument(expr, 0));
        char element_type[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (mir_source_local_unwrap_array_or_slice_type(array_type,
                element_type, sizeof(element_type))) {
            return mir_source_local_type_scratch_format(scratch, "Array",
                element_type);
        }
        return NULL;
    }
    if (callee_name != NULL && strcmp(callee_name, "ArrayMap") == 0
        && ast_call_arg_count(expr) >= 2) {
        ASTNode *callback = ast_call_argument(expr, 1);
        const MIRRoutine *callback_routine;
        const char *return_type;
        if (callback == NULL || callback->type != AST_IDENTIFIER)
            return NULL;
        callback_routine = mir_source_local_top_level_routine(program,
            ast_identifier_name(callback));
        return_type = callback_routine != NULL
            ? callback_routine->return_type_name : NULL;
        if (return_type == NULL || return_type[0] == '\0'
            || strcmp(return_type, "Void") == 0) {
            return NULL;
        }
        return mir_source_local_type_scratch_format(scratch, "Array",
            return_type);
    }
    if (callee_name != NULL && strcmp(callee_name, "MapKeys") == 0
        && ast_call_arg_count(expr) >= 1) {
        const char *map_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_call_argument(expr, 0));
        char key_type[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (mir_source_local_unwrap_hash_map_key_type(map_type, key_type,
                sizeof(key_type))) {
            return mir_source_local_type_scratch_format(scratch, "Array",
                key_type);
        }
        return NULL;
    }
    if (callee_name != NULL && strcmp(callee_name, "SetValues") == 0
        && ast_call_arg_count(expr) >= 1) {
        const char *set_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_call_argument(expr, 0));
        char element_type[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (mir_source_local_unwrap_set_element_type(set_type, element_type,
                sizeof(element_type))) {
            return mir_source_local_type_scratch_format(scratch, "Array",
                element_type);
        }
        return NULL;
    }

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
    if (callee_name != NULL && strcmp(callee_name, "SliceCopy") == 0
        && ast_call_arg_count(expr) >= 1) {
        const char *slice_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_call_argument(expr, 0));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (slice_type != NULL
            && strncmp(slice_type, "Slice<", 6) == 0
            && mir_source_local_unwrap_array_or_slice_type(slice_type,
                inner, sizeof(inner))) {
            return mir_source_local_type_scratch_format(scratch, "Array",
                inner);
        }
        return NULL;
    }
    fixed_return = mir_source_local_builtin_fixed_return_type_name(callee_name);
    if (fixed_return != NULL)
        return fixed_return;
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
mir_source_local_member_call_type_name(const MIRProgram *program,
                                       const MIRRoutine *routine,
                                       MIRSourceLocalTypeScratch *scratch,
                                       ASTNode *callee)
{
    const char *member_name = ast_member_name(callee);
    const char *receiver_type = mir_source_local_expr_type_name(program,
        routine, scratch, ast_member_object(callee));

    if (member_name != NULL && strcmp(member_name, "Slice") == 0) {
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (mir_source_local_unwrap_array_or_slice_type(receiver_type,
                inner, sizeof(inner))) {
            return mir_source_local_type_scratch_format(scratch, "Slice",
                inner);
        }
        return NULL;
    }
    return mir_source_local_member_method_return_type_name(program,
        receiver_type, member_name);
}

static const char *
mir_source_local_named_call_type_name(const MIRProgram *program,
                                      const MIRRoutine *routine,
                                      MIRSourceLocalTypeScratch *scratch,
                                      ASTNode *expr,
                                      const char *callee_name)
{
    const char *type_name =
        mir_source_local_decl_call_type_name(program, callee_name);

    if (type_name != NULL)
        return type_name;
    type_name = mir_source_local_call_return_type_name(program, routine,
        scratch, expr, callee_name);
    if (type_name != NULL)
        return type_name;
    type_name = mir_source_local_extern_return_type_name(program, scratch,
        callee_name);
    if (type_name != NULL)
        return type_name;
    type_name = mir_source_local_owner_method_return_type_name(program,
        routine, callee_name);
    if (type_name != NULL)
        return type_name;
    return mir_source_local_builtin_call_type_name(program, routine, scratch,
        expr, callee_name);
}

const char *
mir_source_local_call_expr_type_name(const MIRProgram *program,
                                     const MIRRoutine *routine,
                                     MIRSourceLocalTypeScratch *scratch,
                                     ASTNode *expr)
{
    ASTNode *callee = ast_call_callee(expr);

    if (callee != NULL && callee->type == AST_MEMBER_ACCESS) {
        return mir_source_local_member_call_type_name(program, routine,
            scratch, callee);
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER) {
        return mir_source_local_named_call_type_name(program, routine,
            scratch, expr, ast_identifier_name(callee));
    }
    return NULL;
}
