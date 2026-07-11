#include "mir_source_local_expr_types.h"

#include <stdio.h>
#include <string.h>

#include "mir_decl_headers.h"
#include "mir_source_local_expr_call_facts.h"
#include "../common/pgy_builtin_type_table.h"
#include "../parser/ast_api.h"

char *
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
mir_source_local_decl_header_is_constructor_type(const MIRDeclHeader *header)
{
    if (header == NULL)
        return false;
    switch (header->ast_type) {
    case AST_CLASS_DECL:
    case AST_ZONE_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
        return true;
    default:
        return false;
    }
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

static bool
mir_source_local_unwrap_iterable_type(const char *type_name,
                                      char *out,
                                      size_t out_size)
{
    static const char *prefixes[] = {
        "Array<",
        "Slice<",
        "List<",
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

static bool
mir_source_local_unwrap_channel_type(const char *type_name,
                                     char *out,
                                     size_t out_size)
{
    const char *prefix = "Channel<";
    const char *open;
    const char *close;
    size_t len;

    if (type_name == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (strncmp(type_name, prefix, strlen(prefix)) != 0)
        return false;
    open = type_name + strlen(prefix);
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

static bool
mir_source_local_unwrap_array_or_slice_type(const char *type_name,
                                            char *out,
                                            size_t out_size)
{
    static const char *prefixes[] = {
        "Array<",
        "Slice<",
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

static bool
mir_source_local_unwrap_future_type(const char *type_name,
                                    char *out,
                                    size_t out_size,
                                    bool *is_remote_out)
{
    static const char *future_prefix = "Future<";
    static const char *remote_prefix = "RemoteFuture<";
    const char *open = NULL;
    const char *close;
    size_t len;

    if (is_remote_out != NULL)
        *is_remote_out = false;
    if (type_name == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (strncmp(type_name, future_prefix, strlen(future_prefix)) == 0) {
        open = type_name + strlen(future_prefix);
    } else if (strncmp(type_name, remote_prefix,
                   strlen(remote_prefix)) == 0) {
        open = type_name + strlen(remote_prefix);
        if (is_remote_out != NULL)
            *is_remote_out = true;
    } else {
        return false;
    }
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
    if (name != NULL && strcmp(name, "self") == 0) {
        const char *owner_name = mir_source_local_routine_owner_name(routine);
        if (owner_name != NULL && owner_name[0] != '\0')
            return owner_name;
    }
    type_name = mir_routine_source_local_type_name(routine, name);
    if (type_name != NULL)
        return type_name;
    return mir_source_local_owner_field_type_name(program, routine, name);
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
    {
        const char *fixed_return =
            mir_source_local_builtin_fixed_return_type_name(callee_name);
        if (fixed_return != NULL)
            return fixed_return;
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

const char *
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
    case AST_ARRAY_LITERAL:
        if (ast_array_literal_count(expr) > 0
            && ast_array_literal_element(expr, 0) != NULL) {
            const char *inner = mir_source_local_expr_type_name(program,
                routine, scratch, ast_array_literal_element(expr, 0));
            return mir_source_local_type_scratch_format(scratch, "Array",
                inner);
        }
        return NULL;
    case AST_IDENTIFIER:
        return mir_source_local_identifier_type_name(program, routine,
            ast_identifier_name(expr));
    case AST_CHANNEL_RECV: {
        const char *channel_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_channel_recv_channel(expr));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        if (mir_source_local_unwrap_channel_type(channel_type, inner,
                sizeof(inner))) {
            char *buffer = mir_source_local_type_scratch_next(scratch);
            if (buffer == NULL)
                return NULL;
            memcpy(buffer, inner, strlen(inner) + 1);
            return buffer;
        }
        return NULL;
    }
    case AST_AWAIT_EXPR: {
        const char *future_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_await_expression(expr));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        bool is_remote = false;
        char *buffer;

        if (!mir_source_local_unwrap_future_type(future_type, inner,
                sizeof(inner), &is_remote)) {
            return NULL;
        }
        if (is_remote)
            return mir_source_local_type_scratch_format(scratch, "Result",
                inner);
        buffer = mir_source_local_type_scratch_next(scratch);
        if (buffer == NULL)
            return NULL;
        memcpy(buffer, inner, strlen(inner) + 1);
        return buffer;
    }
    case AST_SPAWN_EXPR: {
        const char *inner = mir_source_local_expr_type_name(program,
            routine, scratch, ast_spawn_function(expr));
        return mir_source_local_type_scratch_format(scratch, "Future", inner);
    }
    case AST_PARALLEL_BLOCK: {
        /* Expression-form parallel join (docs/181 R2): the result type is
         * the checker-sealed give fact -- never re-derived from the body. */
        const char *give = ast_parallel_join_give_type(expr);
        if (give == NULL || give[0] == '\0')
            return NULL;
        return mir_source_local_type_scratch_format(scratch, "Array", give);
    }
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
            const char *member_name = ast_member_name(callee);
            const char *receiver_type = mir_source_local_expr_type_name(
                program, routine, scratch, ast_member_object(callee));
            if (member_name != NULL && strcmp(member_name, "Slice") == 0) {
                char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
                if (mir_source_local_unwrap_array_or_slice_type(receiver_type,
                        inner, sizeof(inner))) {
                    return mir_source_local_type_scratch_format(scratch,
                        "Slice", inner);
                }
                return NULL;
            }
            const MIRDeclMethod *method = mir_source_local_header_method(
                mir_source_local_decl_header_for_value_type(program,
                    receiver_type),
                member_name);
            return method != NULL ? method->return_type_name : NULL;
        }
        if (callee != NULL && callee->type == AST_IDENTIFIER) {
            const char *callee_name = ast_identifier_name(callee);
            const MIRDeclHeader *callee_header =
                mir_source_local_decl_header(program, callee_name);
            if (mir_source_local_decl_header_is_constructor_type(callee_header))
                return callee_header->name;
            if (callee_header != NULL
                && callee_header->ast_type == AST_INTENT_DECL) {
                return "Bool";
            }
            const MIRDeclMethod *fn = mir_source_local_header_method(
                callee_header, callee_name);
            if (fn != NULL)
                return fn->return_type_name;
            {
                const char *top_return =
                    mir_source_local_call_return_type_name(
                        program, routine, scratch, expr, callee_name);
                if (top_return != NULL)
                    return top_return;
            }
            {
                const char *extern_return =
                    mir_source_local_extern_return_type_name(program,
                        scratch, callee_name);
                if (extern_return != NULL)
                    return extern_return;
            }
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

const char *
mir_source_local_for_loop_variable_type_name(const MIRProgram *program,
                                             const MIRRoutine *routine,
                                             MIRSourceLocalTypeScratch *scratch,
                                             ASTNode *node)
{
    ASTNode *iterable;
    const char *iterable_type;
    char *inner;

    if (node == NULL || node->type != AST_FOR_LOOP
        || ast_for_variable(node) == NULL) {
        return NULL;
    }

    iterable = ast_for_iterable(node);
    if (iterable == NULL)
        return "Int";

    iterable_type = mir_source_local_expr_type_name(program, routine,
        scratch, iterable);
    inner = mir_source_local_type_scratch_next(scratch);
    if (inner == NULL
        || !mir_source_local_unwrap_iterable_type(iterable_type, inner,
            MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE)) {
        return NULL;
    }
    return inner;
}
