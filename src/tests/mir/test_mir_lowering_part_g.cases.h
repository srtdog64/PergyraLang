static void
test_mir_lowering_part_g(void)
{
    TEST("MIR DCE removes dead pure-query statements while preserving routine validity");
    {
        const char *src =
            "func Probe(ch: Channel<Int>) -> Int {\n"
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
}
