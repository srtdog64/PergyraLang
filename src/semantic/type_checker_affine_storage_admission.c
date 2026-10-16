#include "diag_codes.h"
#include "type_checker_internal.h"
#include <stdlib.h>
#include <string.h>

/* Aggregate storage admission for affine handles.
 *
 * Two handle families have no aggregate move or retirement plan, so a type
 * that would physically store one in a field, payload, element, key or a
 * nested parameter or return shape is refused at that storage site:
 * - Future/RemoteFuture completion handles are affine join obligations;
 * - Rc, Weak, Box, Allocator and TextBuilder are single-owner runtime handles
 *   and Slot, SecureSlot and DeviceSlot are anchored resource handles: each
 *   names one runtime storage, so a stored copy dangles after the first
 *   release (a Some(handle) payload compiled into a use-after-free before
 *   this family was admitted here).
 * A handle held directly by a let, parameter or return is not aggregate
 * storage; those sites exclude it with semantic_type_is_direct_affine_handle().
 * The self-host twin is ast_future_storage_admission_owner.pgy. */
typedef enum {
    AFFINE_STORAGE_FAMILY_COMPLETION = 0,
    AFFINE_STORAGE_FAMILY_RUNTIME_HANDLE,
    /* The anchored subset of the runtime family, used only to name the cause. */
    AFFINE_STORAGE_FAMILY_ANCHORED_HANDLE
} AffineStorageFamily;

static bool
affine_storage_leaf(const Type *type, AffineStorageFamily family)
{
    if (family == AFFINE_STORAGE_FAMILY_COMPLETION)
        return semantic_type_is_future_handle(type);
    if (family == AFFINE_STORAGE_FAMILY_ANCHORED_HANDLE)
        return type_is_anchored_resource_handle(type);
    return type_is_single_owner_runtime_handle(type)
        || type_is_anchored_resource_handle(type);
}

static bool
affine_storage_family_transfers_channel(AffineStorageFamily family)
{
    return family != AFFINE_STORAGE_FAMILY_COMPLETION;
}

bool
semantic_type_is_direct_affine_handle(const Type *type)
{
    return affine_storage_leaf(type, AFFINE_STORAGE_FAMILY_COMPLETION)
        || affine_storage_leaf(type, AFFINE_STORAGE_FAMILY_RUNTIME_HANDLE);
}

/* Declaration syntax names a handle by its spelling. A source declaration
 * with the same spelling (a user class named Box) owns that name instead. */
static bool
affine_storage_leaf_name(const char *name, AffineStorageFamily family,
                         SemanticContext *ctx)
{
    static const char *const completion[] = { "Future", "RemoteFuture" };
    static const char *const runtime[] = {
        "Rc", "Weak", "Box", "Allocator", "TextBuilder",
        "Slot", "SecureSlot", "DeviceSlot"
    };
    static const char *const anchored[] = { "Slot", "SecureSlot", "DeviceSlot" };
    const char *const *names = runtime;
    size_t count = sizeof(runtime) / sizeof(runtime[0]);

    if (family == AFFINE_STORAGE_FAMILY_COMPLETION) {
        names = completion;
        count = sizeof(completion) / sizeof(completion[0]);
    } else if (family == AFFINE_STORAGE_FAMILY_ANCHORED_HANDLE) {
        names = anchored;
        count = sizeof(anchored) / sizeof(anchored[0]);
    }

    if (name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, names[i]) != 0)
            continue;
        return family == AFFINE_STORAGE_FAMILY_COMPLETION || ctx == NULL
            || semantic_find_class_decl_by_name(ctx, name) == NULL;
    }
    return false;
}

/* Type arguments are not storage by themselves. Inspect only constructors
 * whose current value representation carries their arguments, so an unused
 * phantom parameter does not make an unrelated nominal type affine. */
static bool
affine_storage_constructor_name_stores_arguments(const char *name)
{
    static const char *const stored[] = {
        "Array", "List", "Queue", "Set", "HashMap", "Option", "Result",
        "Box", "Rc", "Channel"
    };
    if (name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(stored) / sizeof(stored[0]); i++) {
        if (strcmp(name, stored[i]) == 0)
            return true;
    }
    return false;
}

