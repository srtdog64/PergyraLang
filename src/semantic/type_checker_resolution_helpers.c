#include <stdio.h>
#include <string.h>

#include "type_checker_resolution_helpers.h"
#include "diag_codes.h"

ASTNode *
find_type_alias_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_TYPE_ALIAS
            || stmt->data.type_alias.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.type_alias.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static Type *
resolve_named_type_from_metadata(const char *name,
                                 SemanticContext *ctx,
                                 const ASTNode *site)
{
    if (name == NULL)
        return NULL;

    Type *resolved = semantic_type_resolution_lookup_metadata_name_or_alias(ctx,
                                                                            name);
    if (resolved == NULL)
        return NULL;
    if (resolved == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

    Type *builtin = semantic_type_resolution_metadata_builtin_singleton(name);
    if (builtin != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, name,
            "metadata builtin-type lookup");
        return resolved;
    }

    ASTNode *alias_decl = ctx != NULL && ctx->program_root != NULL
        ? find_type_alias_decl(ctx->program_root, name)
        : NULL;
    if (alias_decl != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_ALIAS, alias_decl, name,
            "metadata type-alias lookup");
        return resolved;
    }

    Symbol *sym = ctx != NULL ? scope_lookup(ctx->scope, name) : NULL;
    if (sym != NULL && sym->kind == SYMBOL_CLASS) {
        ASTNode *decl = ctx->program_root != NULL
            ? find_type_decl_by_name(ctx->program_root, name)
            : NULL;
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_DECL, decl, name,
            "metadata named-type lookup");
        return resolved;
    }
    if (sym != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name,
            sym->kind == SYMBOL_TYPE_PARAM
                ? TYPE_RES_NODE_GENERIC_PARAM
                : TYPE_RES_NODE_DECL,
            NULL, name, "metadata scope-type lookup");
        return resolved;
    }

    return resolved;
}

