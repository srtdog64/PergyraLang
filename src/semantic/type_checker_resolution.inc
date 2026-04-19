static void
validate_where_clause_bounds(WhereClause *wc, SemanticContext *ctx, ASTNode *owner)
{
    if (wc == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < wc->count; i++) {
        TypeConstraint *tc = wc->constraints[i];
        if (tc == NULL)
            continue;
        for (size_t b = 0; b < tc->bound_count; b++) {
            if (tc->bounds[b] != NULL) {
                Symbol *bound_sym = NULL;
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    tc->bounds[b],
                    "where-bound lookup");
                size_t saved_diag = ctx->diagnostic_count;
                bool saved_err = ctx->has_error;
                Type *bound_type = resolve_type_node(tc->bounds[b], ctx);
                if (tc->bounds[b]->type == AST_TYPE
                    && tc->bounds[b]->data.type.name != NULL) {
                    bound_sym = scope_lookup(ctx->scope,
                                             tc->bounds[b]->data.type.name);
                }
                if (ctx->diagnostic_count > saved_diag
                    || bound_type == NULL
                    || bound_type == TYPE_UNKNOWN) {
                    ctx->diagnostic_count = saved_diag;
                    ctx->has_error = saved_err;
                    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
                        || (tc->bounds[b]->type == AST_TYPE
                            && tc->bounds[b]->data.type.name != NULL
                            && ctx->program_root != NULL
                            && find_ability_decl_by_name(
                                   ctx->program_root,
                                   tc->bounds[b]->data.type.name) != NULL)) {
                        continue;
                    }
                    semantic_error(ctx, owner != NULL ? owner : tc->bounds[b],
                        "Unknown constraint type '%s' in where clause.\n"
                        "Reason:\n"
                        "- generic where-clause validation could not resolve this bound\n"
                        "- every bound in a multi-bound contract must resolve before specialization can be trusted\n"
                        "Fix:\n"
                        "- declare or import '%s'\n"
                        "- or remove the unresolved bound from the where-clause",
                        tc->bounds[b]->data.type.name,
                        tc->bounds[b]->data.type.name);
                }
            }
        }
    }
}

static void
validate_generic_param_defaults(GenericParams *gp, SemanticContext *ctx,
                                ASTNode *owner, const char *kind_name)
{
    bool saw_default = false;

    if (gp == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *param = gp->params[i];
        if (param == NULL)
            continue;
        if (param->default_type != NULL) {
            saw_default = true;
        } else if (saw_default) {
            semantic_error(ctx, owner != NULL ? owner : (ASTNode *)gp,
                "Non-trailing default generic parameter '%s' in %s declaration.\n"
                "Reason:\n"
                "- a required generic parameter appears after a parameter with a default\n"
                "- generic defaults are only closed for trailing parameters\n"
                "Fix:\n"
                "- move '%s' before all defaulted generic parameters\n"
                "- or give '%s' a default type argument too",
                param->name != NULL ? param->name : "<type-param>",
                kind_name != NULL ? kind_name : "generic",
                param->name != NULL ? param->name : "<type-param>",
                param->name != NULL ? param->name : "<type-param>");
        }
        if (param == NULL || param->default_type == NULL)
            continue;
        {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                owner != NULL ? owner : param->default_type,
                param->name != NULL ? param->name : "<type-param>",
                param->default_type,
                "default-type lookup");
            size_t saved_diag = ctx->diagnostic_count;
            bool saved_err = ctx->has_error;
            Type *resolved = resolve_type_node(param->default_type, ctx);
            if (resolved == NULL || resolved == TYPE_UNKNOWN
                || ctx->diagnostic_count > saved_diag) {
                ctx->diagnostic_count = saved_diag;
                ctx->has_error = saved_err;
                semantic_error(ctx, owner != NULL ? owner : param->default_type,
                    "Invalid default generic type argument '%s' in %s declaration (parameter '%s').\n"
                    "Reason:\n"
                    "- the declared default type could not be resolved as a valid concrete type\n"
                    "- generic defaults must be fully valid before they can participate in effective-argument derivation\n"
                    "Fix:\n"
                    "- replace '%s' with a resolvable concrete type\n"
                    "- or remove the default and require the caller to supply it",
                    param->default_type->type == AST_TYPE
                        && param->default_type->data.type.name != NULL
                            ? param->default_type->data.type.name
                            : "<type>",
                    kind_name != NULL ? kind_name : "generic",
                    param->name != NULL ? param->name : "<type-param>",
                    param->default_type->type == AST_TYPE
                        && param->default_type->data.type.name != NULL
                            ? param->default_type->data.type.name
                            : "<type>");
            }
        }
    }
}

static char *
format_type_constraint_bounds(TypeConstraint *tc)
{
    char *result = NULL;

    if (tc == NULL || tc->bound_count == 0)
        return tc_strdup_fmt("<constraint>");

    for (size_t i = 0; i < tc->bound_count; i++) {
        ASTNode *bound = tc->bounds[i];
        const char *bound_name =
            (bound != NULL
             && bound->type == AST_TYPE
             && bound->data.type.name != NULL)
                ? bound->data.type.name
                : "<constraint>";
        char *next;

        if (result == NULL) {
            result = tc_strdup_fmt("%s", bound_name);
        } else {
            next = tc_strdup_fmt("%s + %s", result, bound_name);
            free(result);
            result = next;
        }

        if (result == NULL)
            return tc_strdup_fmt("<constraint>");
    }

    return result;
}

static bool
type_resolution_labels_equal(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    return strcmp(lhs, rhs) == 0;
}

static size_t
type_resolution_intern_node(TypeResolutionGraph *graph,
                            TypeResolutionNodeKind kind,
                            const ASTNode *site,
                            const char *label)
{
    TypeResolutionNode *grown;

    if (graph == NULL)
        return (size_t)-1;

    for (size_t i = 0; i < graph->node_count; i++) {
        TypeResolutionNode *node = &graph->nodes[i];
        if (node->kind == kind
            && node->site == site
            && type_resolution_labels_equal(node->label, label)) {
            return i;
        }
    }

    if (graph->node_count >= graph->node_capacity) {
        size_t new_cap = graph->node_capacity == 0 ? 16 : graph->node_capacity * 2;
        grown = realloc(graph->nodes, new_cap * sizeof(TypeResolutionNode));
        if (grown == NULL)
            return (size_t)-1;
        graph->nodes = grown;
        graph->node_capacity = new_cap;
    }

    graph->nodes[graph->node_count].kind = kind;
    graph->nodes[graph->node_count].site = site;
    graph->nodes[graph->node_count].label =
        label != NULL ? pergyra_strdup(label) : NULL;
    return graph->node_count++;
}

static void
type_resolution_add_edge(TypeResolutionGraph *graph,
                         size_t from,
                         size_t to,
                         const char *reason)
{
    TypeResolutionEdge *grown;

    if (graph == NULL || from == (size_t)-1 || to == (size_t)-1)
        return;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from == from
            && edge->to == to
            && type_resolution_labels_equal(edge->reason, reason)) {
            return;
        }
    }

    if (graph->edge_count >= graph->edge_capacity) {
        size_t new_cap = graph->edge_capacity == 0 ? 32 : graph->edge_capacity * 2;
        grown = realloc(graph->edges, new_cap * sizeof(TypeResolutionEdge));
        if (grown == NULL)
            return;
        graph->edges = grown;
        graph->edge_capacity = new_cap;
    }

    graph->edges[graph->edge_count].from = from;
    graph->edges[graph->edge_count].to = to;
    graph->edges[graph->edge_count].reason =
        reason != NULL ? pergyra_strdup(reason) : NULL;
    graph->edge_count++;
}

static bool
type_resolution_find_path(TypeResolutionGraph *graph,
                          size_t current,
                          size_t goal,
                          bool *visited,
                          size_t *path,
                          size_t *path_len,
                          size_t path_cap)
{
    if (graph == NULL || visited == NULL || path == NULL || path_len == NULL)
        return false;
    if (current >= graph->node_count || goal >= graph->node_count)
        return false;
    if (*path_len >= path_cap)
        return false;
    if (visited[current])
        return false;

    visited[current] = true;
    path[(*path_len)++] = current;
    if (current == goal)
        return true;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from != current)
            continue;
        if (type_resolution_find_path(graph,
                                      edge->to,
                                      goal,
                                      visited,
                                      path,
                                      path_len,
                                      path_cap)) {
            return true;
        }
    }

    if (*path_len > 0)
        (*path_len)--;
    return false;
}

static char *
type_resolution_edge_reason(TypeResolutionGraph *graph,
                            size_t from,
                            size_t to)
{
    if (graph == NULL)
        return NULL;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from == from && edge->to == to)
            return edge->reason;
    }

    return NULL;
}

