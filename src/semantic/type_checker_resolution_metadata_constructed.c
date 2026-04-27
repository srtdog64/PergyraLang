#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"

static Type *
stable_constructed_constructor(const char *name, size_t argc)
{
    if (name == NULL)
        return NULL;
    if (argc == 1 && strcmp(name, "Array") == 0)
        return TYPE_ARRAY;
    if (argc == 1 && strcmp(name, "Slice") == 0)
        return TYPE_SLICE;
    if (argc == 1 && strcmp(name, "List") == 0)
        return TYPE_LIST;
    if (argc == 1 && strcmp(name, "Queue") == 0)
        return TYPE_QUEUE;
    if (argc == 1 && strcmp(name, "Set") == 0)
        return TYPE_SET;
    if (argc == 1 && strcmp(name, "Box") == 0)
        return TYPE_BOX;
    if (argc == 1 && strcmp(name, "Rc") == 0)
        return TYPE_RC;
    if (argc == 1 && strcmp(name, "Weak") == 0)
        return TYPE_WEAK;
    if (argc == 1 && strcmp(name, "Channel") == 0)
        return TYPE_CHANNEL;
    if (argc == 1 && strcmp(name, "Future") == 0)
        return TYPE_FUTURE;
    if (argc == 1 && strcmp(name, "RemoteFuture") == 0)
        return TYPE_REMOTE_FUTURE;
    if (argc == 1 && strcmp(name, "Token") == 0)
        return TYPE_TOKEN;
    if (argc == 1 && strcmp(name, "DeviceSlot") == 0)
        return TYPE_DEVICE_SLOT;
    if (argc == 2 && strcmp(name, "HashMap") == 0)
        return TYPE_HASHMAP;
    if (argc == 1 && strcmp(name, "Option") == 0)
        return TYPE_OPTION;
    if ((argc == 1 || argc == 2) && strcmp(name, "Result") == 0)
        return TYPE_RESULT;
    return NULL;
}

void
semantic_type_resolution_try_record_stable_constructed_type(SemanticContext *ctx,
                                                            ASTNode *type_node)
{
    GenericParams *args_node;
    Type *constructor;
    Type *args[2];
    Type *result;

    if (ctx == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_CHANNEL_TYPE || type_node->type == AST_FUTURE_TYPE) {
        ASTNode *inner_node = type_node->type == AST_CHANNEL_TYPE
            ? type_node->data.channel_type.element_type
            : type_node->data.future_type.value_type;
        Type *inner = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, inner_node);
        Type *shell;
        if (inner == NULL)
            return;
        args[0] = inner;
        shell = type_create_constructed(
            type_node->type == AST_CHANNEL_TYPE ? TYPE_CHANNEL : TYPE_FUTURE,
            args,
            1);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = type_node->data.event_handler_type.param_count;
        Type **param_types = param_count > 0
            ? calloc(param_count, sizeof(Type *))
            : NULL;
        Type *return_type = TYPE_VOID;
        Type *shell;

        if (param_count > 0 && param_types == NULL)
            return;
        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, type_node->data.event_handler_type.param_types[i]);
            if (param_types[i] == NULL) {
                free(param_types);
                return;
            }
        }
        if (type_node->data.event_handler_type.return_type != NULL) {
            return_type = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, type_node->data.event_handler_type.return_type);
            if (return_type == NULL) {
                free(param_types);
                return;
            }
        }
        shell = type_create_function(param_types, param_count, return_type);
        free(param_types);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->type != AST_TYPE)
        return;

    if (type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        size_t element_count = type_node->data.type.tuple_element_count;
        Type **elements = calloc(element_count, sizeof(Type *));
        Type *shell;

        if (elements == NULL)
            return;
        for (size_t i = 0; i < element_count; i++) {
            elements[i] = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, type_node->data.type.tuple_elements[i]);
            if (elements[i] == NULL) {
                free(elements);
                return;
            }
        }
        shell = type_create_tuple(elements, element_count);
        free(elements);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->data.type.name != NULL
        && (strcmp(type_node->data.type.name, "Slot") == 0
            || strcmp(type_node->data.type.name, "SecureSlot") == 0
            || strcmp(type_node->data.type.name, "ReadView") == 0
            || strcmp(type_node->data.type.name, "WriteView") == 0
            || strcmp(type_node->data.type.name, "MoveToken") == 0)) {
        Type *inner;
        Type *slot_type = NULL;

        args_node = type_node->data.type.generic_args;
        if (args_node == NULL || args_node->count != 1)
            return;
        if (args_node->params[0] == NULL)
            return;
        inner = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, args_node->params[0]->constraint);
        if (inner == NULL) {
            inner = semantic_type_resolution_lookup_metadata_name_or_alias(
                ctx, args_node->params[0]->name);
        }
        if (inner == NULL)
            return;

        if (strcmp(type_node->data.type.name, "SecureSlot") == 0)
            slot_type = type_create_slot(inner, true);
        else if (strcmp(type_node->data.type.name, "ReadView") == 0)
            slot_type = type_create_read_view(inner);
        else if (strcmp(type_node->data.type.name, "WriteView") == 0)
            slot_type = type_create_write_view(inner);
        else if (strcmp(type_node->data.type.name, "MoveToken") == 0)
            slot_type = type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
        else
            slot_type = type_create_slot(inner, false);

        if (slot_type != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, slot_type);
        return;
    }

    args_node = type_node->data.type.generic_args;
    if (args_node == NULL || args_node->count == 0 || args_node->count > 2)
        return;

    constructor = stable_constructed_constructor(
        type_node->data.type.name,
        args_node->count);
    if (constructor == NULL)
        return;

    for (size_t i = 0; i < args_node->count; i++) {
        GenericParam *gp = args_node->params[i];
        ASTNode *arg_type = gp != NULL ? gp->constraint : NULL;
        args[i] = semantic_type_resolution_lookup_metadata_type_ref(ctx, arg_type);
        if (args[i] == NULL && gp != NULL) {
            args[i] = semantic_type_resolution_lookup_metadata_name_or_alias(
                ctx, gp->name);
        }
        if (args[i] == NULL)
            return;
    }

    if (constructor == TYPE_HASHMAP
        && !type_equals(args[0], TYPE_STRING)
        && !type_equals(args[0], TYPE_INT)
        && !type_equals(args[0], TYPE_LONG)
        && !type_equals(args[0], TYPE_BOOL)) {
        return;
    }

    result = type_create_constructed(constructor, args, args_node->count);
    if (result == NULL)
        return;
    semantic_type_resolution_record_owned_resolved_type(ctx, type_node, result);
}
