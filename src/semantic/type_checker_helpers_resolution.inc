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

static int
semantic_ctx_alias_resolution_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < ctx->alias_resolution_count; i++) {
        if (ctx->alias_resolution_stack[i] != NULL
            && strcmp(ctx->alias_resolution_stack[i], name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
semantic_ctx_alias_resolution_push(SemanticContext *ctx, const char *name)
{
    char **grown;

    if (ctx == NULL || name == NULL)
        return false;

    if (ctx->alias_resolution_count >= ctx->alias_resolution_capacity) {
        size_t new_cap = ctx->alias_resolution_capacity == 0
            ? 8 : ctx->alias_resolution_capacity * 2;
        grown = realloc(ctx->alias_resolution_stack, new_cap * sizeof(char *));
        if (grown == NULL)
            return false;
        ctx->alias_resolution_stack = grown;
        ctx->alias_resolution_capacity = new_cap;
    }

    ctx->alias_resolution_stack[ctx->alias_resolution_count] = pergyra_strdup(name);
    if (ctx->alias_resolution_stack[ctx->alias_resolution_count] == NULL)
        return false;
    ctx->alias_resolution_count++;
    return true;
}

static void
semantic_ctx_alias_resolution_pop(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->alias_resolution_count == 0)
        return;

    free(ctx->alias_resolution_stack[ctx->alias_resolution_count - 1]);
    ctx->alias_resolution_stack[ctx->alias_resolution_count - 1] = NULL;
    ctx->alias_resolution_count--;
}

static Type *
resolution_helpers_resolve_type_ref(SemanticContext *ctx, ASTNode *type_ref)
{
    return semantic_type_resolution_resolve_or_fallback(ctx, type_ref);
}

static char *
semantic_ctx_alias_resolution_cycle_text(SemanticContext *ctx,
                                         size_t start_index,
                                         const char *closing_name)
{
    char *result = NULL;

    if (ctx == NULL || start_index >= ctx->alias_resolution_count)
        return tc_strdup_fmt("<cycle>");

    for (size_t i = start_index; i < ctx->alias_resolution_count; i++) {
        const char *label = ctx->alias_resolution_stack[i] != NULL
            ? ctx->alias_resolution_stack[i] : "<alias>";
        char *next = result == NULL
            ? tc_strdup_fmt("%s", label)
            : tc_strdup_fmt("%s -> %s", result, label);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("<cycle>");
    }

    {
        char *next = tc_strdup_fmt("%s -> %s",
                                   result != NULL ? result : "<cycle>",
                                   closing_name != NULL ? closing_name : "<alias>");
        free(result);
        result = next;
    }

    return result != NULL ? result : tc_strdup_fmt("<cycle>");
}

static Type *
resolve_type_alias_decl(ASTNode *alias_decl, SemanticContext *ctx, unsigned depth)
{
    const char *alias_name;
    int cycle_index;

    if (alias_decl == NULL || alias_decl->type != AST_TYPE_ALIAS)
        return TYPE_UNKNOWN;

    alias_name = alias_decl->data.type_alias.name != NULL
        ? alias_decl->data.type_alias.name : "<alias>";

    cycle_index = semantic_ctx_alias_resolution_index(ctx, alias_name);
    if (cycle_index >= 0) {
        char *cycle_text = semantic_ctx_alias_resolution_cycle_text(
            ctx, (size_t)cycle_index, alias_name);
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE, PGY_CAUSE_TYPE_RESOLUTION_CYCLE, PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION, alias_decl,
            "Type resolution dependency cycle detected around alias '%s'.\n"
            "Contract source:\n"
            "- alias chain provenance: %s\n"
            "Reason:\n"
            "- alias resolution re-entered '%s' before the current alias chain completed\n"
            "- cycle path: %s\n"
            "Fix:\n"
            "- break the alias loop so one alias resolves to a concrete type first\n"
            "- or replace one side of the cycle with a non-alias type declaration",
            alias_name,
            cycle_text != NULL ? cycle_text : "<cycle>",
            alias_name,
            cycle_text != NULL ? cycle_text : "<cycle>");
        free(cycle_text);
        return TYPE_UNKNOWN;
    }

    if (depth > 32) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE, PGY_CAUSE_TYPE_RESOLUTION_CYCLE, PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION, alias_decl,
            "Type alias resolution exceeded maximum depth near '%s'.\n"
            "Contract source:\n"
            "- alias chain provenance: %s\n"
            "Reason:\n"
            "- alias materialization exceeded the beta maximum alias depth\n"
            "- this usually means a dependency cycle escaped the graph prepass\n"
            "Fix:\n"
            "- break the alias chain with a concrete type declaration\n"
            "- or split the recursive shape behind an explicit indirection",
            alias_decl->data.type_alias.name != NULL
                ? alias_decl->data.type_alias.name : "<alias>",
            alias_decl->data.type_alias.name != NULL
                ? alias_decl->data.type_alias.name : "<alias>");
        return TYPE_UNKNOWN;
    }

    if (alias_decl->data.type_alias.target_type == NULL)
        return TYPE_UNKNOWN;

    semantic_type_resolution_record_type_ref_dependency(
        ctx,
        alias_decl,
        alias_decl->data.type_alias.name != NULL
            ? alias_decl->data.type_alias.name : "<alias>",
        alias_decl->data.type_alias.target_type,
        "type-alias target lookup");

    if (!semantic_ctx_alias_resolution_push(ctx, alias_name)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
            PGY_CAUSE_RESOLUTION_OOM, PGY_FIX_REDUCE_SCOPE_OR_RETRY,
            alias_decl,
            "Out of memory while tracking type alias resolution for '%s'.",
            alias_name);
        return TYPE_UNKNOWN;
    }

    {
        Type *resolved = resolution_helpers_resolve_type_ref(
            ctx, alias_decl->data.type_alias.target_type);
        semantic_ctx_alias_resolution_pop(ctx);
        return resolved;
    }
}

ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name);