static char *
type_resolution_format_cycle(TypeResolutionGraph *graph,
                             size_t *path,
                             size_t path_len,
                             size_t closing_node)
{
    char *result = NULL;

    if (graph == NULL || path == NULL || path_len == 0)
        return tc_strdup_fmt("<cycle>");

    for (size_t i = 0; i < path_len; i++) {
        const char *label = graph->nodes[path[i]].label != NULL
            ? graph->nodes[path[i]].label
            : "<node>";
        char *next;
        if (result == NULL) {
            next = tc_strdup_fmt("%s", label);
        } else {
            const char *reason = type_resolution_edge_reason(
                graph, path[i - 1], path[i]);
            next = tc_strdup_fmt("%s -[%s]-> %s",
                                 result,
                                 reason != NULL ? reason : "dependency",
                                 label);
        }
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("<cycle>");
    }

    {
        const char *closing = graph->nodes[closing_node].label != NULL
            ? graph->nodes[closing_node].label
            : "<node>";
        const char *reason = path_len > 0
            ? type_resolution_edge_reason(graph, path[path_len - 1], closing_node)
            : NULL;
        char *next = tc_strdup_fmt("%s -[%s]-> %s",
                                   result != NULL ? result : "<cycle>",
                                   reason != NULL ? reason : "dependency",
                                   closing);
        free(result);
        result = next;
    }

    return result != NULL ? result : tc_strdup_fmt("<cycle>");
}

static bool
type_resolution_find_cycle_visit(TypeResolutionGraph *graph,
                                 size_t current,
                                 unsigned char *color,
                                 size_t *stack,
                                 size_t *stack_len,
                                 size_t *cycle_path,
                                 size_t *cycle_len,
                                 size_t cycle_cap,
                                 size_t *closing_node)
{
    if (graph == NULL || color == NULL || stack == NULL || stack_len == NULL
        || cycle_path == NULL || cycle_len == NULL || closing_node == NULL) {
        return false;
    }

    color[current] = 1;
    stack[(*stack_len)++] = current;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from != current)
            continue;

        if (color[edge->to] == 0) {
            if (type_resolution_find_cycle_visit(graph,
                                                 edge->to,
                                                 color,
                                                 stack,
                                                 stack_len,
                                                 cycle_path,
                                                 cycle_len,
                                                 cycle_cap,
                                                 closing_node)) {
                return true;
            }
        } else if (color[edge->to] == 1) {
            size_t start = 0;
            while (start < *stack_len && stack[start] != edge->to)
                start++;
            *cycle_len = 0;
            for (size_t j = start; j < *stack_len && *cycle_len < cycle_cap; j++)
                cycle_path[(*cycle_len)++] = stack[j];
            *closing_node = edge->to;
            return true;
        }
    }

    if (*stack_len > 0)
        (*stack_len)--;
    color[current] = 2;
    return false;
}

static bool
type_resolution_validate_graph(SemanticContext *ctx)
{
    TypeResolutionGraph *graph;
    unsigned char *color = NULL;
    size_t *stack = NULL;
    size_t *cycle_path = NULL;
    size_t stack_len = 0;
    size_t cycle_len = 0;
    size_t closing_node = (size_t)-1;
    bool ok = true;

    if (ctx == NULL)
        return false;

    graph = &ctx->type_resolution_graph;
    if (graph->node_count == 0)
        return true;

    color = calloc(graph->node_count, sizeof(unsigned char));
    stack = calloc(graph->node_count, sizeof(size_t));
    cycle_path = calloc(graph->node_count, sizeof(size_t));
    if (color == NULL || stack == NULL || cycle_path == NULL) {
        free(color);
        free(stack);
        free(cycle_path);
        return false;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (color[i] != 0)
            continue;
        stack_len = 0;
        cycle_len = 0;
        closing_node = (size_t)-1;
        if (type_resolution_find_cycle_visit(graph,
                                             i,
                                             color,
                                             stack,
                                             &stack_len,
                                             cycle_path,
                                             &cycle_len,
                                             graph->node_count,
                                             &closing_node)) {
            ASTNode *site = (ASTNode *)graph->nodes[closing_node].site;
            char *cycle_text = type_resolution_format_cycle(
                graph, cycle_path, cycle_len, closing_node);
            semantic_error(ctx, site,
                "Type resolution dependency cycle detected in the semantic graph around '%s'.\n"
                "Reason:\n"
                "- provider/consumer resolution formed a closed dependency loop\n"
                "- cycle path: %s\n"
                "Fix:\n"
                "- break the generic/alias/ability dependency loop so one edge resolves first\n"
                "- or split the contract into acyclic declarations",
                graph->nodes[closing_node].label != NULL
                    ? graph->nodes[closing_node].label : "<type-ref>",
                cycle_text != NULL ? cycle_text : "<cycle>");
            free(cycle_text);
            ok = false;
            break;
        }
    }

    free(color);
    free(stack);
    free(cycle_path);
    return ok;
}

static bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count)
{
    size_t *indegree = NULL;
    size_t *queue = NULL;
    size_t *order = NULL;
    size_t head = 0;
    size_t tail = 0;
    size_t produced = 0;

    if (out_order != NULL)
        *out_order = NULL;
    if (out_count != NULL)
        *out_count = 0;
    if (graph == NULL)
        return false;
    if (graph->node_count == 0)
        return true;

    indegree = calloc(graph->node_count, sizeof(size_t));
    queue = calloc(graph->node_count, sizeof(size_t));
    order = calloc(graph->node_count, sizeof(size_t));
    if (indegree == NULL || queue == NULL || order == NULL) {
        free(indegree);
        free(queue);
        free(order);
        return false;
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        if (graph->edges[i].to < graph->node_count)
            indegree[graph->edges[i].to]++;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (indegree[i] == 0)
            queue[tail++] = i;
    }

    while (head < tail) {
        size_t node = queue[head++];
        order[produced++] = node;
        for (size_t i = 0; i < graph->edge_count; i++) {
            TypeResolutionEdge *edge = &graph->edges[i];
            if (edge->from != node || edge->to >= graph->node_count)
                continue;
            if (indegree[edge->to] > 0)
                indegree[edge->to]--;
            if (indegree[edge->to] == 0)
                queue[tail++] = edge->to;
        }
    }

    free(indegree);
    free(queue);

    if (produced != graph->node_count) {
        free(order);
        return false;
    }

    if (out_order != NULL)
        *out_order = order;
    else
        free(order);
    if (out_count != NULL)
        *out_count = produced;
    return true;
}

static void
semantic_type_resolution_collect_type_refs(ASTNode *type_node,
                                           SemanticContext *ctx,
                                           const ASTNode *consumer_site,
                                           const char *consumer_name,
                                           const char *reason)
{
    if (type_node == NULL || ctx == NULL || consumer_name == NULL)
        return;

    switch (type_node->type) {
    case AST_CHANNEL_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.channel_type.element_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_FUTURE_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.future_type.value_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_EVENT_HANDLER_TYPE:
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            semantic_type_resolution_collect_type_refs(
                type_node->data.event_handler_type.param_types[i],
                ctx,
                consumer_site,
                consumer_name,
                reason);
        }
        semantic_type_resolution_collect_type_refs(
            type_node->data.event_handler_type.return_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_TYPE:
        if (type_node->data.type.name != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                consumer_site != NULL ? consumer_site : type_node,
                consumer_name,
                type_node,
                reason != NULL ? reason : "type dependency");
        }
        if (type_node->data.type.generic_args != NULL) {
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *gp = type_node->data.type.generic_args->params[i];
                if (gp != NULL && gp->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        gp->constraint,
                        ctx,
                        consumer_site,
                        consumer_name,
                        reason);
                }
            }
        }
        return;

    default:
        return;
    }
}

static void
semantic_type_resolution_collect_generic_contract_inventory(GenericParams *gp,
                                                            WhereClause *wc,
                                                            SemanticContext *ctx,
                                                            const ASTNode *owner,
                                                            const char *owner_kind,
                                                            const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            semantic_type_resolution_collect_type_refs(
                param->default_type,
                ctx,
                owner,
                consumer_name,
                "default-type lookup");
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                semantic_type_resolution_collect_type_refs(
                    tc->bounds[b],
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }
}

