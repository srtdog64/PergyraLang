static void
test_mir_lowering_part_h_2(void)
{
    TEST("MIR validator rejects hosted method signature metadata drift");
    {
        const char *src =
            "enum Status {\n"
            "    Idle,\n"
            "    Busy,\n"
            "    func Code(self) -> Int { return 7; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclMethod *mutated_method = NULL;
        char **saved_param_type_names = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "Status") == 0
                    && header->method_metadata_count > 0) {
                    mutated_method = &header->method_metadata[0];
                    saved_param_type_names = mutated_method->param_type_names;
                    mutated_method->param_type_names = NULL;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "type-name storage") != NULL;
        if (mutated_method != NULL)
            mutated_method->param_type_names = saved_param_type_names;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects zone authority metadata drift");
    {
        const char *src =
            "subject Buyer { }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *zone = NULL;
        size_t saved_authority_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "PaymentZone") == 0) {
                    zone = header;
                    saved_authority_metadata_count =
                        header->zone_authority_metadata_count;
                    header->zone_authority_metadata_count = 0;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "zone authority metadata count") != NULL;
        if (zone != NULL)
            zone->zone_authority_metadata_count =
                saved_authority_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects zone refresh metadata drift");
    {
        const char *src =
            "subject Buyer { let hp: Int; }\n"
            "object BuyerCard { totalHp: Int; }\n"
            "zone Lobby {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot card: BuyerCard\n"
            "    refresh card from buyer map { totalHp <- hp; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *zone = NULL;
        size_t saved_refresh_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "Lobby") == 0) {
                    zone = header;
                    saved_refresh_metadata_count =
                        header->zone_refresh_metadata_count;
                    header->zone_refresh_metadata_count = 0;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "zone refresh metadata count") != NULL;
        if (zone != NULL)
            zone->zone_refresh_metadata_count =
                saved_refresh_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects hosted method routine link metadata drift");
    {
        const char *src =
            "class Item {\n"
            "    func Code(self) -> Int { return 7; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name == NULL
                    || strcmp(header->name, "Item") != 0
                    || header->method_metadata_count == 0) {
                    continue;
                }
                MIRDeclMethod *method = &header->method_metadata[0];
                if (method->has_routine
                    && method->routine_index < mir->routine_count) {
                    mir->routines[method->routine_index].name = "OtherCode";
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "routine link metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR method routine linker requires owner metadata");
    {
        MIRDeclMethod method = { 0 };
        MIRDeclHeader header = { 0 };
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };

        method.name = "Code";
        method.owner_name = "Item";
        header.name = "Item";
        header.method_count = 1;
        header.method_metadata = &method;
        header.method_metadata_count = 1;
        routine.name = "Code";
        routine.owner_name = NULL;
        routine.kind = MIR_SCOPE_METHOD;
        mir.decl_headers = &header;
        mir.decl_header_count = 1;
        mir.routines = &routine;
        mir.routine_count = 1;
        mir_link_decl_method_routines(&mir);
        EXPECT(!method.has_routine);
    }
}