/* A channel transfers an own handle through its transport contract
 * (PGY_SEM_CHANNEL_TRANSPORT_INVALID owns that boundary), so for runtime and
 * anchored handles a Channel is a transfer, not aggregate storage. */
static bool
affine_storage_type_constructor_stores_arguments(const Type *type,
                                                 AffineStorageFamily family)
{
    Type *constructor = type_constructed_constructor(type);
    if (constructor == NULL)
        return false;
    if (affine_storage_family_transfers_channel(family)
        && constructor->name != NULL
        && strcmp(constructor->name, "Channel") == 0)
        return false;
    return affine_storage_constructor_name_stores_arguments(constructor->name);
}

static bool affine_storage_type_contains(const Type *type,
    SemanticContext *ctx, unsigned depth, AffineStorageFamily family);
static bool affine_storage_decl_fields_syntax_contain(ASTNode *decl,
    const bool *affine_params, size_t param_count,
    SemanticContext *ctx, unsigned depth, AffineStorageFamily family);

static void
affine_storage_report_resolution_oom(SemanticContext *ctx, ASTNode *decl)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_RESOLUTION_OOM,
        PGY_FIX_REDUCE_SCOPE_OR_RETRY,
        decl,
        "Could not allocate nominal field facts for affine handle containment.\n"
        "Reason:\n"
        "- aggregate storage cannot be admitted without its field shape\n"
        "Fix:\n"
        "- reduce this compilation unit size and retry");
}

/* A generic argument is storage only where the declaration field actually
 * mentions it, including under a known stored wrapper. An unused Phantom<T>
 * argument is therefore not an affine stored handle. */
static bool
affine_storage_field_syntax_contains(const ASTNode *field_type,
                                     const GenericParams *class_generics,
                                     const bool *affine_params,
                                     size_t param_count,
                                     SemanticContext *ctx,
                                     unsigned depth,
                                     AffineStorageFamily family)
{
    const char *name;
    GenericParams *field_args;
    ASTNode *nested_decl;

    if (field_type == NULL || depth > 32)
        return depth > 32;
    if (ast_type_tuple_element_count(field_type) > 0) {
        for (size_t i = 0; i < ast_type_tuple_element_count(field_type); i++) {
            if (affine_storage_field_syntax_contains(
                    ast_type_tuple_element(field_type, i), class_generics,
                    affine_params, param_count, ctx, depth + 1, family))
                return true;
        }
        return false;
    }
    name = ast_type_name(field_type);
    if (name == NULL)
        return false;
    if (affine_storage_leaf_name(name, family, ctx))
        return true;
    for (size_t i = 0; i < ast_generic_param_count(class_generics); i++) {
        GenericParam *param = ast_generic_param_at(class_generics, i);
        const char *param_name = ast_generic_param_name(param);
        if (param_name != NULL && strcmp(name, param_name) == 0)
            return i < param_count ? affine_params[i] : true;
    }
    field_args = ast_type_generic_args(field_type);
    if (affine_storage_family_transfers_channel(family)
        && strcmp(name, "Channel") == 0)
        return false;
    if (affine_storage_constructor_name_stores_arguments(name)) {
        for (size_t i = 0; i < ast_generic_param_count(field_args); i++) {
            if (affine_storage_field_syntax_contains(
                    ast_generic_param_constraint(
                        ast_generic_param_at(field_args, i)),
                    class_generics, affine_params, param_count, ctx,
                    depth + 1, family))
                return true;
        }
        return false;
    }
    nested_decl = ctx != NULL
        ? semantic_find_class_decl_by_name(ctx, name) : NULL;
    if (nested_decl != NULL) {
        GenericParams *nested_generics = ast_class_generic_params(nested_decl);
        size_t nested_count = ast_generic_param_count(nested_generics);
        bool *nested_affine = nested_count > 0
            ? calloc(nested_count, sizeof(bool)) : NULL;
        bool contains;
        if (nested_count > 0 && nested_affine == NULL) {
            affine_storage_report_resolution_oom(ctx, nested_decl);
            return false;
        }
        for (size_t i = 0; i < nested_count; i++) {
            ASTNode *arg = i < ast_generic_param_count(field_args)
                ? ast_generic_param_constraint(
                    ast_generic_param_at(field_args, i)) : NULL;
            if (arg == NULL)
                arg = ast_generic_param_default_type(
                    ast_generic_param_at(nested_generics, i));
            nested_affine[i] = arg == NULL
                || affine_storage_field_syntax_contains(
                    arg, class_generics, affine_params, param_count,
                    ctx, depth + 1, family);
        }
        contains = affine_storage_decl_fields_syntax_contain(
            nested_decl, nested_affine, nested_count, ctx, depth + 1, family);
        free(nested_affine);
        return contains;
    }
    /* If the class declaration is unavailable, an affine type argument must
     * not be assumed phantom at this storage boundary. */
    for (size_t i = 0; i < ast_generic_param_count(field_args); i++) {
        if (affine_storage_field_syntax_contains(
                ast_generic_param_constraint(
                    ast_generic_param_at(field_args, i)),
                class_generics, affine_params, param_count,
                ctx, depth + 1, family))
            return true;
    }
    return false;
}