static void
semantic_type_resolution_record_string_dependency(SemanticContext *ctx,
                                                  const ASTNode *consumer_site,
                                                  const char *consumer_name,
                                                  const char *provider_name,
                                                  const char *reason)
{
    if (provider_name == NULL || provider_name[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_DECL,
        NULL,
        provider_name,
        reason);
}

static void
semantic_type_resolution_precollect_required_abilities(ASTNode **ability_refs,
                                                       size_t ability_count,
                                                       SemanticContext *ctx,
                                                       const ASTNode *owner,
                                                       const char *consumer_name,
                                                       const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        semantic_type_resolution_collect_type_refs(
            ability_refs[i],
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                      SemanticContext *ctx)
{
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ability_decl->data.ability_decl.generic_params,
        ability_decl->data.ability_decl.where_clause,
        ctx,
        ability_decl,
        "ability",
        ability_decl->data.ability_decl.name);

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        char *consumer_name;

        if (req == NULL || req->type != AST_REQUIRE_FIELD)
            continue;

        consumer_name = tc_strdup_fmt("ability %s.%s",
                                      ability_decl->data.ability_decl.name != NULL
                                          ? ability_decl->data.ability_decl.name : "<ability>",
                                      req->data.require_field.name != NULL
                                          ? req->data.require_field.name : "<require-field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < ability_decl->data.ability_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            ability_decl->data.ability_decl.methods[i],
            ctx,
            ability_decl->data.ability_decl.name);
    }
}

static void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_strdup_fmt("event %s.%s",
                                      event_decl->data.event_decl.name != NULL
                                          ? event_decl->data.event_decl.name : "<event>",
                                      param->data.let_decl.name != NULL
                                          ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    semantic_type_resolution_collect_type_refs(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name != NULL
            ? event_decl->data.event_decl.name : "<event>",
        "event return type lookup");
}

static void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx)
{
    if (enum_decl == NULL || enum_decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
        ASTNode **params = enum_decl->data.enum_decl.variant_params != NULL
            ? enum_decl->data.enum_decl.variant_params[i] : NULL;
        size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
            ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;
        const char *variant_name = enum_decl->data.enum_decl.variants != NULL
            ? enum_decl->data.enum_decl.variants[i] : NULL;
        char *consumer_name;

        if (params == NULL || param_count == 0)
            continue;

        consumer_name = tc_strdup_fmt("enum %s.%s",
                                      enum_decl->data.enum_decl.name != NULL
                                          ? enum_decl->data.enum_decl.name : "<enum>",
                                      variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            semantic_type_resolution_collect_type_refs(
                params[j],
                ctx,
                enum_decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }

    semantic_stage_method_array(
        enum_decl->data.enum_decl.methods,
        enum_decl->data.enum_decl.method_count,
        ctx,
        enum_decl->data.enum_decl.name);
}

static void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx)
{
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        role_decl->data.role_decl.generic_params,
        role_decl->data.role_decl.where_clause,
        ctx,
        role_decl,
        "role",
        role_decl->data.role_decl.name);

    semantic_type_resolution_collect_type_refs(
        role_decl->data.role_decl.for_type,
        ctx,
        role_decl,
        role_decl->data.role_decl.name != NULL
            ? role_decl->data.role_decl.name : "<role>",
        "role host-type lookup");

    for (size_t i = 0; i < role_decl->data.role_decl.include_count; i++) {
        ASTNode *inc = role_decl->data.role_decl.includes[i];
        char *consumer_name;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT)
            continue;

        consumer_name = tc_strdup_fmt("role %s.include",
                                      role_decl->data.role_decl.name != NULL
                                          ? role_decl->data.role_decl.name : "<role>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_record_string_dependency(
            ctx,
            inc,
            consumer_name,
            inc->data.include_stmt.role_name,
            "role include lookup");

        if (inc->data.include_stmt.type_args != NULL) {
            for (size_t j = 0; j < inc->data.include_stmt.type_args->count; j++) {
                GenericParam *arg = inc->data.include_stmt.type_args->params[j];
                if (arg != NULL && arg->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
                        ctx,
                        inc,
                        consumer_name,
                        "role include type-argument lookup");
                }
            }
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        semantic_type_resolution_collect_type_refs(
            impl->data.impl_ability.ability_ref,
            ctx,
            impl,
            role_decl->data.role_decl.name != NULL
                ? role_decl->data.role_decl.name : "<role>",
            "role impl ability lookup");
    }
}

static void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = method->data.func_decl.name != NULL
        ? method->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<action>");

    semantic_type_resolution_collect_generic_contract_inventory(
        method->data.func_decl.generic_params,
        method->data.func_decl.where_clause,
        ctx,
        method,
        "func",
        consumer_name);

    for (size_t i = 0; i < method->data.func_decl.param_count; i++) {
        FuncParam *param = method->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                param->type,
                ctx,
                method,
                param_consumer_name,
                "function parameter type lookup");
            free(param_consumer_name);
        }
    }

    semantic_type_resolution_collect_type_refs(
        method->data.func_decl.return_type,
        ctx,
        method,
        consumer_name,
        "function return type lookup");

    semantic_type_resolution_precollect_required_abilities(
        method->data.func_decl.required_abilities,
        method->data.func_decl.required_ability_count,
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.within_zone,
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.causes_effect,
        "action causes-effect lookup");
}

static void
semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                    SemanticContext *ctx)
{
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        class_decl->data.class_decl.generic_params,
        class_decl->data.class_decl.where_clause,
        ctx,
        class_decl,
        "class",
        class_decl->data.class_decl.name);

    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *field = class_decl->data.class_decl.fields[i];
        char *consumer_name;

        if (field == NULL)
            continue;

        consumer_name = tc_strdup_fmt("class %s.%s",
                                      class_decl->data.class_decl.name != NULL
                                          ? class_decl->data.class_decl.name : "<class>",
                                      field->name != NULL ? field->name : "<field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                field->type,
                ctx,
                class_decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        semantic_type_resolution_precollect_action_contract(
            method,
            ctx,
            class_decl->data.class_decl.name);
    }
}

static void
semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                    SemanticContext *ctx)
{
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        party_decl->data.party_decl.generic_params,
        NULL,
        ctx,
        party_decl,
        "party",
        party_decl->data.party_decl.name);

    for (size_t i = 0; i < party_decl->data.party_decl.shared_count; i++) {
        ASTNode *field = party_decl->data.party_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<party-shared>",
            "party shared field type lookup");
    }

    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
        ASTNode *role_slot = party_decl->data.party_decl.role_slots[i];
        char *consumer_name;

        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;

        consumer_name = tc_strdup_fmt("party %s.%s",
                                      party_decl->data.party_decl.name != NULL
                                          ? party_decl->data.party_decl.name : "<party>",
                                      role_slot->data.role_slot.slot_name != NULL
                                          ? role_slot->data.role_slot.slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            role_slot->data.role_slot.required_abilities,
            role_slot->data.role_slot.ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < party_decl->data.party_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            party_decl->data.party_decl.methods[i],
            ctx,
            party_decl->data.party_decl.name);
    }
}

static void
semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                     SemanticContext *ctx)
{
    if (roster_decl == NULL || roster_decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        roster_decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        roster_decl,
        "roster",
        roster_decl->data.roster_decl.name);

    for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
        ASTNode *field = roster_decl->data.roster_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<roster-shared>",
            "roster shared field type lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.party_count; i++) {
        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.roster_slot.slot_name != NULL
                ? slot->data.roster_slot.slot_name : "<roster-slot>",
            slot->data.roster_slot.party_type,
            "roster party lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            roster_decl->data.roster_decl.methods[i],
            ctx,
            roster_decl->data.roster_decl.name);
    }
}

