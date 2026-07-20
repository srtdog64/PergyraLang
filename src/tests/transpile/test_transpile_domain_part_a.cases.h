static void
test_ability_role_emit(void)
{
    printf("\n[ability_role_emit]\n");

    TEST("ability emits vtable typedef");
    {
        const char *source =
            "ability Damageable { func TakeDamage(amount: Int) -> Void; }\n"
            "func Main() -> Void { return; }\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        EXPECT(ok);
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "(*TakeDamage)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *self");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("role emits static method and vtable instance");
    {
        const char *source =
            "ability Healable { func Heal() -> Void; }\n"
            "subject Player {}\n"
            "role PlayerHeal for Player {\n"
            "    impl ability Healable { func Heal() -> Void { return; } }\n"
            "}\n"
            "func Main() -> Void { return; }\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        EXPECT(ok);
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerHeal_Heal");
        EXPECT_STR_CONTAINS(ctx->out->data, "Healable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "vtable_instance");
        EXPECT_STR_CONTAINS(ctx->out->data, ".Heal = PlayerHeal_Heal");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
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
        char *impl_param_type_names[1] = { "Int" };
        char *ability_param_type_names[1] = { "T" };
        MIRProgram mir; memset(&mir, 0, sizeof(mir));
        MIRRoutine routine; memset(&routine, 0, sizeof(routine));
        MIRDeclHeader decl_headers[2]; memset(decl_headers, 0, sizeof(decl_headers));
        MIRDeclHeader *role_header = &decl_headers[0];
        MIRDeclHeader *ability_header = &decl_headers[1];
        MIRDeclMethod role_method; memset(&role_method, 0, sizeof(role_method));
        MIRDeclMethod ability_method_meta; memset(&ability_method_meta, 0, sizeof(ability_method_meta));
        MIRDeclRoleImpl role_impl_meta; memset(&role_impl_meta, 0, sizeof(role_impl_meta));
        MIRDeclGenericParam ability_generic; memset(&ability_generic, 0, sizeof(ability_generic));
        mir.abilities = abilities;
        mir.ability_count = 1;
        mir.roles = roles;
        mir.role_count = 1;
        routine.kind = MIR_SCOPE_METHOD;
        routine.name = "BatchMark";
        routine.ast = &impl_method;
        routine.owner_name = "CourierRoute";
        routine.owner_ast_type = AST_ROLE_DECL;
        routine.has_signature = true;
        routine.params = impl_params;
        routine.param_type_names = impl_param_type_names;
        routine.param_count = 1;
        routine.return_type = impl_method.data.func_decl.return_type;
        routine.return_type_name = "String";
        mir.routines = &routine;
        mir.routine_count = 1;
        role_method.name = "BatchMark";
        role_method.owner_name = "CourierRoute";
        role_method.params = impl_params;
        role_method.param_type_names = impl_param_type_names;
        role_method.param_count = 1;
        role_method.return_type = impl_method.data.func_decl.return_type;
        role_method.return_type_name = "String";
        role_method.has_routine = true;
        role_method.routine_index = 0;
        role_impl_meta.owner_name = "CourierRoute";
        role_impl_meta.ability_ref.base_name = pergyra_strdup("BatchReady");
        role_impl_meta.ability_ref.actual_arg_count = 1;
        role_impl_meta.ability_ref.actual_arg_type_names = calloc(1, sizeof(char *));
        role_impl_meta.ability_ref.actual_arg_type_names[0] = pergyra_strdup("Int");
        role_impl_meta.method_start_index = 0;
        role_impl_meta.method_count = 1;
        ability_method_meta.name = "BatchMark";
        ability_method_meta.owner_name = "BatchReady";
        ability_method_meta.params = ability_params;
        ability_method_meta.param_type_names = ability_param_type_names;
        ability_method_meta.param_count = 1;
        ability_method_meta.return_type = ability_method.data.func_decl.return_type;
        ability_method_meta.return_type_name = "String";
        ability_generic.name = "T";
        ability_generic.bound_type_name = pergyra_strdup("T");
        role_header->name = "CourierRoute";
        role_header->ast_type = AST_ROLE_DECL;
        role_header->method_count = 1;
        role_header->method_metadata = &role_method;
        role_header->method_metadata_count = 1;
        role_header->role_impl_count = 1;
        role_header->role_impl_metadata = &role_impl_meta;
        role_header->role_impl_metadata_count = 1;
        role_header->uses_pointer_self = true;
        ability_header->name = "BatchReady";
        ability_header->ast_type = AST_ABILITY_DECL;
        ability_header->generic_param_count = 1;
        ability_header->generic_metadata = &ability_generic;
        ability_header->generic_metadata_count = 1;
        ability_header->method_count = 1;
        ability_header->method_metadata = &ability_method_meta;
        ability_header->method_metadata_count = 1;
        mir.decl_headers = decl_headers;
        mir.decl_header_count = 2;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = &mir;
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "BatchReady_Int_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "CourierRoute_BatchReady_Int_vtable_instance");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "BatchReady_Int__vtable");

        transpiler_ctx_destroy(ctx);
        mir_ability_ref_clear(&role_impl_meta.ability_ref);
        free(ability_generic.bound_type_name);
    }

    TEST("role include forwards inherited impls through derived wrappers");
    {
        const char *source =
            "ability Updatable { func Tick() -> Void; }\n"
            "subject Player {}\n"
            "role BaseRole for Player {\n"
            "    impl ability Updatable { func Tick() -> Void { return; } }\n"
            "}\n"
            "role DerivedRole for Player {\n"
            "    include BaseRole;\n"
            "}\n"
            "func Main() -> Void { return; }\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        EXPECT(ok);
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Tick");
        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Updatable_vtable_instance");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
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
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "Damageable";

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

        ASTNode program; memset(&program, 0, sizeof(program));
        ASTNode *stmts[2] = { &ability_node, &party_node };
        program.type = AST_PROGRAM;
        program.data.program.statements = stmts;
        program.data.program.count = 2;

        MIRProgram *mir = mir_program_from_ast(&program);
        MIRDeclHeaderInventory header_inventory;
        const MIRDeclHeader *party_header = NULL;
        g_last_mir = mir;
        mir_decl_header_inventory_from_program(mir, &header_inventory);
        for (size_t i = 0; i < header_inventory.count; i++) {
            const MIRDeclHeader *header =
                mir_decl_header_inventory_get(&header_inventory, i);
            if (header != NULL
                && mir_decl_header_ast_type_or(header, AST_PROGRAM)
                    == AST_PARTY_DECL) {
                party_header = header;
                break;
            }
        }
        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_party_decl_from_mir_header(party_header, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct DungeonTeam");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *tank");
        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "formation");
        EXPECT_STR_CONTAINS(ctx->out->data, "} DungeonTeam;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
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
