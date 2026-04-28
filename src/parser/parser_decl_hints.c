#include "parser_internal.h"

static const char *
parser_decl_hint_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_CLASS_DECL:
        return node->data.class_decl.name;
    case AST_INTENT_DECL:
        return node->data.intent_decl.name;
    case AST_ZONE_DECL:
        return node->data.zone_decl.name;
    case AST_WORLD_DECL:
        return node->data.world_decl.name;
    case AST_ROSTER_DECL:
        return node->data.roster_decl.name;
    case AST_PARTY_DECL:
        return node->data.party_decl.name;
    case AST_RELATION_DECL:
        return node->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return node->data.effect_decl.name;
    case AST_EVENT_DECL:
        return node->data.event_decl.name;
    default:
        return NULL;
    }
}

void
parser_register_decl_hint(Parser *parser, ASTNode *node)
{
    const char *name;
    ASTNodeType node_type;
    NominalDeclKind nominal_kind = NOMINAL_DECL_CLASS;

    if (parser == NULL || node == NULL || parser->scope_depth != 0)
        return;

    name = parser_decl_hint_name(node);
    if (name == NULL)
        return;

    for (size_t i = 0; i < parser->decl_hint_count; i++) {
        if (parser->decl_hint_names[i] != NULL
            && strcmp(parser->decl_hint_names[i], name) == 0) {
            parser->decl_hint_types[i] = node->type;
            if (node->type == AST_CLASS_DECL)
                parser->decl_hint_nominal_kinds[i] = node->data.class_decl.nominal_kind;
            return;
        }
    }

    if (parser->decl_hint_count >= parser->decl_hint_capacity) {
        size_t new_capacity = parser->decl_hint_capacity == 0
            ? 16 : parser->decl_hint_capacity * 2;
        char **new_names = realloc(parser->decl_hint_names,
            new_capacity * sizeof(char *));
        ASTNodeType *new_types = realloc(parser->decl_hint_types,
            new_capacity * sizeof(ASTNodeType));
        NominalDeclKind *new_nominal_kinds = realloc(parser->decl_hint_nominal_kinds,
            new_capacity * sizeof(NominalDeclKind));
        if (new_names == NULL || new_types == NULL || new_nominal_kinds == NULL) {
            free(new_names);
            free(new_types);
            free(new_nominal_kinds);
            return;
        }
        parser->decl_hint_names = new_names;
        parser->decl_hint_types = new_types;
        parser->decl_hint_nominal_kinds = new_nominal_kinds;
        parser->decl_hint_capacity = new_capacity;
    }

    node_type = node->type;
    if (node_type == AST_CLASS_DECL)
        nominal_kind = node->data.class_decl.nominal_kind;
    parser->decl_hint_names[parser->decl_hint_count] = pergyra_strdup(name);
    parser->decl_hint_types[parser->decl_hint_count] = node_type;
    parser->decl_hint_nominal_kinds[parser->decl_hint_count] = nominal_kind;
    parser->decl_hint_count++;
}

bool
parser_lookup_decl_hint(Parser *parser, const char *name,
                        ASTNodeType *node_type_out,
                        NominalDeclKind *nominal_kind_out)
{
    if (node_type_out != NULL)
        *node_type_out = AST_PROGRAM;
    if (nominal_kind_out != NULL)
        *nominal_kind_out = NOMINAL_DECL_CLASS;
    if (parser == NULL || name == NULL)
        return false;

    for (size_t i = 0; i < parser->decl_hint_count; i++) {
        if (parser->decl_hint_names[i] != NULL
            && strcmp(parser->decl_hint_names[i], name) == 0) {
            if (node_type_out != NULL)
                *node_type_out = parser->decl_hint_types[i];
            if (nominal_kind_out != NULL)
                *nominal_kind_out = parser->decl_hint_nominal_kinds[i];
            return true;
        }
    }

    return false;
}