Type *
resolve_named_type(const char *name, SemanticContext *ctx, const ASTNode *site)
{
    Type *metadata_type = resolve_named_type_from_metadata(name, ctx, site);
    if (metadata_type != NULL)
        return metadata_type;

    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL && sym->kind == SYMBOL_CLASS && sym->type != TYPE_UNKNOWN) {
        ASTNode *decl = (ctx != NULL && ctx->program_root != NULL)
            ? find_type_decl_by_name(ctx->program_root, name)
            : NULL;
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_DECL, decl, name,
            "named-type lookup");
        return sym->type;
    }

    if (strcmp(name, "Int") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Int", "builtin-type lookup");
        return TYPE_INT;
    }
    if (strcmp(name, "Long") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Long", "builtin-type lookup");
        return TYPE_LONG;
    }
    if (strcmp(name, "Float") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Float", "builtin-type lookup");
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Double") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Double", "builtin-type lookup");
        return TYPE_DOUBLE;
    }
    if (strcmp(name, "Bool") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Bool", "builtin-type lookup");
        return TYPE_BOOL;
    }
    if (strcmp(name, "String") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "String", "builtin-type lookup");
        return TYPE_STRING;
    }
    if (strcmp(name, "QubitSlot") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "QubitSlot", "builtin-type lookup");
        return TYPE_QUBIT;
    }
    if (strcmp(name, "Void") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Void", "builtin-type lookup");
        return TYPE_VOID;
    }
    if (strcmp(name, "Array") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Array", "builtin-type lookup");
        return TYPE_ARRAY;
    }
    if (strcmp(name, "Slice") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Slice", "builtin-type lookup");
        return TYPE_SLICE;
    }
    if (strcmp(name, "List") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "List", "builtin-type lookup");
        return TYPE_LIST;
    }
    if (strcmp(name, "Queue") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Queue", "builtin-type lookup");
        return TYPE_QUEUE;
    }
    if (strcmp(name, "HashMap") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "HashMap", "builtin-type lookup");
        return TYPE_HASHMAP;
    }
    if (strcmp(name, "Set") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Set", "builtin-type lookup");
        return TYPE_SET;
    }
    if (strcmp(name, "Box") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Box", "builtin-type lookup");
        return TYPE_BOX;
    }
    if (strcmp(name, "Rc") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Rc", "builtin-type lookup");
        return TYPE_RC;
    }
    if (strcmp(name, "Weak") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Weak", "builtin-type lookup");
        return TYPE_WEAK;
    }
    if (strcmp(name, "RemoteFuture") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "RemoteFuture", "builtin-type lookup");
        return TYPE_REMOTE_FUTURE;
    }
    if (strcmp(name, "DeviceSlot") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "DeviceSlot", "builtin-type lookup");
        return TYPE_DEVICE_SLOT;
    }
    if (strcmp(name, "Allocator") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Allocator", "builtin-type lookup");
        return TYPE_ALLOCATOR;
    }
    if (strcmp(name, "Option") == 0) {
        semantic_type_resolution_record_named_dependency(ctx, site, name,
            TYPE_RES_NODE_BUILTIN, NULL, "Option", "builtin-type lookup");
        return TYPE_OPTION;
    }

    if (sym != NULL && sym->type != TYPE_UNKNOWN) {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name,
            sym->kind == SYMBOL_TYPE_PARAM
                ? TYPE_RES_NODE_GENERIC_PARAM
                : TYPE_RES_NODE_DECL,
            NULL, name, "scope-type lookup");
        return sym->type;
    }

    if (ctx != NULL && ctx->program_root != NULL) {
        ASTNode *alias_decl = find_type_alias_decl(ctx->program_root, name);
        if (alias_decl != NULL) {
            semantic_type_resolution_record_named_dependency(
                ctx, site, name, TYPE_RES_NODE_ALIAS, alias_decl, name,
                "type-alias lookup");
            Type *resolved = semantic_type_resolution_lookup_metadata_name_or_alias(
                ctx, name);
            if (sym != NULL && resolved != NULL)
                sym->type = resolved;
            return resolved != NULL ? resolved : TYPE_UNKNOWN;
        }
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_IMPORT_OR_DECLARE_TYPE, site,
        "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

bool
name_looks_qualified(const char *name)
{
    return name != NULL && strchr(name, '.') != NULL;
}

const char *
semantic_symbol_kind_label(SymbolKind kind)
{
    switch (kind) {
    case SYMBOL_VARIABLE: return "variable";
    case SYMBOL_FUNCTION: return "function";
    case SYMBOL_CLASS: return "class";
    case SYMBOL_TYPE_PARAM: return "type parameter";
    case SYMBOL_SLOT: return "slot";
    case SYMBOL_TOKEN: return "token";
    case SYMBOL_ABILITY: return "ability";
    case SYMBOL_ROLE: return "role";
    case SYMBOL_PARTY: return "party";
    case SYMBOL_ROSTER: return "roster";
    case SYMBOL_WORLD: return "world";
    case SYMBOL_INTENT: return "intent";
    case SYMBOL_RELATION: return "relation";
    case SYMBOL_EFFECT: return "effect";
    case SYMBOL_ZONE: return "zone";
    default: return "symbol";
    }
}

const char *
type_name_or_unknown(const Type *type)
{
    if (type == NULL)
        return "<unknown>";
    if (type->name != NULL && type->name[0] != '\0')
        return type->name;
    return "<unknown>";
}

void
semantic_format_function_signature(const Type *type, char *out, size_t out_cap)
{
    size_t pos = 0;
    if (out == NULL || out_cap == 0)
        return;

    out[0] = '\0';
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;

    pos = snprintf(out, out_cap, "fn(");
    for (size_t i = 0; i < type->data.function.param_count; i++) {
        const char *param_name =
            type_name_or_unknown(type->data.function.param_types[i]);
        if (i > 0)
            pos += snprintf(out + pos, out_cap > pos ? out_cap - pos : 0, ", ");
        pos += snprintf(out + pos, out_cap > pos ? out_cap - pos : 0, "%s",
                        param_name);
        if (pos >= out_cap)
            break;
    }
    if (pos < out_cap) {
        snprintf(out + pos, out_cap > pos ? out_cap - pos : 0,
                 ")->%s",
                 type_name_or_unknown(type->data.function.return_type));
    }
}

static const char *
root_identifier_name(ASTNode *expr)
{
    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return expr->data.identifier.name;
    case AST_MEMBER_ACCESS:
        return root_identifier_name(expr->data.member.object);
    case AST_ARRAY_ACCESS:
        return root_identifier_name(expr->data.array_access.array);
    default:
        return NULL;
    }
}

static size_t
embedded_world_zone_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++) {
        if (ctx->embedded_world_zone_names[i] != NULL
            && strcmp(ctx->embedded_world_zone_names[i], name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static bool
has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return embedded_world_zone_index(ctx, name) != (size_t)-1;
}

static const char *
embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

static const char *
embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

void
reject_if_embedded_world_zone_mutation(SemanticContext *ctx, ASTNode *site,
                                       ASTNode *target, const char *op_name)
{
    if (ctx == NULL || site == NULL || target == NULL || op_name == NULL)
        return;
    if (ctx->current_world != NULL)
        return;

    const char *root_name = root_identifier_name(target);
    if (root_name == NULL)
        return;

    Symbol *root_sym = scope_lookup(ctx->scope, root_name);
    if ((root_sym == NULL || !root_sym->embedded_in_world)
        && !has_embedded_world_zone_name(ctx, root_name)) {
        return;
    }

    const char *owner_world = embedded_world_zone_world_name(ctx, root_name);
    const char *owner_slot = embedded_world_zone_slot_name(ctx, root_name);

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
        PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
        "Zone '%s' cannot be mutated via %s after it was embedded into world '%s' slot '%s'.\n"
        "Reason:\n"
        "- origin binding is '%s'\n"
        "Contract source:\n"
        "- world '%s' zone slot '%s'\n"
        "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
        "- derived embedding provenance points to world '%s' slot '%s'\n"
        "- ownership/authority now flows through the world-owned slot rather than the old local binding\n"
        "- owned embedding hands authority-bearing visibility to the world-owned slot\n"
        "- mutating the old binding would diverge from the world-owned handoff destination\n"
        "Fix:\n"
        "- finish configuring '%s' before constructing world '%s'\n"
        "- or mutate it through world '%s' slot '%s' after embedding",
        root_name, op_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>");
}
