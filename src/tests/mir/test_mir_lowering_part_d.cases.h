static void
test_mir_lowering_part_d(void)
{
    TEST("MIR declaration headers preserve field metadata");
    {
        const char *src =
            "class Item {\n"
            "    let value: Int;\n"
            "    let label: String;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *item = NULL;
        const MIRDeclField *value = NULL;
        const MIRDeclField *label = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            item = mir_find_decl_header(mir, "Item");
        if (item != NULL) {
            value = mir_decl_header_field(item, 0);
            label = mir_decl_header_field(item, 1);
        }
        EXPECT(ok
               && item != NULL
               && mir_decl_header_field_count(item) == 2
               && value != NULL
               && label != NULL
               && mir_decl_field_kind_or(value, MIR_DECL_FIELD_UNKNOWN)
                    == MIR_DECL_FIELD_CLASS
               && mir_decl_field_name(value) != NULL
               && strcmp(mir_decl_field_name(value), "value") == 0
               && mir_decl_field_type(value) != NULL
               && ast_type_name(mir_decl_field_type(value)) != NULL
               && strcmp(ast_type_name(mir_decl_field_type(value)), "Int") == 0
               && mir_decl_field_name(label) != NULL
               && strcmp(mir_decl_field_name(label), "label") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve role-slot ability refs");
    {
        const char *src =
            "subject IntBag { let item: Int; }\n"
            "subject TextBag { let item: String; }\n"
            "ability Bufferable<T = Int> {\n"
            "    func Put(self, value: T) -> Int;\n"
            "}\n"
            "role IntBuffer for IntBag {\n"
            "    impl ability Bufferable {\n"
            "        func Put(self, value: Int) -> Int { return value; }\n"
            "    }\n"
            "}\n"
            "role TextBuffer for TextBag {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(self, value: String) -> Int { return 1; }\n"
            "    }\n"
            "}\n"
            "party StorageParty {\n"
            "    role slot defaulted: Bufferable\n"
            "    role slot explicit: Bufferable<String>\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *party = NULL;
        const MIRDeclField *defaulted = NULL;
        const MIRDeclField *explicit_slot = NULL;
        const MIRAbilityRef *default_ref = NULL;
        const MIRAbilityRef *explicit_ref = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            party = mir_find_decl_header(mir, "StorageParty");
        if (party != NULL) {
            defaulted = mir_decl_header_field(party, 0);
            explicit_slot = mir_decl_header_field(party, 1);
        }
        if (defaulted != NULL)
            default_ref = mir_decl_field_required_ability_ref(defaulted, 0);
        if (explicit_slot != NULL)
            explicit_ref =
                mir_decl_field_required_ability_ref(explicit_slot, 0);
        EXPECT(ok
               && party != NULL
               && mir_decl_header_field_count(party) == 2
               && default_ref != NULL
               && explicit_ref != NULL
               && mir_decl_field_required_ability_count(defaulted) == 1
               && mir_ability_ref_base_name(default_ref) != NULL
               && strcmp(mir_ability_ref_base_name(default_ref),
                         "Bufferable") == 0
               && mir_ability_ref_actual_arg_count(default_ref) == 0
               && mir_ability_ref_base_name(explicit_ref) != NULL
               && strcmp(mir_ability_ref_base_name(explicit_ref),
                         "Bufferable") == 0
               && mir_ability_ref_actual_arg_count(explicit_ref) == 1
               && mir_ability_ref_actual_arg_type_name(explicit_ref, 0)
                    != NULL
               && strcmp(mir_ability_ref_actual_arg_type_name(
                             explicit_ref, 0),
                         "String") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve ability method metadata");
    {
        const char *src =
            "ability Bufferable<T = Int> {\n"
            "    func Put(self, value: T) -> Int;\n"
            "    func Size(self) -> Int;\n"
            "}\n"
            "func Main() -> Void { }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *ability = NULL;
        const MIRDeclMethod *put = NULL;
        const MIRDeclMethod *size = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            ability = mir_find_decl_header_of_type(
                mir, AST_ABILITY_DECL, "Bufferable");
        if (ability != NULL) {
            put = mir_decl_header_method(ability, 0);
            size = mir_decl_header_method(ability, 1);
        }
        EXPECT(ok
               && ability != NULL
               && mir_decl_header_ast_type_or(ability, AST_PROGRAM)
                    == AST_ABILITY_DECL
               && mir_decl_header_method_count(ability) == 2
               && put != NULL
               && mir_decl_method_name(put) != NULL
               && strcmp(mir_decl_method_name(put), "Put") == 0
               && mir_decl_method_param_count(put) == 2
               && mir_decl_method_param_type_name(put, 1) != NULL
               && strcmp(mir_decl_method_param_type_name(put, 1), "T") == 0
               && mir_decl_method_return_type_name(put) != NULL
               && strcmp(mir_decl_method_return_type_name(put), "Int") == 0
               && size != NULL
               && mir_decl_method_name(size) != NULL
               && strcmp(mir_decl_method_name(size), "Size") == 0
               && mir_decl_method_return_type_name(size) != NULL
               && strcmp(mir_decl_method_return_type_name(size), "Int") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve action authorization metadata");
    {
        const char *src =
            "subject Runner {\n"
            "    action Execute(self) -> Int within RunZone authorized by self {\n"
            "        return 1;\n"
            "    }\n"
            "}\n"
            "zone RunZone {\n"
            "    subject slot runner: Runner\n"
            "    authority runner\n"
            "}\n"
            "func Main() -> Void { }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *runner = NULL;
        const MIRDeclMethod *execute = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            runner = mir_find_decl_header_of_type(
                mir, AST_CLASS_DECL, "Runner");
        if (runner != NULL)
            execute = mir_decl_header_method(runner, 0);
        EXPECT(ok
               && execute != NULL
               && mir_decl_method_is_action_like(execute)
               && mir_decl_method_authorized_by_count(execute) == 1
               && mir_decl_method_authorized_by(execute, 0) != NULL
               && strcmp(mir_decl_method_authorized_by(execute, 0),
                         "self") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve role impl method spans");
    {
        const char *src =
            "subject Runner {}\n"
            "ability Tickable { func Tick(self) -> Int; }\n"
            "ability Movable {\n"
            "    func Move(self, amount: Int) -> Int;\n"
            "    func Stop(self) -> Int;\n"
            "}\n"
            "role Route for Runner {\n"
            "    impl ability Tickable {\n"
            "        func Tick(self) -> Int { return 1; }\n"
            "    }\n"
            "    impl ability Movable {\n"
            "        func Move(self, amount: Int) -> Int { return amount; }\n"
            "        func Stop(self) -> Int { return 0; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void { }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *role = NULL;
        const MIRDeclRoleImpl *tick_impl = NULL;
        const MIRDeclRoleImpl *move_impl = NULL;
        const MIRDeclMethod *tick_method = NULL;
        const MIRDeclMethod *move_method = NULL;
        const MIRDeclMethod *stop_method = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            role = mir_find_decl_header_of_type(mir, AST_ROLE_DECL, "Route");
        if (role != NULL) {
            tick_impl = mir_decl_header_role_impl(role, 0);
            move_impl = mir_decl_header_role_impl(role, 1);
        }
        if (tick_impl != NULL)
            tick_method = mir_decl_header_role_impl_method(role, tick_impl, 0);
        if (move_impl != NULL) {
            move_method = mir_decl_header_role_impl_method(role, move_impl, 0);
            stop_method = mir_decl_header_role_impl_method(role, move_impl, 1);
        }
        EXPECT(ok
               && role != NULL
               && mir_decl_header_role_impl_count(role) == 2
               && mir_decl_header_method_count(role) == 3
               && tick_impl != NULL
               && mir_decl_role_impl_method_start_index(tick_impl) == 0
               && mir_decl_role_impl_method_count(tick_impl) == 1
               && move_impl != NULL
               && mir_decl_role_impl_method_start_index(move_impl) == 1
               && mir_decl_role_impl_method_count(move_impl) == 2
               && tick_method != NULL
               && mir_decl_method_name(tick_method) != NULL
               && strcmp(mir_decl_method_name(tick_method), "Tick") == 0
               && move_method != NULL
               && mir_decl_method_name(move_method) != NULL
               && strcmp(mir_decl_method_name(move_method), "Move") == 0
               && stop_method != NULL
               && mir_decl_method_name(stop_method) != NULL
               && strcmp(mir_decl_method_name(stop_method), "Stop") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve role include metadata");
    {
        const char *src =
            "subject Runner {}\n"
            "ability Stepper { func Step(self) -> Int; }\n"
            "role BaseRole for Runner {\n"
            "    impl ability Stepper {\n"
            "        func Step(self) -> Int { return 1; }\n"
            "    }\n"
            "}\n"
            "role DerivedRole for Runner {\n"
            "    include BaseRole;\n"
            "}\n"
            "func Main() -> Void { }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *role = NULL;
        const MIRDeclRoleInclude *include = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            role = mir_find_decl_header_of_type(
                mir, AST_ROLE_DECL, "DerivedRole");
        if (role != NULL)
            include = mir_decl_header_role_include(role, 0);
        EXPECT(ok
               && role != NULL
               && mir_decl_header_role_include_count(role) == 1
               && include != NULL
               && mir_decl_role_include_owner_name(include) != NULL
               && strcmp(mir_decl_role_include_owner_name(include),
                         "DerivedRole") == 0
               && mir_decl_role_include_name(include) != NULL
               && strcmp(mir_decl_role_include_name(include),
                         "BaseRole") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects role include metadata drift");
    {
        const char *src =
            "subject Runner {}\n"
            "role BaseRole for Runner { }\n"
            "role DerivedRole for Runner {\n"
            "    include BaseRole;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *role = NULL;
        char *mir_error = NULL;
        size_t saved_include_metadata_count = 0;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            role = (MIRDeclHeader *)mir_find_decl_header_of_type(
                mir, AST_ROLE_DECL, "DerivedRole");
            if (role != NULL && role->role_include_metadata_count == 1) {
                saved_include_metadata_count =
                    role->role_include_metadata_count;
                role->role_include_metadata_count = 0;
                mutated = true;
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "role include metadata count") != NULL;
        if (role != NULL)
            role->role_include_metadata_count =
                saved_include_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve zone authority ability refs");
    {
        const char *src =
            "subject Buyer { let name: String; }\n"
            "ability Payable<T = Int> {\n"
            "    func Pay(self, value: T) -> Int;\n"
            "}\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable<String> {\n"
            "        func Pay(self, value: String) -> Int { return 1; }\n"
            "    }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable<String>\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *zone = NULL;
        const MIRDeclZoneAuthority *authority = NULL;
        const MIRAbilityRef *ability = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            zone = mir_find_decl_header(mir, "PaymentZone");
        if (zone != NULL)
            authority = mir_decl_header_zone_authority(zone, 0);
        if (authority != NULL)
            ability =
                mir_decl_zone_authority_required_ability_ref(authority, 0);
        EXPECT(ok
               && zone != NULL
               && mir_decl_header_zone_authority_count(zone) == 1
               && authority != NULL
               && mir_decl_zone_authority_subject_slot_name(authority) != NULL
               && strcmp(mir_decl_zone_authority_subject_slot_name(authority),
                         "buyer") == 0
               && mir_decl_zone_authority_required_ability_count(authority) == 1
               && ability != NULL
               && mir_ability_ref_base_name(ability) != NULL
               && strcmp(mir_ability_ref_base_name(ability), "Payable") == 0
               && mir_ability_ref_actual_arg_count(ability) == 1
               && mir_ability_ref_actual_arg_type_name(ability, 0) != NULL
               && strcmp(mir_ability_ref_actual_arg_type_name(ability, 0),
                         "String") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve zone refresh field maps");
    {
        const char *src =
            "subject Buyer { let hp: Int; let name: String; }\n"
            "object BuyerCard {\n"
            "    let totalHp: Int;\n"
            "    let displayName: String;\n"
            "}\n"
            "zone Lobby {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot card: BuyerCard\n"
            "    refresh card from buyer map {\n"
            "        totalHp <- hp;\n"
            "        displayName <- name;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRDeclHeader *zone = NULL;
        const MIRDeclZoneRefresh *refresh = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL)
            zone = mir_find_decl_header(mir, "Lobby");
        if (zone != NULL)
            refresh = mir_decl_header_zone_refresh(zone, 0);
        EXPECT(ok
               && zone != NULL
               && mir_decl_header_zone_refresh_count(zone) == 1
               && refresh != NULL
               && mir_decl_zone_refresh_owner_name(refresh) != NULL
               && strcmp(mir_decl_zone_refresh_owner_name(refresh),
                         "Lobby") == 0
               && mir_decl_zone_refresh_object_slot_name(refresh) != NULL
               && strcmp(mir_decl_zone_refresh_object_slot_name(refresh),
                         "card") == 0
               && mir_decl_zone_refresh_source_slot_name(refresh) != NULL
               && strcmp(mir_decl_zone_refresh_source_slot_name(refresh),
                         "buyer") == 0
               && !mir_decl_zone_refresh_requires_dto(refresh)
               && !mir_decl_zone_refresh_derives_target_kind(refresh)
               && mir_decl_zone_refresh_field_map_count(refresh) == 2
               && strcmp(mir_decl_zone_refresh_mapped_target_field(
                             refresh, 0),
                         "totalHp") == 0
               && strcmp(mir_decl_zone_refresh_mapped_source_field(
                             refresh, 0),
                         "hp") == 0
               && strcmp(mir_decl_zone_refresh_mapped_target_field(
                             refresh, 1),
                         "displayName") == 0
               && strcmp(mir_decl_zone_refresh_mapped_source_field(
                             refresh, 1),
                         "name") == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

}