static bool
affine_storage_decl_fields_syntax_contain(ASTNode *decl,
                                          const bool *affine_params,
                                          size_t param_count,
                                          SemanticContext *ctx,
                                          unsigned depth,
                                          AffineStorageFamily family)
{
    PgyDeclField *fields = NULL;
    size_t count;
    bool contains = false;

    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return false;
    if (!pgy_class_decl_field_model_try_build(decl, &fields, &count)) {
        affine_storage_report_resolution_oom(ctx, decl);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (affine_storage_field_syntax_contains(
                fields[i].type_ast, ast_class_generic_params(decl),
                affine_params, param_count, ctx, depth + 1, family)) {
            contains = true;
            break;
        }
    }
    pgy_decl_field_model_free(fields, count);
    return contains;
}

static bool
affine_storage_nominal_fields_contain(const Type *type,
                                      SemanticContext *ctx,
                                      unsigned depth,
                                      AffineStorageFamily family)
{
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);
    GenericParams *generics;
    size_t param_count;
    bool *affine_params;
    bool contains;

    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return true;
    generics = ast_class_generic_params(decl);
    param_count = ast_generic_param_count(generics);
    affine_params = param_count > 0
        ? calloc(param_count, sizeof(bool)) : NULL;
    if (param_count > 0 && affine_params == NULL) {
        affine_storage_report_resolution_oom(ctx, decl);
        return false;
    }
    for (size_t i = 0; i < param_count; i++) {
        Type *arg = type_constructed_arg(type, i);
        affine_params[i] = arg == NULL
            || affine_storage_type_contains(arg, ctx, depth + 1, family);
    }
    contains = affine_storage_decl_fields_syntax_contain(
        decl, affine_params, param_count, ctx, depth + 1, family);
    free(affine_params);
    return contains;
}

/* This is only a cheap candidate filter, not a storage verdict. Phantom<T>
 * still reaches the field model when T mentions a handle, then remains legal
 * if none of its fields physically carry T. */
static bool
affine_storage_type_argument_mentions(const Type *type, unsigned depth,
                                      AffineStorageFamily family)
{
    if (type == NULL)
        return false;
    if (depth > 32)
        return true;
    if (affine_storage_leaf(type, family))
        return true;
    if (type->kind == TYPE_KIND_SLOT)
        return affine_storage_type_argument_mentions(
            type_slot_inner_type(type), depth + 1, family);
    if (type->kind == TYPE_KIND_TUPLE) {
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (affine_storage_type_argument_mentions(
                    type_tuple_get_element(type, i), depth + 1, family))
                return true;
        }
    }
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (affine_storage_type_argument_mentions(
                    type_constructed_arg(type, i), depth + 1, family))
                return true;
        }
    }
    return false;
}

