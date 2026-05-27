#include "dir_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static void
dir_clear_error_message(DIRProgram *dir)
{
    if (dir == NULL)
        return;
    free(dir->error_message);
    dir->error_message = NULL;
}

bool
dir_failf(DIRProgram *dir, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *message;

    if (dir == NULL || dir->error_message != NULL)
        return false;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return false;
    }

    message = malloc((size_t)length + 1);
    if (message == NULL) {
        va_end(args);
        return false;
    }

    vsnprintf(message, (size_t)length + 1, fmt, args);
    va_end(args);
    dir->error_message = message;
    return false;
}

static char *
dir_type_name_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static char *
dir_render_type_name_dup(ASTNode *type_node)
{
    char *result = NULL;

    if (type_node == NULL)
        return NULL;

    switch (type_node->type) {
    case AST_TYPE: {
        const char *base_name = ast_type_name(type_node) != NULL
            ? ast_type_name(type_node)
            : "Int";
        result = pergyra_strdup(base_name);
        if (result == NULL)
            return NULL;
        GenericParams *generic_args = ast_type_generic_args(type_node);
        size_t generic_count = ast_generic_param_count(generic_args);
        if (generic_count > 0) {
            char *next = dir_type_name_strdup_fmt("%s<", result);
            free(result);
            result = next;
            if (result == NULL)
                return NULL;
            for (size_t i = 0; i < generic_count; i++) {
                GenericParam *param = ast_generic_param_at(generic_args, i);
                char *arg_text = NULL;
                if (ast_generic_param_constraint(param) != NULL) {
                    arg_text = dir_render_type_name_dup(
                        ast_generic_param_constraint(param));
                } else if (ast_generic_param_name(param) != NULL) {
                    arg_text = pergyra_strdup(ast_generic_param_name(param));
                } else if (ast_generic_param_default_type(param) != NULL) {
                    arg_text = dir_render_type_name_dup(
                        ast_generic_param_default_type(param));
                } else {
                    arg_text = pergyra_strdup("Int");
                }
                next = dir_type_name_strdup_fmt(
                    "%s%s%s",
                    result,
                    i > 0 ? ", " : "",
                    arg_text != NULL ? arg_text : "Int");
                free(arg_text);
                free(result);
                result = next;
                if (result == NULL)
                    return NULL;
            }
            next = dir_type_name_strdup_fmt("%s>", result);
            free(result);
            result = next;
        }
        return result;
    }
    case AST_CHANNEL_TYPE: {
        char *inner = dir_render_type_name_dup(
            ast_channel_type_element_type(type_node));
        result = dir_type_name_strdup_fmt("Channel<%s>",
                                          inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    case AST_FUTURE_TYPE: {
        char *inner = dir_render_type_name_dup(
            ast_future_type_value_type(type_node));
        result = dir_type_name_strdup_fmt("Future<%s>",
                                          inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    default:
        break;
    }

    return NULL;
}

const char *
type_name(DIRProgram *dir, ASTNode *type_node)
{
    char *owned;

    if (dir == NULL || type_node == NULL)
        return NULL;

    owned = dir_render_type_name_dup(type_node);
    if (owned == NULL)
        return NULL;
    if (!dir_track_owned_name(dir, owned)) {
        free(owned);
        return NULL;
    }
    return owned;
}

bool
dir_domain_slot_is_projection(ASTNode *slot)
{
    return slot != NULL
        && slot->type == AST_DOMAIN_SLOT
        && !ast_domain_slot_is_subject(slot)
        && !ast_domain_slot_is_vessel(slot);
}

DIRProgram *
dir_lower(ASTNode *annotated_ast, char **error_message)
{
    DIRProgram *dir;

    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    dir = calloc(1, sizeof(DIRProgram));
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }

    if (!dir_collect_nodes(dir, annotated_ast) || !dir_collect_edges_and_intents(dir, annotated_ast)) {
        if (error_message != NULL) {
            if (dir->error_message != NULL) {
                *error_message = dir->error_message;
                dir->error_message = NULL;
            } else {
                *error_message = pergyra_strdup("Out of memory");
            }
        }
        dir_clear_error_message(dir);
        dir_destroy(dir);
        return NULL;
    }

    dir_clear_error_message(dir);
    return dir;
}

void
dir_destroy(DIRProgram *dir)
{
    if (dir == NULL)
        return;
    if (dir->intents != NULL) {
        for (size_t i = 0; i < dir->intent_count; i++) {
            free(dir->intents[i].participants);
            for (size_t j = 0; j < dir->intents[i].step_count; j++) {
                free((void *)dir->intents[i].steps[j].who_names);
                free((void *)dir->intents[i].steps[j].required_abilities);
                free((void *)dir->intents[i].steps[j].authorized_by);
            }
            free(dir->intents[i].steps);
        }
    }
    if (dir->owned_names != NULL) {
        for (size_t i = 0; i < dir->owned_name_count; i++)
            free(dir->owned_names[i]);
    }
    free(dir->nodes);
    free(dir->edges);
    free(dir->intents);
    free(dir->owned_names);
    free(dir->error_message);
    free(dir);
}