static void
semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                    SemanticContext *ctx)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        char *zone_slot_label;

        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;

        zone_slot_label = semantic_type_resolution_world_zone_slot_label(
            world_decl,
            zone->data.world_zone.slot_name);
        if (zone_slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, zone, zone_slot_label);
            free(zone_slot_label);
        }
    }

    for (size_t i = 0; i < world_decl->data.world_decl.shared_count; i++) {
        ASTNode *field = world_decl->data.world_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<world-shared>",
            "world shared field type lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.roster_count; i++) {
        ASTNode *roster = world_decl->data.world_decl.rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            roster,
            roster->data.world_roster.slot_name != NULL
                ? roster->data.world_roster.slot_name : "<world-roster>",
            roster->data.world_roster.roster_type,
            "world roster lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            zone,
            zone->data.world_zone.slot_name != NULL
                ? zone->data.world_zone.slot_name : "<world-zone>",
            zone->data.world_zone.zone_type,
            "world zone lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        ASTNode *zone_slot_decl = NULL;
        char *state_label;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        state_label = semantic_type_resolution_world_state_label(
            world_decl,
            state->data.world_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);

        if (state->data.world_state.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                state->data.world_state.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    zone_slot_label,
                    "world state zone-slot lookup");
                free(zone_slot_label);
            }
        }

        if (zone_slot_decl != NULL
            && zone_slot_decl->type == AST_WORLD_ZONE
            && zone_slot_decl->data.world_zone.zone_type != NULL
            && state->data.world_state.detail_name != NULL) {
            ASTNode *zone_type_decl = find_domain_decl_by_name(
                ctx->program_root,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_type_decl != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    char *projection_label = semantic_type_resolution_zone_slot_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (projection_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            projection_label,
                            "world state projection lookup");
                        free(projection_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    char *layer_label = semantic_type_resolution_zone_layer_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (layer_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            layer_label,
                            "world state layer lookup");
                        free(layer_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    char *nested_state_label = semantic_type_resolution_zone_state_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (nested_state_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            nested_state_label,
                            "world state nested-state lookup");
                        free(nested_state_label);
                    }
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                char *input_state_label = semantic_type_resolution_world_state_label(
                    world_decl,
                    input_name);
                if (input_state_label != NULL) {
                    semantic_type_resolution_record_local_contract_dependency(
                        ctx,
                        state,
                        state_label,
                        NULL,
                        input_state_label,
                        "world state composition input lookup");
                    free(input_state_label);
                }

                if (input_name != NULL) {
                    char *input_zone_label = semantic_type_resolution_world_zone_slot_label(
                        world_decl,
                        input_name);
                    if (input_zone_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            NULL,
                            input_zone_label,
                            "world state composition zone-input lookup");
                        free(input_zone_label);
                    }
                }
            }
        }

        free(state_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.activate_count; i++) {
        ASTNode *activate = world_decl->data.world_decl.activations[i];
        char *consumer_label;

        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.activate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       activate->data.world_activate.state_name != NULL
                                           ? activate->data.world_activate.state_name
                                           : (activate->data.world_activate.zone_slot_name != NULL
                                               ? activate->data.world_activate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (activate->data.world_activate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                activate->data.world_activate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world activate state lookup");
                free(state_label);
            }
        } else if (activate->data.world_activate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                activate->data.world_activate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world activate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = world_decl->data.world_decl.deactivations[i];
        char *consumer_label;

        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.deactivate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       deactivate->data.world_deactivate.state_name != NULL
                                           ? deactivate->data.world_deactivate.state_name
                                           : (deactivate->data.world_deactivate.zone_slot_name != NULL
                                               ? deactivate->data.world_deactivate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                deactivate->data.world_deactivate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world deactivate state lookup");
                free(state_label);
            }
        } else if (deactivate->data.world_deactivate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world deactivate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = world_decl->data.world_decl.maintained_zones[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;

        consumer_label = tc_strdup_fmt("world %s.maintain.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       maintain->data.world_maintain.state_name != NULL
                                           ? maintain->data.world_maintain.state_name
                                           : (maintain->data.world_maintain.zone_slot_name != NULL
                                               ? maintain->data.world_maintain.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (maintain->data.world_maintain.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                maintain->data.world_maintain.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "world maintain state lookup");
                free(state_label);
            }
        } else if (maintain->data.world_maintain.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                maintain->data.world_maintain.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world maintain zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            world_decl->data.world_decl.methods[i],
            ctx,
            world_decl->data.world_decl.name);
    }
}

static void
semantic_type_resolution_precollect_domain_inventory(ASTNode *domain_decl,
                                                     ASTNode **slots,
                                                     size_t slot_count,
                                                     ASTNode **methods,
                                                     size_t method_count,
                                                     SemanticContext *ctx,
                                                     const char *kind_name,
                                                     const char *decl_name)
{
    (void)domain_decl;

    if (ctx == NULL)
        return;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<domain-slot>",
            "domain slot type lookup");
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            decl_name != NULL ? decl_name : kind_name);
    }
}

static void
semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                     SemanticContext *ctx)
{
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            involves->data.intent_involves.subject_type,
            ctx,
            involves,
            involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "<intent-binding>",
            "intent binding type lookup");
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.value_count; i++) {
        ASTNode *value = intent_decl->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            value->data.intent_value.value_type,
            ctx,
            value,
            value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        intent_decl->data.intent_decl.default_where_type,
        ctx,
        intent_decl,
        intent_decl->data.intent_decl.name != NULL
            ? intent_decl->data.intent_decl.name : "<intent>",
        "intent default zone lookup");

    for (size_t i = 0; i < intent_decl->data.intent_decl.step_count; i++) {
        ASTNode *step = intent_decl->data.intent_decl.steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = tc_strdup_fmt(
            "intent %s.%s",
            intent_decl->data.intent_decl.name != NULL
                ? intent_decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step zone lookup");
        semantic_type_resolution_precollect_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            step->data.intent_step.causes_effect,
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}

static void
semantic_type_resolution_precollect_zone_inventory(ASTNode *zone_decl,
                                                   SemanticContext *ctx)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        char *slot_label;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;

        slot_label = semantic_type_resolution_zone_slot_label(
            zone_decl,
            slot->data.domain_slot.slot_name);
        if (slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, slot_label);
            free(slot_label);
        }

        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<zone-slot>",
            "zone slot type lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        char *layer_label;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT)
            continue;
        layer_label = semantic_type_resolution_zone_layer_label(
            zone_decl,
            slot->data.zone_layer_slot.slot_name);
        if (layer_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, layer_label);
            free(layer_label);
        }
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.zone_layer_slot.slot_name != NULL
                ? slot->data.zone_layer_slot.slot_name : "<zone-layer>",
            slot->data.zone_layer_slot.layer_type,
            "zone layer lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
        char *consumer_label;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.refresh.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       refresh->data.zone_refresh.object_slot_name != NULL
                                           ? refresh->data.zone_refresh.object_slot_name : "<refresh>");
        if (consumer_label == NULL)
            continue;

        if (refresh->data.zone_refresh.object_slot_name != NULL) {
            char *object_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            if (object_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    object_label,
                    "zone refresh target-slot lookup");
                free(object_label);
            }
        }

        if (refresh->data.zone_refresh.source_slot_name != NULL) {
            char *source_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.source_slot_name);
            if (source_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    source_label,
                    "zone refresh source-slot lookup");
                free(source_label);
            }
        }

        for (size_t map_i = 0; map_i < refresh->data.zone_refresh.field_map_count; map_i++) {
            const char *target_field = refresh->data.zone_refresh.mapped_target_fields != NULL
                ? refresh->data.zone_refresh.mapped_target_fields[map_i] : NULL;
            const char *source_field = refresh->data.zone_refresh.mapped_source_fields != NULL
                ? refresh->data.zone_refresh.mapped_source_fields[map_i] : NULL;
            char *projection_label = semantic_type_resolution_projection_path_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name,
                refresh->data.zone_refresh.source_slot_name,
                target_field,
                source_field);
            if (projection_label != NULL) {
                (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                  TYPE_RES_NODE_PROJECTION_PATH,
                                                  refresh,
                                                  projection_label);
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    TYPE_RES_NODE_PROJECTION_PATH,
                    refresh,
                    projection_label,
                    "zone refresh projection-path lookup");

                if (refresh->data.zone_refresh.object_slot_name != NULL) {
                    char *target_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name);
                    if (target_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            target_slot_label,
                            "projection target-slot carrier");
                        free(target_slot_label);
                    }
                }

                if (refresh->data.zone_refresh.source_slot_name != NULL) {
                    char *source_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name);
                    if (source_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            source_slot_label,
                            "projection source-slot carrier");
                        free(source_slot_label);
                    }
                }

                if (target_field != NULL) {
                    char *target_field_label = semantic_type_resolution_projection_slot_field_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name,
                        target_field);
                    if (target_field_label != NULL) {
                        (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                          TYPE_RES_NODE_PROJECTION_PATH,
                                                          refresh,
                                                          target_field_label);
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_PROJECTION_PATH,
                            refresh,
                            target_field_label,
                            "projection target field-path lookup");
                        free(target_field_label);
                    }
                }

                if (source_field != NULL) {
                    ASTNode *source_decl = semantic_type_resolution_projection_source_decl(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name,
                        ctx);
                    char *resolved_source_path = NULL;
                    const char *source_path_text = source_field;

                    if (source_decl != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_error = ctx->has_error;
                        Type *field_type = NULL;
                        int path_status = resolve_projection_source_field_path(
                            ctx->program_root,
                            source_decl,
                            source_field,
                            ctx,
                            &resolved_source_path,
                            &field_type);
                        (void)field_type;
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_error;
                        }
                        if (path_status == 1 && resolved_source_path != NULL)
                            source_path_text = resolved_source_path;
                    }

                    if (source_path_text != NULL) {
                        char *source_field_label = semantic_type_resolution_projection_slot_field_label(
                            zone_decl,
                            refresh->data.zone_refresh.source_slot_name,
                            source_path_text);
                        if (source_field_label != NULL) {
                            (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                              TYPE_RES_NODE_PROJECTION_PATH,
                                                              refresh,
                                                              source_field_label);
                            semantic_type_resolution_record_named_dependency(
                                ctx,
                                refresh,
                                projection_label,
                                TYPE_RES_NODE_PROJECTION_PATH,
                                refresh,
                                source_field_label,
                                "projection source field-path lookup");
                            free(source_field_label);
                        }
                    }

                    if (resolved_source_path != NULL)
                        free(resolved_source_path);
                }

                free(projection_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
        ASTNode *apply = zone_decl->data.zone_decl.applies[i];
        char *consumer_label;

        if (apply == NULL || apply->type != AST_ZONE_APPLY)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.apply.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       apply->data.zone_apply.effect_slot_name != NULL
                                           ? apply->data.zone_apply.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (apply->data.zone_apply.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                apply->data.zone_apply.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone apply effect-slot lookup");
                free(layer_label);
            }
        }
        if (apply->data.zone_apply.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                apply->data.zone_apply.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone apply target-slot lookup");
                free(target_label);
            }
        }
        if (apply->data.zone_apply.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                apply->data.zone_apply.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone apply state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.link_count; i++) {
        ASTNode *link = zone_decl->data.zone_decl.links[i];
        char *consumer_label;

        if (link == NULL || link->type != AST_ZONE_LINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.link.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       link->data.zone_link.relation_slot_name != NULL
                                           ? link->data.zone_link.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (link->data.zone_link.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                link->data.zone_link.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone link relation-slot lookup");
                free(layer_label);
            }
        }
        if (link->data.zone_link.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone link left-slot lookup");
                free(left_label);
            }
        }
        if (link->data.zone_link.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone link right-slot lookup");
                free(right_label);
            }
        }
        if (link->data.zone_link.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                link->data.zone_link.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone link state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.detach_count; i++) {
        ASTNode *detach = zone_decl->data.zone_decl.detaches[i];
        char *consumer_label;

        if (detach == NULL || detach->type != AST_ZONE_DETACH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.detach.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       detach->data.zone_detach.effect_slot_name != NULL
                                           ? detach->data.zone_detach.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (detach->data.zone_detach.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                detach->data.zone_detach.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone detach effect-slot lookup");
                free(layer_label);
            }
        }
        if (detach->data.zone_detach.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                detach->data.zone_detach.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone detach target-slot lookup");
                free(target_label);
            }
        }
        if (detach->data.zone_detach.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                detach->data.zone_detach.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone detach state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = zone_decl->data.zone_decl.unlinks[i];
        char *consumer_label;

        if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.unlink.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       unlink->data.zone_unlink.relation_slot_name != NULL
                                           ? unlink->data.zone_unlink.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (unlink->data.zone_unlink.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                unlink->data.zone_unlink.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone unlink relation-slot lookup");
                free(layer_label);
            }
        }
        if (unlink->data.zone_unlink.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone unlink left-slot lookup");
                free(left_label);
            }
        }
        if (unlink->data.zone_unlink.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone unlink right-slot lookup");
                free(right_label);
            }
        }
        if (unlink->data.zone_unlink.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                unlink->data.zone_unlink.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone unlink state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_effects[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-effect.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_effect.effect_slot_name != NULL
                                           ? maintain->data.zone_maintain_effect.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_effect.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_effect.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-effect slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_effect.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_effect.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone maintain-effect target-slot lookup");
                free(target_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_relations[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-relation.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_relation.relation_slot_name != NULL
                                           ? maintain->data.zone_maintain_relation.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_relation.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-relation slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_relation.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone maintain-relation left-slot lookup");
                free(left_label);
            }
        }
        if (maintain->data.zone_maintain_relation.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone maintain-relation right-slot lookup");
                free(right_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        char *state_label;

        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;

        state_label = semantic_type_resolution_zone_state_label(
            zone_decl,
            state->data.zone_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        if (state->data.zone_state.layer_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                state->data.zone_state.layer_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    layer_label,
                    "zone state layer lookup");
                free(layer_label);
            }
        }

        if (state->data.zone_state.left_or_target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.left_or_target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    target_label,
                    "zone state target-slot lookup");
                free(target_label);
            }
        }

        if (state->data.zone_state.is_relation
            && state->data.zone_state.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    right_label,
                    "zone state right-slot lookup");
                free(right_label);
            }
        }
        free(state_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_states[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-state.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_state.state_name != NULL
                                           ? maintain->data.zone_maintain_state.state_name
                                           : "<state>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_state.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                maintain->data.zone_maintain_state.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone maintain-state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone_decl->data.zone_decl.authorities[i];
        char *consumer_name;

        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;

        consumer_name = tc_strdup_fmt("zone %s.%s",
                                      zone_decl->data.zone_decl.name != NULL
                                          ? zone_decl->data.zone_decl.name : "<zone>",
                                      authority->data.zone_authority.subject_slot_name != NULL
                                          ? authority->data.zone_authority.subject_slot_name
                                          : "<authority>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            authority->data.zone_authority.required_abilities,
            authority->data.zone_authority.ability_count,
            ctx,
            authority,
            consumer_name,
            "zone authority ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            zone_decl->data.zone_decl.methods[i],
            ctx,
            zone_decl->data.zone_decl.name);
    }
}