static bool
affine_storage_type_contains(const Type *type,
                             SemanticContext *ctx,
                             unsigned depth,
                             AffineStorageFamily family)
{
    if (type == NULL)
        return false;
    if (depth > 32)
        return true;
    /* An anchored Slot is itself the runtime-handle leaf; for completion
     * handles it is a wrapper whose payload is inspected below. */
    if (affine_storage_leaf(type, family))
        return true;
    if (type->kind == TYPE_KIND_SLOT)
        return affine_storage_type_contains(
            type_slot_inner_type(type), ctx, depth + 1, family);
    if (type->kind == TYPE_KIND_TUPLE) {
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (affine_storage_type_contains(
                    type_tuple_get_element(type, i), ctx, depth + 1, family))
                return true;
        }
        return false;
    }
    if (type->kind == TYPE_KIND_CONSTRUCTED
        && affine_storage_family_transfers_channel(family)
        && type_is_constructed_named(type, "Channel"))
        return false;
    if (type->kind == TYPE_KIND_CONSTRUCTED
        && affine_storage_type_constructor_stores_arguments(type, family)) {
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (affine_storage_type_contains(
                    type_constructed_arg(type, i), ctx, depth + 1, family))
                return true;
        }
        return false;
    }
    /* A concrete nominal field is refused at its declaration. A generic
     * instance needs field inspection only when an actual type argument can
     * carry a handle of this family. */
    if (ctx != NULL && type->kind == TYPE_KIND_CONSTRUCTED
        && affine_storage_type_argument_mentions(type, depth + 1, family))
        return affine_storage_nominal_fields_contain(type, ctx, depth + 1,
                                                     family);
    return false;
}

#define AFFINE_RUNTIME_HANDLE_STORAGE_MESSAGE \
        "Handle storage in %s is not admitted for type '%s'.\n" \
        "Reason:\n" \
        "- Rc, Weak, Box, Allocator and TextBuilder are single-owner runtime handles; Slot, SecureSlot and DeviceSlot are anchored resource handles\n" \
        "- a stored copy would name the same runtime storage as its source, and releasing either one would leave the other dangling\n" \
        "- no aggregate move/retirement plan is admitted for these handles\n" \
        "Fix:\n" \
        "- keep the handle in its own binding\n" \
        "- or store the value it manages instead of the handle"

bool
semantic_reject_affine_aggregate_storage(ASTNode *site,
                                         const Type *stored_type,
                                         SemanticContext *ctx,
                                         const char *boundary)
{
    if (ctx == NULL)
        return false;
    if (affine_storage_type_contains(stored_type, ctx, 0,
                                     AFFINE_STORAGE_FAMILY_COMPLETION)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TASK_LIFECYCLE,
            PGY_CAUSE_TASK_LIFECYCLE,
            PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
            site,
            "Completion handle storage in %s is not admitted for type '%s'.\n"
            "Reason:\n"
            "- Future/RemoteFuture completion handles are affine join obligations\n"
            "- aggregate element copying or extraction could duplicate one live handle\n"
            "- no aggregate move/retirement plan is admitted at this boundary\n"
            "Fix:\n"
            "- keep the handle in its direct binding and await it once\n"
            "- transfer it only through an explicit own Future parameter",
            boundary != NULL ? boundary : "aggregate storage",
            type_name_or_unknown(stored_type));
        return true;
    }
    if (!affine_storage_type_contains(stored_type, ctx, 0,
                                      AFFINE_STORAGE_FAMILY_RUNTIME_HANDLE))
        return false;
    if (affine_storage_type_contains(stored_type, ctx, 0,
                                     AFFINE_STORAGE_FAMILY_ANCHORED_HANDLE)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
            PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            site,
            AFFINE_RUNTIME_HANDLE_STORAGE_MESSAGE,
            boundary != NULL ? boundary : "aggregate storage",
            type_name_or_unknown(stored_type));
        return true;
    }
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site,
        AFFINE_RUNTIME_HANDLE_STORAGE_MESSAGE,
        boundary != NULL ? boundary : "aggregate storage",
        type_name_or_unknown(stored_type));
    return true;
}
