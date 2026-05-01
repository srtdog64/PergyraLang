#ifndef PGY_MIR_DECL_HEADERS_H
#define PGY_MIR_DECL_HEADERS_H

/* Private MIR declaration-header inventory helpers used by mir_lower(). */

static bool
mir_append_decl_header(MIRProgram *mir, MIRDeclHeader header)
{
    if (mir == NULL)
        return false;
    if (mir->decl_header_count == mir->decl_header_capacity) {
        size_t next_capacity = mir->decl_header_capacity == 0 ? 8 : mir->decl_header_capacity * 2;
        MIRDeclHeader *grown =
            realloc(mir->decl_headers, next_capacity * sizeof(MIRDeclHeader));
        if (grown == NULL)
            return false;
        mir->decl_headers = grown;
        mir->decl_header_capacity = next_capacity;
    }
    mir->decl_headers[mir->decl_header_count++] = header;
    return true;
}

static bool
mir_decl_header_set_methods(MIRDeclHeader *header,
                            ASTNode **methods,
                            size_t method_count)
{
    if (header == NULL)
        return false;

    header->methods = methods;
    header->method_count = method_count;
    header->method_metadata = NULL;
    header->method_metadata_count = 0;

    if (method_count == 0)
        return true;

    header->method_metadata = calloc(method_count, sizeof(MIRDeclMethod));
    if (header->method_metadata == NULL)
        return false;

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        MIRDeclMethod *meta = &header->method_metadata[i];
        meta->ast = method;
        meta->owner_name = header->name;
        if (method != NULL && method->type == AST_FUNC_DECL) {
            meta->name = method->data.func_decl.name;
            meta->params = method->data.func_decl.params;
            meta->param_count = method->data.func_decl.param_count;
            meta->return_type = method->data.func_decl.return_type;
            meta->is_action_like = method->data.func_decl.is_action;
            meta->within_zone = method->data.func_decl.within_zone;
        }
    }
    header->method_metadata_count = method_count;
    return true;
}

static bool
mir_record_decl_header(MIRProgram *mir, ASTNode *decl)
{
    MIRDeclHeader header;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (mir == NULL || decl == NULL)
        return true;

    memset(&header, 0, sizeof(header));
    header.ast = decl;
    header.ast_type = decl->type;

    switch (decl->type) {
    case AST_CLASS_DECL:
        header.name = decl->data.class_decl.name;
        methods = decl->data.class_decl.methods;
        method_count = decl->data.class_decl.method_count;
        header.uses_pointer_self =
            decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
        break;
    case AST_ENUM_DECL:
        header.name = decl->data.enum_decl.name;
        methods = decl->data.enum_decl.methods;
        method_count = decl->data.enum_decl.method_count;
        break;
    case AST_PARTY_DECL:
        header.name = decl->data.party_decl.name;
        methods = decl->data.party_decl.methods;
        method_count = decl->data.party_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ROSTER_DECL:
        header.name = decl->data.roster_decl.name;
        header.uses_pointer_self = true;
        break;
    case AST_WORLD_DECL:
        header.name = decl->data.world_decl.name;
        methods = decl->data.world_decl.methods;
        method_count = decl->data.world_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_RELATION_DECL:
        header.name = decl->data.relation_decl.name;
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_EFFECT_DECL:
        header.name = decl->data.effect_decl.name;
        methods = decl->data.effect_decl.methods;
        method_count = decl->data.effect_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ZONE_DECL:
        header.name = decl->data.zone_decl.name;
        methods = decl->data.zone_decl.methods;
        method_count = decl->data.zone_decl.method_count;
        header.uses_pointer_self = true;
        break;
    default:
        return true;
    }

    if (header.name == NULL)
        return true;
    if (!mir_decl_header_set_methods(&header, methods, method_count))
        return false;
    if (!mir_append_decl_header(mir, header)) {
        free(header.method_metadata);
        return false;
    }
    return true;
}

static void
mir_link_decl_method_routines(MIRProgram *mir)
{
    if (mir == NULL)
        return;

    for (size_t hi = 0; hi < mir->decl_header_count; hi++) {
        MIRDeclHeader *header = &mir->decl_headers[hi];
        for (size_t mi = 0; mi < header->method_metadata_count; mi++) {
            MIRDeclMethod *method = &header->method_metadata[mi];
            method->has_routine = false;
            method->routine_index = 0;
            if (method->name == NULL)
                continue;

            for (size_t ri = 0; ri < mir->routine_count; ri++) {
                const MIRRoutine *routine = &mir->routines[ri];
                if (routine->kind != MIR_SCOPE_METHOD)
                    continue;
                if (routine->ast == method->ast) {
                    method->has_routine = true;
                    method->routine_index = ri;
                    break;
                }
                if (routine->name == NULL
                    || strcmp(routine->name, method->name) != 0) {
                    continue;
                }
                if (method->owner_name != NULL
                    && routine->owner_name != NULL
                    && strcmp(routine->owner_name, method->owner_name) != 0) {
                    continue;
                }
                method->has_routine = true;
                method->routine_index = ri;
                break;
            }
        }
    }
}

#endif /* PGY_MIR_DECL_HEADERS_H */
