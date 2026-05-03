#ifndef PERGYRA_MIR_FACT_VALIDATE_H
#define PERGYRA_MIR_FACT_VALIDATE_H

static bool
mir_has_inventory_payload(const MIRProgram *mir)
{
    return mir != NULL
        && (mir->extern_count > 0
            || mir->type_count > 0
            || mir->ability_count > 0
            || mir->role_count > 0
            || mir->party_count > 0
            || mir->roster_count > 0
            || mir->world_count > 0
            || mir->relation_count > 0
            || mir->effect_count > 0
            || mir->zone_count > 0
            || mir->event_count > 0
            || mir->intent_count > 0
            || mir->function_count > 0);
}

static bool
mir_validate_inventory_surface_usage(const MIRProgram *mir, char **error_message)
{
    if (mir == NULL)
        return false;

    if (!mir_has_inventory_payload(mir))
        return true;

    if (!mir->has_inventory_surface_usage_facts) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program is missing inventory surface usage facts");
        }
        return false;
    }

    if (mir->inventory_uses_thread_pool_surface
        != mir_inventory_uses_thread_pool_surface(mir)) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program has stale thread-pool inventory surface usage fact");
        }
        return false;
    }

    if (mir->inventory_uses_intent_observability_surface
        != mir_inventory_uses_intent_observability_surface(mir)) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program has stale intent observability inventory surface usage fact");
        }
        return false;
    }

    return true;
}

static bool
mir_decl_header_ast_shape(const MIRDeclHeader *header,
                          const char **name_out,
                          ASTNode ***methods_out,
                          size_t *method_count_out,
                          bool *uses_pointer_self_out)
{
    ASTNode *ast;

    if (name_out != NULL)
        *name_out = NULL;
    if (methods_out != NULL)
        *methods_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (uses_pointer_self_out != NULL)
        *uses_pointer_self_out = false;
    if (header == NULL || header->ast == NULL)
        return false;

    ast = header->ast;
    switch (ast->type) {
    case AST_CLASS_DECL:
        if (name_out != NULL)
            *name_out = ast->data.class_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.class_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.class_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out =
                ast->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT
                || ast->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
        return true;
    case AST_ENUM_DECL:
        if (name_out != NULL)
            *name_out = ast->data.enum_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.enum_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.enum_decl.method_count;
        return true;
    case AST_PARTY_DECL:
        if (name_out != NULL)
            *name_out = ast->data.party_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.party_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.party_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROLE_DECL:
        if (name_out != NULL)
            *name_out = ast->data.role_decl.name;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROSTER_DECL:
        if (name_out != NULL)
            *name_out = ast->data.roster_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.roster_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.roster_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_WORLD_DECL:
        if (name_out != NULL)
            *name_out = ast->data.world_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.world_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.world_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_RELATION_DECL:
        if (name_out != NULL)
            *name_out = ast->data.relation_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.relation_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.relation_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_EFFECT_DECL:
        if (name_out != NULL)
            *name_out = ast->data.effect_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.effect_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.effect_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ZONE_DECL:
        if (name_out != NULL)
            *name_out = ast->data.zone_decl.name;
        if (methods_out != NULL)
            *methods_out = ast->data.zone_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = ast->data.zone_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    default:
        return false;
    }
}

static bool
mir_validate_decl_header_ast_compat(const MIRDeclHeader *header,
                                    size_t header_index,
                                    char **error_message)
{
    const char *ast_name = NULL;
    ASTNode **ast_methods = NULL;
    size_t ast_method_count = 0;
    bool ast_uses_pointer_self = false;

    if (header == NULL)
        return false;
    if (header->ast == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has no AST compatibility payload",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type != header->ast->type) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST type metadata drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (!mir_decl_header_ast_shape(
            header, &ast_name, &ast_methods, &ast_method_count,
            &ast_uses_pointer_self)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has unsupported declaration AST shape",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->name == NULL
        || ast_name == NULL
        || strcmp(header->name, ast_name) != 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] name metadata drift",
                header_index);
        }
        return false;
    }
    if (header->methods != ast_methods
        || header->method_count != ast_method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST method compatibility drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->uses_pointer_self != ast_uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' pointer-self ABI metadata drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    return true;
}