static void
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label)
{
    if (ctx == NULL || label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      TYPE_RES_NODE_LOCAL_CONTRACT,
                                      site,
                                      label);
}

static void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason)
{
    if (ctx == NULL || provider_label == NULL || provider_label[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_label,
        TYPE_RES_NODE_LOCAL_CONTRACT,
        provider_site,
        provider_label,
        reason);
}

static char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name)
{
    return tc_strdup_fmt("world %s.zone.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         slot_name != NULL ? slot_name : "<zone-slot>");
}

static char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name)
{
    return tc_strdup_fmt("world %s.state.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name)
{
    return tc_strdup_fmt("zone %s.slot.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>");
}

static char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name)
{
    return tc_strdup_fmt("zone %s.layer.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<layer>");
}

static char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name)
{
    return tc_strdup_fmt("zone %s.state.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name)
{
    return tc_strdup_fmt("zone %s.projection.%s.%s<-%s.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         target_slot_name != NULL ? target_slot_name : "<target-slot>",
                         target_field_name != NULL ? target_field_name : "<target-field>",
                         source_slot_name != NULL ? source_slot_name : "<source-slot>",
                         source_field_name != NULL ? source_field_name : "<source-field>");
}

static char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path)
{
    return tc_strdup_fmt("zone %s.slot.%s.field.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>",
                         field_path != NULL ? field_path : "<field-path>");
}

static ASTNode *
semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                const char *slot_name,
                                                SemanticContext *ctx)
{
    ASTNode *slot;
    ASTNode *type_node;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL || ctx == NULL) {
        return NULL;
    }

    slot = find_zone_domain_slot(zone_decl, slot_name);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return NULL;

    type_node = slot->data.domain_slot.type;
    if (type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return NULL;
    }

    return find_type_decl_by_name(ctx->program_root, type_node->data.type.name);
}

static void
semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                 SemanticContext *ctx)
{
    const char *label = NULL;
    TypeResolutionNodeKind kind = TYPE_RES_NODE_DECL;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        label = stmt->data.type_alias.name;
        kind = TYPE_RES_NODE_ALIAS;
        break;
    case AST_CLASS_DECL:
        label = stmt->data.class_decl.name;
        break;
    case AST_FUNC_DECL:
        label = stmt->data.func_decl.name;
        break;
    case AST_EVENT_DECL:
        label = stmt->data.event_decl.name;
        break;
    case AST_ENUM_DECL:
        label = stmt->data.enum_decl.name;
        break;
    case AST_ABILITY_DECL:
        label = stmt->data.ability_decl.name;
        break;
    case AST_ROLE_DECL:
        label = stmt->data.role_decl.name;
        break;
    case AST_PARTY_DECL:
        label = stmt->data.party_decl.name;
        break;
    case AST_ROSTER_DECL:
        label = stmt->data.roster_decl.name;
        break;
    case AST_WORLD_DECL:
        label = stmt->data.world_decl.name;
        break;
    case AST_INTENT_DECL:
        label = stmt->data.intent_decl.name;
        break;
    case AST_RELATION_DECL:
        label = stmt->data.relation_decl.name;
        break;
    case AST_EFFECT_DECL:
        label = stmt->data.effect_decl.name;
        break;
    case AST_ZONE_DECL:
        label = stmt->data.zone_decl.name;
        break;
    default:
        return;
    }

    if (label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      kind,
                                      stmt,
                                      label);
}

