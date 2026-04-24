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
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
                        PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_DECLARE_OR_IMPORT_TYPE,
                        owner != NULL ? owner : tc->bounds[b],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_NON_TRAILING_DEFAULT,
                PGY_FIX_MOVE_DEFAULTS_TO_TRAILING,
                owner != NULL ? owner : (ASTNode *)gp,
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
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                    PGY_CAUSE_GENERIC_DEFAULT_UNRESOLVED,
                    PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                    owner != NULL ? owner : param->default_type,
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE, PGY_CAUSE_TYPE_RESOLUTION_CYCLE, PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION, site,
                "Type resolution dependency cycle detected in the semantic graph around '%s'.\n"
                "Contract source:\n"
                "- graph edge provenance: %s\n"
                "Reason:\n"
                "- provider/consumer resolution formed a closed dependency loop\n"
                "- cycle path: %s\n"
                "Fix:\n"
                "- break the generic/alias/ability dependency loop so one edge resolves first\n"
                "- or split the contract into acyclic declarations",
                graph->nodes[closing_node].label != NULL
                    ? graph->nodes[closing_node].label : "<type-ref>",
                cycle_text != NULL ? cycle_text : "<cycle>",
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