static bool
mir_validate_decl_method_metadata(const MIRProgram *mir,
                                  const MIRDeclHeader *header,
                                  size_t header_index,
                                  char **error_message)
{
    if (mir == NULL || header == NULL)
        return false;

    if (!mir_validate_decl_header_ast_compat(header, header_index, error_message))
        return false;

    if (header->method_metadata_count > 0 && header->method_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu method metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_metadata_count);
        }
        return false;
    }

    if (header->method_count > 0 && header->method_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu hosted method(s) without MIRDeclMethod metadata",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_count);
        }
        return false;
    }

    if (header->ast_type != AST_ROLE_DECL
        && header->method_metadata_count != header->method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' method metadata count %zu does not match AST compatibility count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_metadata_count,
                header->method_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        ASTNode *ast = method->ast;

        if (header->methods != NULL && method->ast != header->methods[i]) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] AST payload drift",
                    header_index, i);
            }
            return false;
        }

        if (method->owner_name == NULL
            || header->name == NULL
            || strcmp(method->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }

        if (method->has_routine && method->routine_index >= mir->routine_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] routine index %zu exceeds routine count %zu",
                    header_index, i, method->routine_index, mir->routine_count);
            }
            return false;
        }

        if (ast == NULL || ast->type != AST_FUNC_DECL)
            continue;
        if (method->name != ast->data.func_decl.name
            && (method->name == NULL || ast->data.func_decl.name == NULL
                || strcmp(method->name, ast->data.func_decl.name) != 0)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] name metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (method->params != ast->data.func_decl.params
            || method->param_count != ast->data.func_decl.param_count
            || method->return_type != ast->data.func_decl.return_type) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] signature metadata drift",
                    header_index, i);
            }
            return false;
        }
    }

    return true;
}

static bool
mir_validate_decl_header_metadata(const MIRProgram *mir,
                                  char **error_message)
{
    if (mir == NULL)
        return false;

    if (mir->decl_header_count > 0 && mir->decl_headers == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program has %zu declaration header(s) but no declaration header storage",
                mir->decl_header_count);
        }
        return false;
    }

    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *header = &mir->decl_headers[i];
        if (header->name != NULL) {
            for (size_t j = i + 1; j < mir->decl_header_count; j++) {
                const MIRDeclHeader *other = &mir->decl_headers[j];
                if (other->name != NULL && strcmp(header->name, other->name) == 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] '%s' duplicates declaration header[%zu]",
                            j, other->name, i);
                    }
                    return false;
                }
            }
        }
        if (!mir_validate_decl_method_metadata(
                mir, header, i, error_message)) {
            return false;
        }
    }

    return true;
}

static bool
mir_validate_statement_inventory(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (block->source_statement_inventory.count > 0
        && block->source_statement_inventory.items == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] statement inventory has %zu item(s) but no storage",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index,
                block->source_statement_inventory.count);
        }
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (!inst->has_source_statement_index)
            continue;
        if (inst->source_statement_index >= block->source_statement_inventory.count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source statement index %zu exceeds inventory count %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->source_statement_index,
                    block->source_statement_inventory.count);
            }
            return false;
        }
    }

    return true;
}

static bool
mir_validate_instruction_surface_usage(const MIRRoutine *routine,
                                       const MIRBasicBlock *block,
                                       size_t block_index,
                                       char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        bool has_surface_payload = inst->ast != NULL
                                   || inst->expr0 != NULL
                                   || inst->expr1 != NULL
                                   || inst->has_source_location;
        if (!has_surface_payload)
            continue;
        if (!inst->has_surface_usage_facts) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has source payload without surface usage facts",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->uses_thread_pool_surface !=
            (ast_uses_thread_pool_surface(inst->ast)
             || ast_uses_thread_pool_surface(inst->expr0)
             || ast_uses_thread_pool_surface(inst->expr1))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has stale thread-pool surface usage fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->uses_intent_observability_surface !=
            (ast_uses_intent_observability_surface(inst->ast)
             || ast_uses_intent_observability_surface(inst->expr0)
             || ast_uses_intent_observability_surface(inst->expr1))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has stale intent observability surface usage fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}

static bool
mir_validate_terminator_provenance(const MIRRoutine *routine,
                                   const MIRBasicBlock *block,
                                   size_t block_index,
                                   char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_BRANCH && inst->kind != MIR_INST_RETURN)
            continue;
        if (!inst->has_source_terminator_kind) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has CFG terminator without HIR source terminator kind",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && inst->source_terminator_kind != HIR_BLOCK_BRANCH) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch source terminator is not HIR_BLOCK_BRANCH",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RETURN
            && inst->source_terminator_kind != HIR_BLOCK_RETURN) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] return source terminator is not HIR_BLOCK_RETURN",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}

#endif /* PERGYRA_MIR_FACT_VALIDATE_H */
