static void
test_mir_lowering_part_g(void)
{
    TEST("MIR DCE removes dead pure-query statements while preserving routine validity");
    {
        const char *src =
            "func Probe() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    ChannelLength(ch);\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *probe = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            probe = find_mir_routine(mir, "Probe", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && probe != NULL
               && !probe->used_non_cfg_body_fallback
               && probe->non_cfg_body_fallback_count == 0
               && probe->has_dce
               && probe->dce_removed_count > 0
               && !routine_has_stmt_call_named(probe, "ChannelLength"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects declaration header name metadata drift");
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
                if (header->name != NULL && strcmp(header->name, "Item") == 0) {
                    header->name = NULL;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "declaration name metadata") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve pointer-self ABI shape");
    {
        const char *src =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func Read(self) -> Int { return hp; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *player = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                if (mir->decl_headers[i].name != NULL
                    && strcmp(mir->decl_headers[i].name, "Player") == 0) {
                    player = &mir->decl_headers[i];
                    break;
                }
            }
        }
        EXPECT(ok
               && player != NULL
               && player->uses_pointer_self
               && player->method_count == 1
               && player->method_metadata_count == 1
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pointer-self ABI metadata drift");
    {
        const char *src =
            "subject Player { }\n"
            "zone Handle {\n"
            "    subject slot player: Player\n"
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
                if (header->name != NULL && strcmp(header->name, "Handle") == 0) {
                    header->uses_pointer_self = false;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pointer-self ABI metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects zone state metadata drift");
    {
        const char *src =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *zone = NULL;
        size_t saved_state_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "BattleZone") == 0) {
                    zone = header;
                    saved_state_metadata_count =
                        header->zone_state_metadata_count;
                    header->zone_state_metadata_count = 0;
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
                             "zone state metadata count") != NULL;
        if (zone != NULL)
            zone->zone_state_metadata_count =
                saved_state_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects world state metadata drift");
    {
        const char *src =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state inner: zone battle\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *world = NULL;
        size_t saved_state_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "GameWorld") == 0) {
                    world = header;
                    saved_state_metadata_count =
                        header->world_state_metadata_count;
                    header->world_state_metadata_count = 0;
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
                             "world state metadata count") != NULL;
        if (world != NULL)
            world->world_state_metadata_count =
                saved_state_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects world directive metadata drift");
    {
        const char *src =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state inner: zone battle\n"
            "    activate inner\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *world = NULL;
        size_t saved_directive_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "GameWorld") == 0) {
                    world = header;
                    saved_directive_metadata_count =
                        header->world_directive_metadata_count;
                    header->world_directive_metadata_count = 0;
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
                             "world directive metadata count") != NULL;
        if (world != NULL)
            world->world_directive_metadata_count =
                saved_directive_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects unknown world directive target");
    {
        const char *src =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state inner: zone battle\n"
            "    activate inner\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *world = NULL;
        MIRDeclWorldDirective *directive = NULL;
        char *saved_state_name = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "GameWorld") == 0) {
                    world = header;
                    directive = header->world_directive_metadata_count == 1
                        ? &header->world_directive_metadata[0] : NULL;
                    break;
                }
            }
        }
        if (directive != NULL) {
            saved_state_name = directive->state_name;
            directive->state_name = "missing";
            mutated = true;
        }
        rejected = ok
                   && world != NULL
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "references unknown state or zone slot") != NULL;
        if (directive != NULL)
            directive->state_name = saved_state_name;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects unknown world state input reference");
    {
        const char *src =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state inner: zone battle\n"
            "    state ready: any inner\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclWorldState *ready = NULL;
        char *saved_input_name = NULL;
        char *mir_error = NULL;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "GameWorld") == 0
                    && header->world_state_metadata_count == 2) {
                    ready = &header->world_state_metadata[1];
                    if (ready->input_names != NULL
                        && ready->input_count == 1) {
                        saved_input_name = ready->input_names[0];
                        ready->input_names[0] = "missing";
                    }
                    break;
                }
            }
        }
        rejected = ok
                   && saved_input_name != NULL
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "references unknown input 'missing'") != NULL;
        if (ready != NULL && saved_input_name != NULL)
            ready->input_names[0] = saved_input_name;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