Type *
resolve_named_type(const char *name, SemanticContext *ctx, const ASTNode *site)
{
    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL && sym->kind == SYMBOL_CLASS && sym->type != TYPE_UNKNOWN)
    {
        ASTNode *decl = (ctx != NULL && ctx->program_root != NULL)
            ? find_type_decl_by_name(ctx->program_root, name)
            : NULL;
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_DECL, decl, name, "named-type lookup");
        return sym->type;
    }

    if (strcmp(name, "Int")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Int", "builtin-type lookup"); return TYPE_INT; }
    if (strcmp(name, "Long")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Long", "builtin-type lookup"); return TYPE_LONG; }
    if (strcmp(name, "Float")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Float", "builtin-type lookup"); return TYPE_FLOAT; }
    if (strcmp(name, "Double") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Double", "builtin-type lookup"); return TYPE_DOUBLE; }
    if (strcmp(name, "Bool")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Bool", "builtin-type lookup"); return TYPE_BOOL; }
    if (strcmp(name, "String") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "String", "builtin-type lookup"); return TYPE_STRING; }
    if (strcmp(name, "QubitSlot") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "QubitSlot", "builtin-type lookup"); return TYPE_QUBIT; }
    if (strcmp(name, "Void")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Void", "builtin-type lookup"); return TYPE_VOID; }
    if (strcmp(name, "Array")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Array", "builtin-type lookup"); return TYPE_ARRAY; }
    if (strcmp(name, "Slice")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Slice", "builtin-type lookup"); return TYPE_SLICE; }
    if (strcmp(name, "List")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "List", "builtin-type lookup"); return TYPE_LIST; }
    if (strcmp(name, "Queue")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Queue", "builtin-type lookup"); return TYPE_QUEUE; }
    if (strcmp(name, "HashMap")== 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "HashMap", "builtin-type lookup"); return TYPE_HASHMAP; }
    if (strcmp(name, "Set")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Set", "builtin-type lookup"); return TYPE_SET; }
    if (strcmp(name, "Box")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Box", "builtin-type lookup"); return TYPE_BOX; }
    if (strcmp(name, "Rc")     == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Rc", "builtin-type lookup"); return TYPE_RC; }
    if (strcmp(name, "Weak")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Weak", "builtin-type lookup"); return TYPE_WEAK; }
    if (strcmp(name, "RemoteFuture") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "RemoteFuture", "builtin-type lookup"); return TYPE_REMOTE_FUTURE; }
    if (strcmp(name, "DeviceSlot") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "DeviceSlot", "builtin-type lookup"); return TYPE_DEVICE_SLOT; }
    if (strcmp(name, "Allocator") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Allocator", "builtin-type lookup"); return TYPE_ALLOCATOR; }
    if (strcmp(name, "Option") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Option", "builtin-type lookup"); return TYPE_OPTION; }

    if (sym != NULL && sym->type != TYPE_UNKNOWN)
    {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name,
            sym->kind == SYMBOL_TYPE_PARAM ? TYPE_RES_NODE_GENERIC_PARAM : TYPE_RES_NODE_DECL,
            NULL, name, "scope-type lookup");
        return sym->type;
    }

    if (ctx != NULL && ctx->program_root != NULL) {
        ASTNode *alias_decl = find_type_alias_decl(ctx->program_root, name);
        if (alias_decl != NULL) {
            semantic_type_resolution_record_named_dependency(
                ctx, site, name, TYPE_RES_NODE_ALIAS, alias_decl, name, "type-alias lookup");
            Type *resolved = resolve_type_alias_decl(alias_decl, ctx, 0);
            if (sym != NULL && resolved != NULL)
                sym->type = resolved;
            return resolved != NULL ? resolved : TYPE_UNKNOWN;
        }
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE, PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_IMPORT_OR_DECLARE_TYPE, site,
                        "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

static Type *
resolve_generic_type_arg(GenericParam *gp, SemanticContext *ctx,
                         const ASTNode *site)
{
    if (gp == NULL)
        return TYPE_UNKNOWN;
    if (gp->constraint != NULL)
        return resolution_helpers_resolve_type_ref(ctx, gp->constraint);
    return resolve_named_type(gp->name, ctx, site);
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

    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_GENERIC:
    case TYPE_KIND_CONSTRUCTED:
    case TYPE_KIND_TUPLE:
    case TYPE_KIND_SLOT:
    case TYPE_KIND_CLASS:
    case TYPE_KIND_ENUM:
    case TYPE_KIND_TRAIT:
    case TYPE_KIND_ALIAS:
    case TYPE_KIND_FUNCTION:
        return "<unknown>";
    default:
        return "<unknown>";
    }
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
    if (pos < out_cap)
        snprintf(out + pos, out_cap > pos ? out_cap - pos : 0,
                 ")->%s",
                 type_name_or_unknown(type->data.function.return_type));
}

ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name);
ASTNode *
find_domain_decl_by_name(ASTNode *program, ASTNodeType decl_type, const char *name);

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
static void
reject_if_embedded_world_zone_mutation(SemanticContext *ctx, ASTNode *site,
                                       ASTNode *target, const char *op_name)
{
    const char *root_name;
    const char *owner_world;
    const char *owner_slot;
    Symbol *root_sym;

    if (ctx == NULL || site == NULL || target == NULL || op_name == NULL)
        return;
    if (ctx->current_world != NULL)
        return;

    root_name = root_identifier_name(target);
    if (root_name == NULL)
        return;

    root_sym = scope_lookup(ctx->scope, root_name);
    if ((root_sym == NULL || !root_sym->embedded_in_world)
        && !semantic_ctx_has_embedded_world_zone_name(ctx, root_name)) {
        return;
    }

    owner_world = semantic_ctx_embedded_world_zone_world_name(ctx, root_name);
    owner_slot = semantic_ctx_embedded_world_zone_slot_name(ctx, root_name);

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
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