static void
semantic_type_resolution_precollect_program(ASTNode *program,
                                            SemanticContext *ctx)
{
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;

        semantic_type_resolution_register_top_level_decl(stmt, ctx);

        switch (stmt->type) {
        case AST_TYPE_ALIAS:
            if (stmt->data.type_alias.name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    stmt->data.type_alias.target_type,
                    ctx,
                    stmt,
                    stmt->data.type_alias.name,
                    "type-alias target lookup");
            }
            break;

        case AST_CLASS_DECL:
            semantic_type_resolution_precollect_class_inventory(stmt, ctx);
            break;

        case AST_FUNC_DECL:
            semantic_type_resolution_precollect_action_contract(
                stmt,
                ctx,
                stmt->data.func_decl.name);
            break;

        case AST_EVENT_DECL:
            semantic_type_resolution_precollect_event_inventory(stmt, ctx);
            break;

        case AST_ENUM_DECL:
            semantic_type_resolution_precollect_enum_inventory(stmt, ctx);
            break;

        case AST_ABILITY_DECL:
            semantic_type_resolution_precollect_ability_inventory(stmt, ctx);
            break;

        case AST_ROLE_DECL:
            semantic_type_resolution_precollect_role_inventory(stmt, ctx);
            break;

        case AST_PARTY_DECL:
            semantic_type_resolution_precollect_party_inventory(stmt, ctx);
            break;

        case AST_ROSTER_DECL:
            semantic_type_resolution_precollect_roster_inventory(stmt, ctx);
            break;

        case AST_WORLD_DECL:
            semantic_type_resolution_precollect_world_inventory(stmt, ctx);
            break;

        case AST_INTENT_DECL:
            semantic_type_resolution_precollect_intent_inventory(stmt, ctx);
            break;

        case AST_ZONE_DECL:
            semantic_type_resolution_precollect_zone_inventory(stmt, ctx);
            break;

        case AST_RELATION_DECL:
            semantic_type_resolution_precollect_domain_inventory(
                stmt,
                stmt->data.relation_decl.slots,
                stmt->data.relation_decl.slot_count,
                stmt->data.relation_decl.methods,
                stmt->data.relation_decl.method_count,
                ctx,
                "relation",
                stmt->data.relation_decl.name);
            break;

        case AST_EFFECT_DECL:
            semantic_type_resolution_precollect_domain_inventory(
                stmt,
                stmt->data.effect_decl.slots,
                stmt->data.effect_decl.slot_count,
                stmt->data.effect_decl.methods,
                stmt->data.effect_decl.method_count,
                ctx,
                "effect",
                stmt->data.effect_decl.name);
            break;

        default:
            break;
        }
    }
}

static const char *
semantic_type_resolution_decl_label(ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name;
    case AST_PARTY_DECL:
        return stmt->data.party_decl.name;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name;
    default:
        return NULL;
    }
}

static TypeResolutionNodeKind
semantic_type_resolution_decl_kind(ASTNode *stmt)
{
    if (stmt != NULL && stmt->type == AST_TYPE_ALIAS)
        return TYPE_RES_NODE_ALIAS;
    return TYPE_RES_NODE_DECL;
}

static Type *
semantic_stage_resolve_type_quiet(ASTNode *type_node,
                                  SemanticContext *ctx,
                                  const ASTNode *consumer_site,
                                  const char *consumer_name,
                                  const char *reason)
{
    size_t saved_diag;
    bool saved_error;
    Type *resolved;

    if (type_node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    (void)consumer_site;
    (void)consumer_name;
    (void)reason;

    saved_diag = ctx->diagnostic_count;
    saved_error = ctx->has_error;
    resolved = resolve_type_node(type_node, ctx);
    if (ctx->diagnostic_count > saved_diag) {
        for (size_t i = saved_diag; i < ctx->diagnostic_count; i++) {
            if (ctx->diagnostics[i] != NULL) {
                free(ctx->diagnostics[i]->message);
                free(ctx->diagnostics[i]);
                ctx->diagnostics[i] = NULL;
            }
        }
        ctx->diagnostic_count = saved_diag;
        ctx->has_error = saved_error;
        return TYPE_UNKNOWN;
    }

    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static ASTNode *
semantic_stage_named_decl_quiet(SemanticContext *ctx,
                                ASTNodeType decl_type,
                                const char *provider_name)
{
    if (ctx == NULL || ctx->program_root == NULL
        || provider_name == NULL || provider_name[0] == '\0') {
        return NULL;
    }

    switch (decl_type) {
    case AST_CLASS_DECL:
        return find_type_decl_by_name(ctx->program_root, provider_name);
    case AST_ABILITY_DECL:
        return find_ability_decl_by_name(ctx->program_root, provider_name);
    case AST_ROLE_DECL:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        return find_domain_decl_by_name(ctx->program_root, decl_type, provider_name);
    default:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    }
}

static ASTNode *
semantic_world_find_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL
            && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
semantic_world_find_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL
            && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
semantic_zone_find_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
semantic_zone_find_state_local(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static void
semantic_stage_world_local_contracts(ASTNode *world_decl,
                                     SemanticContext *ctx)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        ASTNode *zone_slot_decl = NULL;
        ASTNode *zone_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL) {
            zone_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
        }

        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ZONE:
            (void)zone_slot_decl;
            break;

        case WORLD_STATE_SOURCE_PROJECTION:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_LAYER:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_STATE:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_state_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY:
            for (size_t j = 0; j < state->data.world_state.input_count; j++) {
                const char *input_name = state->data.world_state.input_names[j];
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
            break;
        }
    }

    for (size_t i = 0; i < world_decl->data.world_decl.activate_count; i++) {
        ASTNode *activate = world_decl->data.world_decl.activations[i];
        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;
        if (activate->data.world_activate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                activate->data.world_activate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                activate->data.world_activate.zone_slot_name);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = world_decl->data.world_decl.deactivations[i];
        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                deactivate->data.world_deactivate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = world_decl->data.world_decl.maintained_zones[i];
        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;
        if (maintain->data.world_maintain.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                maintain->data.world_maintain.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                maintain->data.world_maintain.zone_slot_name);
    }
}

static void
semantic_stage_zone_local_contracts(ASTNode *zone_decl)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.object_slot_name);
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.source_slot_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_states[i];
        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;
        (void)semantic_zone_find_state_local(zone_decl,
            maintain->data.zone_maintain_state.state_name);
    }
}

static void
semantic_stage_required_abilities(ASTNode **ability_refs,
                                  size_t ability_count,
                                  SemanticContext *ctx,
                                  const ASTNode *owner,
                                  const char *consumer_name,
                                  const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability_ref = ability_refs[i];

        if (ability_ref != NULL
            && ability_ref->type == AST_TYPE
            && ability_ref->data.type.name != NULL) {
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ability_ref->data.type.name);
        }

        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_stage_generic_contract_nodes(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *kind_name,
                                      const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            if (param->default_type != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->default_type,
                    ctx,
                    owner,
                    consumer_name,
                    "default-type lookup");
            }

            if (param->constraint != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->constraint,
                    ctx,
                    owner,
                    consumer_name,
                    "generic constraint lookup");
            }
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                (void)semantic_stage_resolve_type_quiet(
                    tc->bounds[b],
                    ctx,
                    owner,
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }

    validate_generic_param_defaults(gp, ctx, owner, kind_name);
    validate_where_clause_bounds(wc, ctx, owner);
}

static void
semantic_stage_function_signature(ASTNode *func_decl,
                                  SemanticContext *ctx,
                                  const char *fallback_name)
{
    const char *consumer_name;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = func_decl->data.func_decl.name != NULL
        ? func_decl->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<func>");

    semantic_stage_generic_contract_nodes(
        func_decl->data.func_decl.generic_params,
        func_decl->data.func_decl.where_clause,
        ctx,
        func_decl,
        "func",
        consumer_name);

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->type,
            ctx,
            func_decl,
            param_consumer_name,
            "function parameter type lookup");
        free(param_consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        func_decl->data.func_decl.return_type,
        ctx,
        func_decl,
        consumer_name,
        "function return type lookup");

    semantic_stage_required_abilities(
        func_decl->data.func_decl.required_abilities,
        func_decl->data.func_decl.required_ability_count,
        ctx,
        func_decl,
        consumer_name,
        "action ability consumer lookup");
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_ZONE_DECL,
        func_decl->data.func_decl.within_zone);
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_EFFECT_DECL,
        func_decl->data.func_decl.causes_effect);
}

static void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name)
{
    if (methods == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < method_count; i++)
        semantic_stage_function_signature(methods[i], ctx, fallback_name);
}

static void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_strdup_fmt("event %s.%s",
                                      event_decl->data.event_decl.name != NULL
                                          ? event_decl->data.event_decl.name : "<event>",
                                      param->data.let_decl.name != NULL
                                          ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name,
        "event return type lookup");
}

static ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind)
{
    if (program == NULL || program->type != AST_PROGRAM || label == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *stmt_label;

        if (stmt == NULL)
            continue;
        if (semantic_type_resolution_decl_kind(stmt) != kind)
            continue;

        stmt_label = semantic_type_resolution_decl_label(stmt);
        if (stmt_label != NULL && strcmp(stmt_label, label) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label)
{
    const char *space;
    const char *dot;
    size_t name_len;
    char *host_name;
    ASTNode *decl = NULL;

    if (program == NULL || label == NULL)
        return NULL;

    if (strncmp(label, "world ", 6) == 0) {
        space = label + 6;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_WORLD_DECL, host_name);
        free(host_name);
        return decl;
    }

    if (strncmp(label, "zone ", 5) == 0) {
        space = label + 5;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_ZONE_DECL, host_name);
        free(host_name);
        return decl;
    }

    return NULL;
}

static void
semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                               const char *label,
                                               SemanticContext *ctx)
{
    const char *suffix;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    suffix = strstr(label, ".zone.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        (void)semantic_world_find_zone_slot_local(world_decl, slot_name);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_world_find_state_local(world_decl, suffix + 7);
        ASTNode *zone_slot_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            return;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL && zone_slot_decl->type == AST_WORLD_ZONE) {
            ASTNode *zone_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL
                && state->data.world_state.detail_name != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    (void)semantic_zone_find_layer_slot_local(zone_decl,
                        state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    (void)semantic_zone_find_state_local(zone_decl,
                        state->data.world_state.detail_name);
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t i = 0; i < state->data.world_state.input_count; i++) {
                const char *input_name = state->data.world_state.input_names[i];
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".activate.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".deactivate.");
    if (suffix != NULL) {
        const char *target = suffix + 12;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".maintain.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
    }
}

static void
semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                              const char *label,
                                              SemanticContext *ctx)
{
    const char *suffix;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    suffix = strstr(label, ".slot.");
    if (suffix != NULL && strstr(label, ".field.") == NULL) {
        (void)find_zone_domain_slot(zone_decl, suffix + 6);
        return;
    }

    suffix = strstr(label, ".layer.");
    if (suffix != NULL) {
        (void)semantic_zone_find_layer_slot_local(zone_decl, suffix + 7);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_zone_find_state_local(zone_decl, suffix + 7);
        if (state == NULL || state->type != AST_ZONE_STATE)
            return;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
        return;
    }

    suffix = strstr(label, ".refresh.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 9;
        for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
            ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            if (refresh->data.zone_refresh.object_slot_name != NULL
                && strcmp(refresh->data.zone_refresh.object_slot_name, slot_name) == 0) {
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.object_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.source_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".apply.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 7;
        for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
            ASTNode *apply = zone_decl->data.zone_decl.applies[i];
            if (apply == NULL || apply->type != AST_ZONE_APPLY)
                continue;
            if (apply->data.zone_apply.effect_slot_name != NULL
                && strcmp(apply->data.zone_apply.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    apply->data.zone_apply.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    apply->data.zone_apply.target_slot_name);
                if (apply->data.zone_apply.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        apply->data.zone_apply.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".link.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        for (size_t i = 0; i < zone_decl->data.zone_decl.link_count; i++) {
            ASTNode *link = zone_decl->data.zone_decl.links[i];
            if (link == NULL || link->type != AST_ZONE_LINK)
                continue;
            if (link->data.zone_link.relation_slot_name != NULL
                && strcmp(link->data.zone_link.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    link->data.zone_link.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.right_slot_name);
                if (link->data.zone_link.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        link->data.zone_link.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".detach.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < zone_decl->data.zone_decl.detach_count; i++) {
            ASTNode *detach = zone_decl->data.zone_decl.detaches[i];
            if (detach == NULL || detach->type != AST_ZONE_DETACH)
                continue;
            if (detach->data.zone_detach.effect_slot_name != NULL
                && strcmp(detach->data.zone_detach.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    detach->data.zone_detach.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    detach->data.zone_detach.target_slot_name);
                if (detach->data.zone_detach.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        detach->data.zone_detach.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".unlink.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < zone_decl->data.zone_decl.unlink_count; i++) {
            ASTNode *unlink = zone_decl->data.zone_decl.unlinks[i];
            if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
                continue;
            if (unlink->data.zone_unlink.relation_slot_name != NULL
                && strcmp(unlink->data.zone_unlink.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    unlink->data.zone_unlink.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.right_slot_name);
                if (unlink->data.zone_unlink.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        unlink->data.zone_unlink.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-effect.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 17;
        for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_effect_count; i++) {
            ASTNode *maintain = zone_decl->data.zone_decl.maintained_effects[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
                continue;
            if (maintain->data.zone_maintain_effect.effect_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_effect.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_effect.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_effect.target_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-relation.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 19;
        for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_relation_count; i++) {
            ASTNode *maintain = zone_decl->data.zone_decl.maintained_relations[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
                continue;
            if (maintain->data.zone_maintain_relation.relation_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_relation.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_relation.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.right_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-state.");
    if (suffix != NULL) {
        (void)semantic_zone_find_state_local(zone_decl, suffix + 15);
        return;
    }

    if (strstr(label, ".projection.") != NULL) {
        for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
            ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.source_slot_name);
        }
        return;
    }

    if (strstr(label, ".field.") != NULL) {
        const char *slot_part = strstr(label, ".slot.");
        const char *field_part = strstr(label, ".field.");
        if (slot_part != NULL && field_part != NULL && field_part > slot_part) {
            size_t slot_len = (size_t)(field_part - (slot_part + 6));
            char *slot_name = calloc(slot_len + 1, 1);
            if (slot_name == NULL)
                return;
            memcpy(slot_name, slot_part + 6, slot_len);
            if (semantic_type_resolution_projection_source_decl(zone_decl, slot_name, ctx) == NULL)
                (void)find_zone_domain_slot(zone_decl, slot_name);
            free(slot_name);
        }
    }
}

static void
semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode *saved_nominal;
    ASTNode *saved_relation;
    ASTNode *saved_effect;
    ASTNode *saved_zone;
    ASTNode *saved_world;

    if (decl == NULL || ctx == NULL)
        return;

    saved_nominal = ctx->current_nominal_decl;
    saved_relation = ctx->current_relation;
    saved_effect = ctx->current_effect;
    saved_zone = ctx->current_zone;
    saved_world = ctx->current_world;

    switch (decl->type) {
    case AST_TYPE_ALIAS: {
        Symbol *sym;
        if (decl->data.type_alias.name == NULL)
            break;
        sym = scope_lookup_current(ctx->scope, decl->data.type_alias.name);
        if (sym != NULL)
            sym->type = resolve_type_node(decl->data.type_alias.target_type, ctx);
        break;
    }

    case AST_CLASS_DECL:
        ctx->current_nominal_decl = decl;
        semantic_stage_generic_contract_nodes(
            decl->data.class_decl.generic_params,
            decl->data.class_decl.where_clause,
            ctx,
            decl,
            "class",
            decl->data.class_decl.name);
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            char *consumer_name;
            if (field == NULL)
                continue;
            consumer_name = tc_strdup_fmt("class %s.%s",
                                          decl->data.class_decl.name != NULL
                                              ? decl->data.class_decl.name : "<class>",
                                          field->name != NULL ? field->name : "<field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->type,
                ctx,
                decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.class_decl.methods,
            decl->data.class_decl.method_count,
            ctx,
            decl->data.class_decl.name);
        break;

    case AST_FUNC_DECL:
        semantic_stage_function_signature(decl, ctx, decl->data.func_decl.name);
        break;

    case AST_EVENT_DECL:
        semantic_stage_event_signature(decl, ctx);
        break;

    case AST_ENUM_DECL:
        for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
            ASTNode **params = decl->data.enum_decl.variant_params != NULL
                ? decl->data.enum_decl.variant_params[i] : NULL;
            size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
                ? decl->data.enum_decl.variant_param_counts[i] : 0;
            const char *variant_name = decl->data.enum_decl.variants != NULL
                ? decl->data.enum_decl.variants[i] : NULL;
            char *consumer_name;

            if (params == NULL || param_count == 0)
                continue;

            consumer_name = tc_strdup_fmt("enum %s.%s",
                                          decl->data.enum_decl.name != NULL
                                              ? decl->data.enum_decl.name : "<enum>",
                                          variant_name != NULL ? variant_name : "<variant>");
            if (consumer_name == NULL)
                continue;

            for (size_t j = 0; j < param_count; j++) {
                (void)semantic_stage_resolve_type_quiet(
                    params[j],
                    ctx,
                    decl,
                    consumer_name,
                    "enum variant payload type lookup");
            }
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.enum_decl.methods,
            decl->data.enum_decl.method_count,
            ctx,
            decl->data.enum_decl.name);
        break;

    case AST_ABILITY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.ability_decl.generic_params,
            decl->data.ability_decl.where_clause,
            ctx,
            decl,
            "ability",
            decl->data.ability_decl.name);
        for (size_t i = 0; i < decl->data.ability_decl.require_count; i++) {
            ASTNode *req = decl->data.ability_decl.require_fields[i];
            char *consumer_name;
            if (req == NULL || req->type != AST_REQUIRE_FIELD)
                continue;
            consumer_name = tc_strdup_fmt("ability %s.%s",
                                          decl->data.ability_decl.name != NULL
                                              ? decl->data.ability_decl.name : "<ability>",
                                          req->data.require_field.name != NULL
                                              ? req->data.require_field.name : "<require-field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.ability_decl.methods,
            decl->data.ability_decl.method_count,
            ctx,
            decl->data.ability_decl.name);
        break;

    case AST_ROLE_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.role_decl.generic_params,
            decl->data.role_decl.where_clause,
            ctx,
            decl,
            "role",
            decl->data.role_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.role_decl.for_type,
            ctx,
            decl,
            decl->data.role_decl.name,
            "role host-type lookup");
        for (size_t i = 0; i < decl->data.role_decl.include_count; i++) {
            ASTNode *inc = decl->data.role_decl.includes[i];
            ASTNode *included_role_decl;
            ASTNode **effective = NULL;
            size_t effective_count = 0;

            if (inc == NULL || inc->type != AST_INCLUDE_STMT)
                continue;

            included_role_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ROLE_DECL,
                inc->data.include_stmt.role_name);
            effective = collect_effective_generic_arg_nodes(
                (included_role_decl != NULL && included_role_decl->type == AST_ROLE_DECL)
                    ? included_role_decl->data.role_decl.generic_params
                    : NULL,
                inc->data.include_stmt.type_args,
                inc,
                ctx,
                "role include",
                inc->data.include_stmt.role_name,
                &effective_count);
            free(effective);
            (void)effective_count;
        }
        for (size_t i = 0; i < decl->data.role_decl.impl_count; i++) {
            ASTNode *impl = decl->data.role_decl.impl_abilities[i];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            if (impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
                (void)semantic_stage_named_decl_quiet(
                    ctx,
                    AST_ABILITY_DECL,
                    impl->data.impl_ability.ability_ref->data.type.name);
            }
            (void)semantic_stage_resolve_type_quiet(
                impl->data.impl_ability.ability_ref,
                ctx,
                impl,
                decl->data.role_decl.name,
                "role impl ability lookup");
        }
        break;

    case AST_PARTY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.party_decl.generic_params,
            NULL,
            ctx,
            decl,
            "party",
            decl->data.party_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.party_decl.extends,
            ctx,
            decl,
            decl->data.party_decl.name,
            "party extends lookup");
        for (size_t i = 0; i < decl->data.party_decl.shared_count; i++) {
            ASTNode *field = decl->data.party_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "party shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.party_decl.role_count; i++) {
            ASTNode *role_slot = decl->data.party_decl.role_slots[i];
            char *consumer_name;
            if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
                continue;
            consumer_name = tc_strdup_fmt("party %s.%s",
                                          decl->data.party_decl.name != NULL
                                              ? decl->data.party_decl.name : "<party>",
                                          role_slot->data.role_slot.slot_name != NULL
                                              ? role_slot->data.role_slot.slot_name : "<role-slot>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                role_slot->data.role_slot.required_abilities,
                role_slot->data.role_slot.ability_count,
                ctx,
                role_slot,
                consumer_name,
                "party role slot ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.party_decl.methods,
            decl->data.party_decl.method_count,
            ctx,
            decl->data.party_decl.name);
        break;

    case AST_ROSTER_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.roster_decl.generic_params,
            NULL,
            ctx,
            decl,
            "roster",
            decl->data.roster_decl.name);
        for (size_t i = 0; i < decl->data.roster_decl.party_count; i++) {
            ASTNode *slot = decl->data.roster_decl.party_slots[i];
            if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_PARTY_DECL,
                slot->data.roster_slot.party_type);
        }
        for (size_t i = 0; i < decl->data.roster_decl.shared_count; i++) {
            ASTNode *field = decl->data.roster_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "roster shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.roster_decl.methods,
            decl->data.roster_decl.method_count,
            ctx,
            decl->data.roster_decl.name);
        break;

    case AST_WORLD_DECL:
        ctx->current_world = decl;
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *roster = decl->data.world_decl.rosters[i];
            if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ROSTER_DECL,
                roster->data.world_roster.roster_type);
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *zone = decl->data.world_decl.zones[i];
            if (zone == NULL || zone->type != AST_WORLD_ZONE)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone->data.world_zone.zone_type);
        }
        semantic_stage_world_local_contracts(decl, ctx);
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *field = decl->data.world_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "world shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.world_decl.methods,
            decl->data.world_decl.method_count,
            ctx,
            decl->data.world_decl.name);
        break;

    case AST_INTENT_DECL:
        for (size_t i = 0; i < decl->data.intent_decl.involve_count; i++) {
            ASTNode *binding = decl->data.intent_decl.involves[i];
            if (binding == NULL || binding->type != AST_INTENT_INVOLVES)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_involves.subject_type,
                ctx,
                binding,
                binding->data.intent_involves.alias,
                "intent involves type lookup");
        }
        for (size_t i = 0; i < decl->data.intent_decl.value_count; i++) {
            ASTNode *binding = decl->data.intent_decl.values[i];
            if (binding == NULL || binding->type != AST_INTENT_VALUE)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_value.value_type,
                ctx,
                binding,
                binding->data.intent_value.alias,
                "intent value type lookup");
        }
        (void)semantic_stage_resolve_type_quiet(
            decl->data.intent_decl.default_where_type,
            ctx,
            decl,
            decl->data.intent_decl.name,
            "intent default where-type lookup");
        for (size_t i = 0; i < decl->data.intent_decl.step_count; i++) {
            ASTNode *step = decl->data.intent_decl.steps[i];
            char *step_consumer_name;
            if (step == NULL || step->type != AST_INTENT_STEP)
                continue;
            step_consumer_name = tc_strdup_fmt(
                "intent %s.%s",
                decl->data.intent_decl.name != NULL
                    ? decl->data.intent_decl.name : "<intent>",
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>");
            if (step_consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                step->data.intent_step.where_type,
                ctx,
                step,
                step_consumer_name,
                "intent step where-type lookup");
            semantic_stage_required_abilities(
                step->data.intent_step.required_abilities,
                step->data.intent_step.required_ability_count,
                ctx,
                step,
                step_consumer_name,
                "intent step ability consumer lookup");
            free(step_consumer_name);
        }
        break;

    case AST_RELATION_DECL:
        ctx->current_relation = decl;
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_left_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-left type lookup");
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_right_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-right type lookup");
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "relation slot type lookup");
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *field = decl->data.relation_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "relation shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.relation_decl.methods,
            decl->data.relation_decl.method_count,
            ctx,
            decl->data.relation_decl.name);
        break;

    case AST_EFFECT_DECL:
        ctx->current_effect = decl;
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "effect slot type lookup");
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *field = decl->data.effect_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "effect shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.effect_decl.methods,
            decl->data.effect_decl.method_count,
            ctx,
            decl->data.effect_decl.name);
        break;

    case AST_ZONE_DECL:
        ctx->current_zone = decl;
        for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
            ASTNode *slot = decl->data.zone_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "zone slot type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer == NULL || layer->type != AST_ZONE_LAYER_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                layer->data.zone_layer_slot.is_relation
                    ? AST_RELATION_DECL
                    : AST_EFFECT_DECL,
                layer->data.zone_layer_slot.layer_type);
        }
        semantic_stage_zone_local_contracts(decl);
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *field = decl->data.zone_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "zone shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.authority_count; i++) {
            ASTNode *authority = decl->data.zone_decl.authorities[i];
            char *consumer_name;
            if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
                continue;
            consumer_name = tc_strdup_fmt("zone %s.%s",
                                          decl->data.zone_decl.name != NULL
                                              ? decl->data.zone_decl.name : "<zone>",
                                          authority->data.zone_authority.subject_slot_name != NULL
                                              ? authority->data.zone_authority.subject_slot_name
                                              : "<authority>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                authority->data.zone_authority.required_abilities,
                authority->data.zone_authority.ability_count,
                ctx,
                authority,
                consumer_name,
                "zone authority ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.zone_decl.methods,
            decl->data.zone_decl.method_count,
            ctx,
            decl->data.zone_decl.name);
        break;

    default:
        break;
    }

    ctx->current_nominal_decl = saved_nominal;
    ctx->current_relation = saved_relation;
    ctx->current_effect = saved_effect;
    ctx->current_zone = saved_zone;
    ctx->current_world = saved_world;
}

