#include "transpiler_type_decl_schedule.h"

#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "codegen_type_mapping.h"
#include "transpiler_class_decl_emit.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum_decl_emit.h"
#include "transpiler_inventory_view.h"

typedef enum {
    TYPE_DECL_READY,
    TYPE_DECL_WAITING,
    TYPE_DECL_INVALID
} TypeDeclReadiness;

static bool
is_value_type_header(const MIRDeclHeader *header)
{
    ASTNodeType type;

    if (header == NULL)
        return false;
    type = mir_decl_header_ast_type_or(header, AST_PROGRAM);
    return type == AST_CLASS_DECL || type == AST_ENUM_DECL;
}

static TypeDeclReadiness
dependency_readiness(const MIRDeclHeaderInventory *inventory,
                     const bool *emitted,
                     const char *type_name)
{
    char first_arg[512];
    char second_arg[512];
    TypeDeclReadiness readiness;

    if (type_name == NULL || type_name[0] == '\0')
        return TYPE_DECL_INVALID;
    if (transpiler_type_name_is_option(type_name)) {
        copy_constructed_arg_name_at(
            type_name, 0, first_arg, sizeof(first_arg));
        if (strcmp(first_arg, "Unknown") == 0)
            return TYPE_DECL_INVALID;
        return dependency_readiness(inventory, emitted, first_arg);
    }
    if (transpiler_type_name_is_result(type_name)) {
        copy_constructed_arg_name_at(
            type_name, 0, first_arg, sizeof(first_arg));
        copy_constructed_arg_name_at(
            type_name, 1, second_arg, sizeof(second_arg));
        if (strcmp(first_arg, "Unknown") == 0)
            return TYPE_DECL_INVALID;
        /* The one-argument bootstrap Result<T> owns a fixed runtime row.
         * Only explicit Result<T, E> materializations are declaration nodes. */
        if (strcmp(second_arg, "Unknown") == 0)
            return TYPE_DECL_READY;
        readiness = dependency_readiness(inventory, emitted, first_arg);
        if (readiness != TYPE_DECL_READY)
            return readiness;
        return dependency_readiness(inventory, emitted, second_arg);
    }
    for (size_t i = 0; i < inventory->count; i++) {
        const MIRDeclHeader *candidate =
            mir_decl_header_inventory_get(inventory, i);
        const char *candidate_name;

        if (!is_value_type_header(candidate))
            continue;
        candidate_name = mir_decl_header_name(candidate);
        if (candidate_name != NULL
            && strcmp(candidate_name, type_name) == 0) {
            return emitted[i] ? TYPE_DECL_READY : TYPE_DECL_WAITING;
        }
    }
    return TYPE_DECL_READY;
}

static TypeDeclReadiness
enum_readiness(const MIRDeclHeaderInventory *inventory,
               const bool *emitted,
               const MIRDeclHeader *header)
{
    size_t variant_count = mir_decl_header_enum_variant_count(header);

    for (size_t i = 0; i < variant_count; i++) {
        const MIRDeclEnumVariant *variant =
            mir_decl_header_enum_variant(header, i);
        size_t param_count = mir_decl_enum_variant_param_count(variant);

        for (size_t p = 0; p < param_count; p++) {
            TypeDeclReadiness readiness = dependency_readiness(
                inventory, emitted,
                mir_decl_enum_variant_param_type_name(variant, p));
            if (readiness != TYPE_DECL_READY)
                return readiness;
        }
    }
    return TYPE_DECL_READY;
}

static TypeDeclReadiness
class_readiness(const MIRDeclHeaderInventory *inventory,
                const bool *emitted,
                const MIRDeclHeader *header)
{
    size_t field_count = mir_decl_header_field_count(header);

    for (size_t i = 0; i < field_count; i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        TypeDeclReadiness readiness = dependency_readiness(
            inventory, emitted, mir_decl_field_type_name(field));
        if (readiness != TYPE_DECL_READY)
            return readiness;
    }
    return TYPE_DECL_READY;
}

