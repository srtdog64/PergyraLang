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

    TEST("graph-backed reverse schedule places providers before generic and authority consumers");
    {
        const char *source =
            "ability Commandable { func Command() -> Void; }\n"
            "subject Player {\n"
            "    action Command(self) -> Void { return; }\n"
            "}\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "func Wrap<T = Int>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "party StrikeTeam {\n"
            "    role slot commander: Commandable\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable\n"
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
            "func Wrap.T",
            "Int",
            "default-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "Int",
            "where-bound lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.player",
            "Commandable",
            "zone authority ability consumer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "party StrikeTeam.commander",
            "Commandable",
            "party role slot ability consumer lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "func Wrap.T",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "zone BattleZone.player",
            "Commandable"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "party StrikeTeam.commander",
            "Commandable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed declaration order handles forward generic and authority consumers");
    {
        const char *source =
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable\n"
            "}\n"
            "party StrikeTeam {\n"
            "    role slot commander: Commandable\n"
            "}\n"
            "func Wrap<T = UserId>(value: T) -> T where T: UserId {\n"
            "    return value;\n"
            "}\n"
            "type UserId = Int;\n"
            "subject Player {\n"
            "    action Command(self) -> Void { return; }\n"
            "}\n"
            "ability Commandable { func Command() -> Void; }\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command() -> Void { return; }\n"
            "    }\n"
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
            "func Wrap.T",
            "UserId",
            "default-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "UserId",
            "where-bound lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "UserId",
            "Int",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.player",
            "Commandable",
            "zone authority ability consumer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "party StrikeTeam.commander",
            "Commandable",
            "party role slot ability consumer lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "func Wrap.T",
            "UserId"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "UserId",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "zone BattleZone.player",
            "Commandable"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "party StrikeTeam.commander",
            "Commandable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers action and intent effect/zone consumers");
    {
        const char *source =
            "effect Alerted for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot alert: Alerted\n"
            "    authority player\n"
            "}\n"
            "subject Player {\n"
            "    action Guard(self) -> Void\n"
            "        causes Alerted\n"
            "        within BattleZone\n"
            "        authorized by self\n"
            "    {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "intent BattlePlan(zone: BattleZone, player: Player) {\n"
            "    step act {\n"
            "        where: BattleZone;\n"
            "        using: zone;\n"
            "        who: player;\n"
            "        causes: Alerted;\n"
            "        authorized by: player;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
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
            "Guard",
            "BattleZone",
            "action within-zone lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "Guard",
            "Alerted",
            "action causes-effect lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "intent BattlePlan.act",
            "BattleZone",
            "intent step where-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "intent BattlePlan.act",
            "Alerted",
            "intent step causes-effect lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "Guard",
            "BattleZone"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "Guard",
            "Alerted"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "intent BattlePlan.act",
            "BattleZone"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "intent BattlePlan.act",
            "Alerted"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers party extends and world systemic consumers");
    {
        const char *source =
            "party BaseTeam { }\n"
            "party EliteTeam extends BaseTeam { }\n"
            "roster CombatRoster {\n"
            "    party slot squad: EliteTeam\n"
            "}\n"
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    roster combat: CombatRoster\n"
            "    zone battle: BattleZone\n"
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
            "EliteTeam",
            "BaseTeam",
            "party extends lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "squad",
            "EliteTeam",
            "roster party lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "combat",
            "CombatRoster",
            "world roster lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "battle",
            "BattleZone",
            "world zone lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "EliteTeam",
            "BaseTeam"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "squad",
            "EliteTeam"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "combat",
            "CombatRoster"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "battle",
            "BattleZone"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers role include generic argument derivation");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "role BaseRole<T = Int> for Player { }\n"
            "role DefaultRole for Player {\n"
            "    include role BaseRole;\n"
            "}\n"
            "role LongRole for Player {\n"
            "    include role BaseRole<Long>;\n"
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
            "role DefaultRole.include",
            "BaseRole",
            "role include lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "role LongRole.include",
            "BaseRole",
            "role include lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "BaseRole",
            "Int",
            "omitted default generic argument lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "BaseRole",
            "Long",
            "provided generic argument lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "role DefaultRole.include",
            "BaseRole"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "role LongRole.include",
            "BaseRole"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "BaseRole",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "BaseRole",
            "Long"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers inline generic constraint consumers");
    {
        const char *source =
            "ability Damageable { }\n"
            "class Box<T: Damageable> { }\n";
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
            "class Box.T",
            "Damageable",
            "generic constraint lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "class Box.T",
            "Damageable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers type alias target consumers");
    {
        const char *source =
            "type UserId = Int;\n"
            "type NameList = List<String>;\n";
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
            "UserId",
            "Int",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "NameList",
            "List",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "NameList",
            "String",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "UserId",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "NameList",
            "List"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "NameList",
            "String"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
