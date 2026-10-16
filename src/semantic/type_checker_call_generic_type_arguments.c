#include "type_checker_internal.h"
#include "type_checker_call_generic_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

/* A type fixes a parameter only when no component is unresolved: None, []
 * and a contextual Ok/Err carry no type for it. */
bool
generic_call_type_is_resolved(const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN)
        return false;
    switch (type->kind) {
    case TYPE_KIND_CONSTRUCTED:
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (!generic_call_type_is_resolved(type_constructed_arg(type, i)))
                return false;
        }
        return true;
    case TYPE_KIND_TUPLE:
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (!generic_call_type_is_resolved(
                    type_tuple_get_element(type, i)))
                return false;
        }
        return true;
    case TYPE_KIND_SLOT:
        return generic_call_type_is_resolved(type_slot_inner_type(type));
    default:
        return true;
    }
}

static bool
generic_type_name_append(char **text, size_t *len, size_t *cap,
                         const char *piece, size_t piece_len)
{
    char *grown;
    size_t next;

    if (*len + piece_len + 1 > *cap) {
        next = *cap > 0 ? *cap : 32;
        while (next < *len + piece_len + 1)
            next *= 2;
        grown = realloc(*text, next);
        if (grown == NULL)
            return false;
        *text = grown;
        *cap = next;
    }
    memcpy(*text + *len, piece, piece_len);
    *len += piece_len;
    (*text)[*len] = '\0';
    return true;
}

/* MIR renders a declared type with its arguments joined by ',' and no
 * space, and the checker's own names put ", " between them, so a bound type
 * is rendered from its structure. A callable, Void, Never or unresolved type
 * has no spelling in MIR's grammar. */
static bool
generic_type_name_render(const Type *type, char **text, size_t *len,
                         size_t *cap)
{
    const char *head = "";
    size_t head_len = 0;
    size_t count = 1;
    const char *open = "<";
    const char *close = ">";

    if (!generic_call_type_is_resolved(type) || type == TYPE_VOID
        || type == TYPE_NEVER || type->name == NULL
        || type->kind == TYPE_KIND_FUNCTION)
        return false;
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        head = type_constructed_constructor(type) != NULL
            ? type_constructed_constructor(type)->name : NULL;
        if (head == NULL)
            return false;
        head_len = strlen(head);
        count = type_constructed_arg_count(type);
    } else if (type->kind == TYPE_KIND_TUPLE) {
        count = type_tuple_arity(type);
        open = "(";
        close = ")";
    } else if (type->kind == TYPE_KIND_SLOT) {
        head = type->name; /* "Slot<", "ReadView<", ... */
        head_len = strcspn(type->name, "<");
    } else {
        return generic_type_name_append(text, len, cap, type->name,
            strlen(type->name));
    }
    if (!generic_type_name_append(text, len, cap, head, head_len)
        || !generic_type_name_append(text, len, cap, open, 1))
        return false;
    for (size_t i = 0; i < count; i++) {
        const Type *component = type->kind == TYPE_KIND_CONSTRUCTED
            ? type_constructed_arg(type, i)
            : type->kind == TYPE_KIND_TUPLE
                ? type_tuple_get_element(type, i)
                : type_slot_inner_type(type);
        if ((i > 0 && !generic_type_name_append(text, len, cap, ",", 1))
            || !generic_type_name_render(component, text, len, cap))
            return false;
    }
    return generic_type_name_append(text, len, cap, close, 1);
}

bool
semantic_generic_call_seal_type_arguments(SemanticContext *ctx,
                                          ASTNode *call,
                                          const char *display_name,
                                          GenericParams *params,
                                          Type *const *types,
                                          size_t count)
{
    char **names;
    const char *callee = display_name != NULL ? display_name : "<call>";

    if (call == NULL || call->type != AST_CALL || types == NULL || count == 0)
        return false;
    names = calloc(count, sizeof(char *));
    if (names == NULL) {
        semantic_error(ctx, call,
            "Generic call type-argument binding allocation failed");
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        size_t len = 0;
        size_t cap = 0;
        const char *param_name;

        if (generic_type_name_render(types[i], &names[i], &len, &cap))
            continue;
        param_name = ast_generic_param_name(ast_generic_param_at(params, i));
        if (param_name == NULL)
            param_name = "<type-param>";
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            call,
            "Call to '%s' binds generic parameter '%s' to '%s', which cannot instantiate it.\n"
            "Reason:\n"
            "- a generic callee is specialized once per type argument\n"
            "- a callable, Void, Never or unresolved type has no specialization\n"
            "Fix:\n"
            "- pass a value of a named type for '%s'\n"
            "- or declare the parameter with a concrete type",
            callee, param_name, type_name_or_unknown(types[i]), param_name);
        for (size_t j = 0; j < count; j++)
            free(names[j]);
        free(names);
        return false;
    }
    if (!ast_call_seal_semantic_generic_arg_type_names(call, names, count)) {
        for (size_t j = 0; j < count; j++)
            free(names[j]);
        free(names);
        semantic_error(ctx, call,
            "Generic call type-argument binding could not be recorded");
        return false;
    }
    return true;
}