static ASTNode *
find_enum_node(ASTNode **types, size_t type_count, const char *name)
{
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *node = types != NULL ? types[i] : NULL;
        const char *candidate;

        if (node == NULL || node->type != AST_ENUM_DECL)
            continue;
        candidate = transpiler_decl_name_local(node);
        if (candidate != NULL && name != NULL
            && strcmp(candidate, name) == 0) {
            return node;
        }
    }
    return NULL;
}

static bool
emit_ready_kind(TranspilerCtx *ctx,
                ASTNode **types,
                size_t type_count,
                const MIRDeclHeaderInventory *inventory,
                bool *emitted,
                ASTNodeType requested_type,
                size_t *remaining,
                bool *made_progress)
{
    for (size_t i = 0; i < inventory->count; i++) {
        const MIRDeclHeader *header =
            mir_decl_header_inventory_get(inventory, i);
        ASTNodeType type;
        TypeDeclReadiness readiness;
        const char *name;

        if (header == NULL || emitted[i])
            continue;
        type = mir_decl_header_ast_type_or(header, AST_PROGRAM);
        if (type != requested_type)
            continue;
        readiness = type == AST_ENUM_DECL
            ? enum_readiness(inventory, emitted, header)
            : class_readiness(inventory, emitted, header);
        name = mir_decl_header_name(header);
        if (readiness == TYPE_DECL_INVALID) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR type declaration '%s' has a missing dependency type fact",
                name != NULL ? name : "(anonymous-type)");
            return false;
        }
        if (readiness == TYPE_DECL_WAITING)
            continue;
        if (type == AST_ENUM_DECL) {
            ASTNode *node = find_enum_node(types, type_count, name);
            if (node == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR enum declaration '%s' has no source identity carrier",
                    name != NULL ? name : "(anonymous-enum)");
                return false;
            }
            emit_enum_decl_stmt(node, ctx);
        } else {
            emit_class_decl_from_mir_header(header, ctx);
        }
        if (ctx->backend_error != NULL)
            return false;
        emitted[i] = true;
        *remaining -= 1;
        *made_progress = true;
    }
    return true;
}

bool
transpiler_emit_mir_type_declarations(TranspilerCtx *ctx,
                                      ASTNode **types,
                                      size_t type_count)
{
    MIRDeclHeaderInventory inventory;
    bool *emitted;
    size_t remaining = 0;
    size_t source_value_count = 0;
    bool ok = true;

    if (ctx == NULL)
        return false;
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *node = types != NULL ? types[i] : NULL;

        if (node != NULL
            && (node->type == AST_CLASS_DECL
                || node->type == AST_ENUM_DECL)) {
            source_value_count += 1;
        }
    }
    transpiler_active_decl_header_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        if (is_value_type_header(
                mir_decl_header_inventory_get(&inventory, i))) {
            remaining += 1;
        }
    }
    if (remaining != source_value_count) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR type declaration inventory does not match source identity carriers");
        return false;
    }
    emitted = calloc(inventory.count > 0 ? inventory.count : 1,
                     sizeof(*emitted));
    if (emitted == NULL) {
        transpiler_set_backend_error(ctx,
            "C backend: type declaration schedule allocation failed");
        return false;
    }
    while (remaining > 0) {
        bool made_progress = false;

        if (!emit_ready_kind(ctx, types, type_count, &inventory, emitted,
                AST_ENUM_DECL, &remaining, &made_progress)
            || !emit_ready_kind(ctx, types, type_count, &inventory, emitted,
                AST_CLASS_DECL, &remaining, &made_progress)) {
            ok = false;
            break;
        }
        if (!made_progress) {
            transpiler_set_backend_error(ctx,
                "C backend: cyclic by-value type declaration dependency");
            ok = false;
            break;
        }
    }
    free(emitted);
    return ok;
}
