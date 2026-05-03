static void
test_ability_role_emit(void)
{
    printf("\n[ability_role_emit]\n");

    TEST("ability emits vtable typedef");
    {
        /* Build ability with one method manually (no ast_destroy -> manual free) */
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "Damageable";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "amount";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "TakeDamage";
        method.data.func_decl.params = params;
        method.data.func_decl.param_count = 1;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        ability_node.data.ability_decl.methods = methods;
        ability_node.data.ability_decl.method_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_ability_decl(&ability_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "(*TakeDamage)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *self");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role emits static method and vtable instance");
    {
        ASTNode role_node; memset(&role_node, 0, sizeof(role_node));
        role_node.type = AST_ROLE_DECL;
        role_node.data.role_decl.name = "PlayerHeal";

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Heal";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *impl_methods[1] = { &method };
        ASTNode impl_node; memset(&impl_node, 0, sizeof(impl_node));
        impl_node.type = AST_IMPL_ABILITY;
        impl_node.data.impl_ability.ability_ref = ast_create_type("Healable");
        impl_node.data.impl_ability.methods = impl_methods;
        impl_node.data.impl_ability.method_count = 1;

        ASTNode *impls[1] = { &impl_node };
        role_node.data.role_decl.impl_abilities = impls;
        role_node.data.role_decl.impl_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerHeal_Heal");
        EXPECT_STR_CONTAINS(ctx->out->data, "Healable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "vtable_instance");
        EXPECT_STR_CONTAINS(ctx->out->data, ".Heal = PlayerHeal_Heal");

        transpiler_ctx_destroy(ctx);
    }

    TEST("generic ability specialization emits canonical vtable tag");
    {
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "BatchReady";
        ability_node.data.ability_decl.generic_params = calloc(1, sizeof(GenericParams));
        ability_node.data.ability_decl.generic_params->count = 1;
        ability_node.data.ability_decl.generic_params->params = calloc(1, sizeof(GenericParam *));
        GenericParam *ability_gp = calloc(1, sizeof(GenericParam));
        ability_gp->name = pergyra_strdup("T");
        ability_gp->constraint = ast_create_type("T");
        ability_node.data.ability_decl.generic_params->params[0] = ability_gp;

        FuncParam ability_param; memset(&ability_param, 0, sizeof(ability_param));
        ability_param.name = "batch";
        ability_param.type = make_type_node("T");
        FuncParam *ability_params[1] = { &ability_param };

        ASTNode ability_method; memset(&ability_method, 0, sizeof(ability_method));
        ability_method.type = AST_FUNC_DECL;
        ability_method.data.func_decl.name = "BatchMark";
        ability_method.data.func_decl.params = ability_params;
        ability_method.data.func_decl.param_count = 1;
        ability_method.data.func_decl.return_type = make_type_node("String");
        ability_method.data.func_decl.body = NULL;

        ASTNode *ability_methods[1] = { &ability_method };
        ability_node.data.ability_decl.methods = ability_methods;
        ability_node.data.ability_decl.method_count = 1;

        ASTNode role_node; memset(&role_node, 0, sizeof(role_node));
        role_node.type = AST_ROLE_DECL;
        role_node.data.role_decl.name = "CourierRoute";
        role_node.data.role_decl.for_type = make_type_node("Int");

        FuncParam impl_param; memset(&impl_param, 0, sizeof(impl_param));
        impl_param.name = "batch";
        impl_param.type = make_type_node("Int");
        FuncParam *impl_params[1] = { &impl_param };

        ASTNode impl_method; memset(&impl_method, 0, sizeof(impl_method));
        impl_method.type = AST_FUNC_DECL;
        impl_method.data.func_decl.name = "BatchMark";
        impl_method.data.func_decl.params = impl_params;
        impl_method.data.func_decl.param_count = 1;
        impl_method.data.func_decl.return_type = make_type_node("String");
        impl_method.data.func_decl.body = NULL;

        ASTNode *impl_methods[1] = { &impl_method };
        ASTNode impl_node; memset(&impl_node, 0, sizeof(impl_node));
        impl_node.type = AST_IMPL_ABILITY;
        impl_node.data.impl_ability.ability_ref = make_generic_type("BatchReady", "Int");
        impl_node.data.impl_ability.methods = impl_methods;
        impl_node.data.impl_ability.method_count = 1;

        ASTNode *impls[1] = { &impl_node };
        role_node.data.role_decl.impl_abilities = impls;
        role_node.data.role_decl.impl_count = 1;

        ASTNode *abilities[1] = { &ability_node };
        ASTNode *roles[1] = { &role_node };
        MIRProgram mir; memset(&mir, 0, sizeof(mir));
        MIRRoutine routine; memset(&routine, 0, sizeof(routine));
        MIRDeclHeader role_header; memset(&role_header, 0, sizeof(role_header));
        MIRDeclMethod role_method; memset(&role_method, 0, sizeof(role_method));
        mir.abilities = abilities;
        mir.ability_count = 1;
        mir.roles = roles;
        mir.role_count = 1;
        routine.kind = MIR_SCOPE_METHOD;
        routine.name = "BatchMark";
        routine.ast = &impl_method;
        routine.owner_name = "CourierRoute";
        routine.owner_ast_type = AST_ROLE_DECL;
        mir.routines = &routine;
        mir.routine_count = 1;
        role_method.ast = &impl_method;
        role_method.name = "BatchMark";
        role_method.owner_name = "CourierRoute";
        role_method.params = impl_params;
        role_method.param_count = 1;
        role_method.return_type = impl_method.data.func_decl.return_type;
        role_method.has_routine = true;
        role_method.routine_index = 0;
        role_header.name = "CourierRoute";
        role_header.ast = &role_node;
        role_header.ast_type = AST_ROLE_DECL;
        role_header.method_metadata = &role_method;
        role_header.method_metadata_count = 1;
        role_header.uses_pointer_self = true;
        mir.decl_headers = &role_header;
        mir.decl_header_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = &mir;
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "BatchReady_Int_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "CourierRoute_BatchReady_Int_vtable_instance");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "BatchReady_Int__vtable");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role include copies inherited impls into current role");
    {
        ASTNode base_method; memset(&base_method, 0, sizeof(base_method));
        base_method.type = AST_FUNC_DECL;
        base_method.data.func_decl.name = "Tick";
        base_method.data.func_decl.return_type = make_type_node("Void");

        ASTNode *base_methods[1] = { &base_method };
        ASTNode base_impl; memset(&base_impl, 0, sizeof(base_impl));
        base_impl.type = AST_IMPL_ABILITY;
        base_impl.data.impl_ability.ability_ref = ast_create_type("Updatable");
        base_impl.data.impl_ability.methods = base_methods;
        base_impl.data.impl_ability.method_count = 1;

        ASTNode *base_impls[1] = { &base_impl };
        ASTNode base_role; memset(&base_role, 0, sizeof(base_role));
        base_role.type = AST_ROLE_DECL;
        base_role.data.role_decl.name = "BaseRole";
        base_role.data.role_decl.impl_abilities = base_impls;
        base_role.data.role_decl.impl_count = 1;

        ASTNode include_stmt; memset(&include_stmt, 0, sizeof(include_stmt));
        include_stmt.type = AST_INCLUDE_STMT;
        include_stmt.data.include_stmt.role_name = "BaseRole";

        ASTNode *includes[1] = { &include_stmt };
        ASTNode derived_role; memset(&derived_role, 0, sizeof(derived_role));
        derived_role.type = AST_ROLE_DECL;
        derived_role.data.role_decl.name = "DerivedRole";
        derived_role.data.role_decl.includes = includes;
        derived_role.data.role_decl.include_count = 1;

        ASTNode *roles[2] = { &base_role, &derived_role };
        MIRProgram mir; memset(&mir, 0, sizeof(mir));
        mir.roles = roles;
        mir.role_count = 2;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = &mir;
        emit_role_decl(&derived_role, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Tick");
        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Updatable_vtable_instance");

        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Party codegen
 * ----------------------------------------------------------------- */

static void
test_party_emit(void)
{
    printf("\n[party_emit]\n");

    TEST("party emits struct with role slot and shared field");
    {
        ASTNode party_node; memset(&party_node, 0, sizeof(party_node));
        party_node.type = AST_PARTY_DECL;
        party_node.data.party_decl.name = "DungeonTeam";

        /* Role slot */
        ASTNode rs; memset(&rs, 0, sizeof(rs));
        rs.type = AST_ROLE_SLOT;
        rs.data.role_slot.slot_name = "tank";
        ASTNode ab_type; memset(&ab_type, 0, sizeof(ab_type));
        ab_type.type = AST_TYPE;
        ab_type.data.type.name = "Damageable";
        ASTNode *abilities[1] = { &ab_type };
        rs.data.role_slot.required_abilities = abilities;
        rs.data.role_slot.ability_count = 1;

        ASTNode *role_slots[1] = { &rs };
        party_node.data.party_decl.role_slots = role_slots;
        party_node.data.party_decl.role_count = 1;

        /* Shared field */
        ASTNode shared; memset(&shared, 0, sizeof(shared));
        shared.type = AST_PARTY_SHARED;
        shared.data.party_shared.name = "formation";
        shared.data.party_shared.type = make_type_node("String");

        ASTNode *shared_fields[1] = { &shared };
        party_node.data.party_decl.shared_fields = shared_fields;
        party_node.data.party_decl.shared_count = 1;

        party_node.data.party_decl.methods = NULL;
        party_node.data.party_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_party_decl(&party_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct DungeonTeam");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *tank");
        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "formation");
        EXPECT_STR_CONTAINS(ctx->out->data, "} DungeonTeam;");

        transpiler_ctx_destroy(ctx);
    }

    TEST("party instance emits C compound literal");
    {
        ASTNode value; memset(&value, 0, sizeof(value));
        value.type = AST_IDENTIFIER;
        value.data.identifier.name = "tankRole";

        ASTNode instance; memset(&instance, 0, sizeof(instance));
        instance.type = AST_PARTY_INSTANCE;
        instance.data.party_instance.party_type = "DungeonTeam";
        instance.data.party_instance.assignments = calloc(1, sizeof(*instance.data.party_instance.assignments));
        instance.data.party_instance.assignment_count = 1;
        instance.data.party_instance.assignments[0].slot_name = "tank";
        instance.data.party_instance.assignments[0].value = &value;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&instance, ctx);

        EXPECT_STR_CONTAINS(result, "(DungeonTeam){");
        EXPECT_STR_CONTAINS(result, ".tank = tankRole");

        free(result);
        free(instance.data.party_instance.assignments);
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Roster / World codegen
 * ----------------------------------------------------------------- */
