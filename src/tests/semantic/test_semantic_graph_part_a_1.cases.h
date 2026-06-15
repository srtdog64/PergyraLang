static size_t
type_resolution_graph_count_kind(const TypeResolutionGraph *graph,
                                 TypeResolutionNodeKind kind)
{
    size_t count = 0;

    if (graph == NULL)
        return 0;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == kind)
            count++;
    }

    return count;
}

static bool
type_resolution_graph_has_label_substring(const TypeResolutionGraph *graph,
                                          TypeResolutionNodeKind kind,
                                          const char *needle)
{
    if (graph == NULL || needle == NULL)
        return false;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind != kind)
            continue;
        if (graph->nodes[i].label != NULL
            && strstr(graph->nodes[i].label, needle) != NULL) {
            return true;
        }
    }

    return false;
}

static bool
type_resolution_graph_has_edge_between(const TypeResolutionGraph *graph,
                                       const char *from_needle,
                                       const char *to_needle,
                                       const char *reason_needle)
{
    if (graph == NULL)
        return false;

    for (size_t i = 0; i < graph->edge_count; i++) {
        const TypeResolutionEdge *edge = &graph->edges[i];
        const char *from_label = NULL;
        const char *to_label = NULL;
        const char *reason = edge->reason;

        if (edge->from >= graph->node_count || edge->to >= graph->node_count)
            continue;

        from_label = graph->nodes[edge->from].label;
        to_label = graph->nodes[edge->to].label;

        if (from_needle != NULL
            && (from_label == NULL || strstr(from_label, from_needle) == NULL)) {
            continue;
        }
        if (to_needle != NULL
            && (to_label == NULL || strstr(to_label, to_needle) == NULL)) {
            continue;
        }
        if (reason_needle != NULL
            && (reason == NULL || strstr(reason, reason_needle) == NULL)) {
            continue;
        }

        return true;
    }

    return false;
}

static bool
type_resolution_graph_build_topo_order_local(const TypeResolutionGraph *graph,
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
            const TypeResolutionEdge *edge = &graph->edges[i];
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

static bool
type_resolution_graph_provider_before_consumer_schedule(
    const TypeResolutionGraph *graph,
    const char *consumer_needle,
    const char *provider_needle)
{
    size_t *order = NULL;
    size_t count = 0;
    size_t consumer_index = (size_t)-1;
    size_t provider_index = (size_t)-1;
    size_t consumer_pos = (size_t)-1;
    size_t provider_pos = (size_t)-1;
    bool ok = false;

    if (graph == NULL || consumer_needle == NULL || provider_needle == NULL)
        return false;
    if (!type_resolution_graph_build_topo_order_local(graph, &order, &count))
        return false;

    for (size_t i = 0; i < graph->edge_count; i++) {
        const TypeResolutionEdge *edge = &graph->edges[i];
        const char *from_label = NULL;
        const char *to_label = NULL;

        if (edge->from >= graph->node_count || edge->to >= graph->node_count)
            continue;

        from_label = graph->nodes[edge->from].label;
        to_label = graph->nodes[edge->to].label;
        if (from_label == NULL || to_label == NULL)
            continue;
        if (strstr(from_label, consumer_needle) == NULL)
            continue;
        if (strstr(to_label, provider_needle) == NULL)
            continue;

        consumer_index = edge->from;
        provider_index = edge->to;
        break;
    }

    if (consumer_index == (size_t)-1 || provider_index == (size_t)-1) {
        free(order);
        return false;
    }

    for (size_t pos = 0; pos < count; pos++) {
        size_t node_index = order[pos];

        if (consumer_pos == (size_t)-1 && node_index == consumer_index) {
            consumer_pos = pos;
        }
        if (provider_pos == (size_t)-1 && node_index == provider_index) {
            provider_pos = pos;
        }
    }

    if (consumer_pos != (size_t)-1 && provider_pos != (size_t)-1)
        ok = provider_pos > consumer_pos;

    free(order);
    return ok;
}

static void
test_type_resolution_graph(void)
{
    printf("\n[type_resolution_graph]\n");

    TEST("graph captures local contract and projection path nodes for zone/world contracts");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { totalHp: Int; label: String; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player map {\n"
            "        totalHp <- hp;\n"
            "        label <- name;\n"
            "    }\n"
            "    maintain poisoned\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleProjected: zone battle projection playerView\n"
            "    state battleLayered: zone battle layer poison\n"
            "    state battlePoisoned: zone battle state poisoned\n"
            "    activate battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(graph->node_count > 0);
        EXPECT(type_resolution_graph_count_kind(graph, TYPE_RES_NODE_DECL) > 0);
        EXPECT(type_resolution_graph_count_kind(graph,
               TYPE_RES_NODE_LOCAL_CONTRACT) > 0);
        EXPECT(type_resolution_graph_count_kind(graph,
               TYPE_RES_NODE_PROJECTION_PATH) > 0);
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_LOCAL_CONTRACT,
               "world GameWorld.state.battleProjected"));
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_LOCAL_CONTRACT,
               "world GameWorld.state.battlePoisoned"));
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_LOCAL_CONTRACT,
               "zone BattleZone.state.poisoned"));
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_PROJECTION_PATH,
               "zone BattleZone.projection.playerView"));
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_PROJECTION_PATH,
               "zone BattleZone.slot.playerView.field.totalHp"));
        EXPECT(type_resolution_graph_has_label_substring(graph,
               TYPE_RES_NODE_PROJECTION_PATH,
               "zone BattleZone.slot.player.field.name"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.battleProjected",
            "world GameWorld.zone.battle",
            "world state zone-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.battleProjected",
            "zone BattleZone.slot.playerView",
            "world state projection lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.battleLayered",
            "zone BattleZone.layer.poison",
            "world state layer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.battlePoisoned",
            "zone BattleZone.state.poisoned",
            "world state nested-state lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.state.poisoned",
            "zone BattleZone.layer.poison",
            "zone state layer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.state.poisoned",
            "zone BattleZone.slot.player",
            "zone state target-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-state.poisoned",
            "zone BattleZone.state.poisoned",
            "zone maintain-state lookup"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph captures world composed-state dependencies across local contracts");
    {
        const char *source =
            "zone BattleZone { }\n"
            "zone CampZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: CampZone\n"
            "    state battleLive: zone battle\n"
            "    state campLive: zone camp\n"
            "    state readiness: all battleLive, campLive\n"
            "    state fallback: any battleLive, campLive\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_label_substring(
            graph, TYPE_RES_NODE_LOCAL_CONTRACT,
            "world GameWorld.state.readiness"));
        EXPECT(type_resolution_graph_has_label_substring(
            graph, TYPE_RES_NODE_LOCAL_CONTRACT,
            "world GameWorld.state.fallback"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.readiness",
            "world GameWorld.state.battleLive",
            "world state composition input lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.state.readiness",
            "world GameWorld.state.campLive",
            "world state composition input lookup"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph records world lifecycle edges for state and zone targets");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain liveBattle\n"
            "    deactivate liveBattle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.activate.liveBattle",
            "world GameWorld.state.liveBattle",
            "world activate state lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.maintain.liveBattle",
            "world GameWorld.state.liveBattle",
            "world maintain state lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "world GameWorld.deactivate.liveBattle",
            "world GameWorld.state.liveBattle",
            "world deactivate state lookup"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
