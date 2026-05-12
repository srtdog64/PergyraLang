#include "parser_internal.h"

static bool
parser_reserve_call_argument_capacity(Parser *parser, ASTNode *call,
                                      size_t min_capacity,
                                      const char *error_message)
{
    ASTNode **new_args;

    if (call == NULL || call->type != AST_CALL)
        return false;

    while (call->data.call.arg_capacity < min_capacity) {
        size_t next_capacity = call->data.call.arg_capacity == 0
            ? 4
            : call->data.call.arg_capacity * 2;
        char **new_names = NULL;
        if (next_capacity <= call->data.call.arg_capacity
            || next_capacity > (size_t)-1 / sizeof(ASTNode *)
            || next_capacity > (size_t)-1 / sizeof(char *)) {
            parser_error(parser, error_message);
            return false;
        }
        new_args = realloc(call->data.call.arguments,
            next_capacity * sizeof(ASTNode *));
        if (new_args == NULL) {
            parser_error(parser, error_message);
            return false;
        }
        if (call->data.call.arg_names != NULL) {
            new_names = realloc(call->data.call.arg_names,
                next_capacity * sizeof(char *));
            if (new_names == NULL) {
                call->data.call.arguments = new_args;
                parser_error(parser, error_message);
                return false;
            }
        }
        call->data.call.arguments = new_args;
        if (call->data.call.arg_names != NULL)
            call->data.call.arg_names = new_names;
        call->data.call.arg_capacity = next_capacity;
    }

    return true;
}

bool
parser_prepend_call_argument(Parser *parser, ASTNode *call, ASTNode *argument)
{
    size_t old_count;

    if (call == NULL || call->type != AST_CALL)
        return false;

    old_count = call->data.call.arg_count;
    if (!parser_reserve_call_argument_capacity(
            parser, call, old_count + 1,
            "Out of memory while prepending pipe argument")) {
        return false;
    }

    memmove(call->data.call.arguments + 1, call->data.call.arguments,
            old_count * sizeof(ASTNode *));
    call->data.call.arguments[0] = argument;
    if (call->data.call.arg_names != NULL) {
        memmove(call->data.call.arg_names + 1, call->data.call.arg_names,
                old_count * sizeof(char *));
        call->data.call.arg_names[0] = NULL;
    }
    call->data.call.arg_count = old_count + 1;
    return true;
}

bool
parser_append_call_argument(Parser *parser, ASTNode *call,
                            const char *arg_name, ASTNode *arg)
{
    size_t old_count;

    if (call == NULL || call->type != AST_CALL)
        return false;

    old_count = call->data.call.arg_count;
    if (!parser_reserve_call_argument_capacity(
            parser, call, old_count + 1,
            "Out of memory while parsing call arguments")) {
        return false;
    }

    if (arg_name != NULL && call->data.call.arg_names == NULL) {
        call->data.call.arg_names = calloc(call->data.call.arg_capacity,
            sizeof(char *));
        if (call->data.call.arg_names == NULL) {
            parser_error(parser,
                         "Out of memory while parsing named call arguments");
            return false;
        }
    }

    call->data.call.arguments[old_count] = arg;
    if (call->data.call.arg_names != NULL) {
        call->data.call.arg_names[old_count] = arg_name != NULL
            ? pergyra_strdup(arg_name)
            : NULL;
    }
    call->data.call.arg_count = old_count + 1;
    return true;
}
