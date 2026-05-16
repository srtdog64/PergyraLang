#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_domain_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    if (ast_print_intent_node(node, indent))
        return true;
    if (ast_print_event_node(node, indent))
        return true;
    if (ast_print_world_node(node, indent))
        return true;
    if (ast_print_zone_node(node, indent))
        return true;

    switch (node->type) {
        case AST_ABILITY_DECL:
            printf("Ability: %s", node->data.ability_decl.name);
            print_generic_params_inline(node->data.ability_decl.generic_params);
            if (node->data.ability_decl.where_clause) {
                printf(" ");
                print_where_clause_inline(node->data.ability_decl.where_clause);
            }
            printf("\n");
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
                ast_print(node->data.ability_decl.require_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
                ast_print(node->data.ability_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_DECL:
            printf("Role: %s", node->data.role_decl.name);
            if (node->data.role_decl.for_type != NULL) {
                printf(" for ");
                ast_print_inline(node->data.role_decl.for_type);
            }
            print_generic_params_inline(node->data.role_decl.generic_params);
            print_where_clause_inline(node->data.role_decl.where_clause);
            printf("\n");
            for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
                ast_print(node->data.role_decl.includes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
                ast_print(node->data.role_decl.impl_abilities[i], indent + 1);
            }
            if (node->data.role_decl.parallel_block != NULL) {
                ast_print_indent(indent + 1);
                printf("Parallel On:\n");
                ast_print(node->data.role_decl.parallel_block, indent + 2);
            }
            break;

        case AST_INCLUDE_STMT:
            printf("Include role %s", node->data.include_stmt.role_name);
            print_generic_params_inline(node->data.include_stmt.type_args);
            printf("\n");
            break;

        case AST_REQUIRE_FIELD:
            printf("Require: %s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.require_field.type);
            }
            printf("\n");
            break;

        case AST_IMPL_ABILITY:
            printf("Impl ability: ");
            ast_print_inline(node->data.impl_ability.ability_ref);
            printf("\n");
            for (size_t i = 0; i < node->data.impl_ability.method_count; i++) {
                ast_print(node->data.impl_ability.methods[i], indent + 1);
            }
            break;

        case AST_OVERRIDE_FUNC:
            printf("Override%s\n",
                   node->data.override_func.calls_super ? " (calls super)" : "");
            ast_print(node->data.override_func.func_decl, indent + 1);
            break;

        case AST_PARTY_DECL:
            printf("Party: %s", node->data.party_decl.name);
            if (node->data.party_decl.extends != NULL) {
                printf(" extends ");
                ast_print_inline(node->data.party_decl.extends);
            }
            print_generic_params_inline(node->data.party_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
                ast_print(node->data.party_decl.role_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
                ast_print(node->data.party_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
                ast_print(node->data.party_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_SLOT:
            printf("RoleSlot: %s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            if (node->data.role_slot.ability_count > 0) {
                printf(" requires ");
                for (size_t i = 0; i < node->data.role_slot.ability_count; i++) {
                    if (i > 0)
                        printf(", ");
                    ast_print_inline(node->data.role_slot.required_abilities[i]);
                }
            }
            printf("\n");
            break;

        case AST_PARTY_SHARED:
            printf("Shared: %s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.party_shared.initializer);
            }
            printf("\n");
            break;

        case AST_CONTEXT_ACCESS:
            printf("ContextAccess: %s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_inline(node->data.context_access.ability_type);
            }
            printf(")\n");
            break;

        case AST_PARTY_INSTANCE:
            printf("PartyInstance: %s\n", node->data.party_instance.party_type);
            for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
                ast_print_indent(indent + 1);
                printf("%s = ",
                       node->data.party_instance.assignments[i].slot_name);
                ast_print_inline(node->data.party_instance.assignments[i].value);
                printf("\n");
            }
            break;

        case AST_ROSTER_DECL:
            printf("Roster: %s", node->data.roster_decl.name);
            print_generic_params_inline(node->data.roster_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
                ast_print(node->data.roster_decl.party_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
                ast_print(node->data.roster_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
                ast_print(node->data.roster_decl.methods[i], indent + 1);
            }
            break;

        case AST_SYSTEMIC_SLOT:
            printf("SystemicSlot: %s: %s", node->data.roster_slot.slot_name,
                   node->data.roster_slot.party_type);
            if (node->data.roster_slot.is_array)
                printf("[]");
            printf("\n");
            break;

        case AST_RELATION_DECL:
            printf("Relation: %s", node->data.relation_decl.name);
            if (node->data.relation_decl.between_left_kind != RELATION_ENDPOINT_NAMED
                || node->data.relation_decl.between_right_kind != RELATION_ENDPOINT_NAMED
                || node->data.relation_decl.between_left_type != NULL
                || node->data.relation_decl.between_right_type != NULL) {
                printf(" between ");
                switch (node->data.relation_decl.between_left_kind) {
                    case RELATION_ENDPOINT_SUBJECT: printf("subject"); break;
                    case RELATION_ENDPOINT_OBJECT: printf("object"); break;
                    case RELATION_ENDPOINT_CLASS: printf("class"); break;
                    case RELATION_ENDPOINT_TOBJECT: printf("tobject"); break;
                    case RELATION_ENDPOINT_NAMED:
                        ast_print_inline(node->data.relation_decl.between_left_type);
                        break;
                }
                printf("%s, ", node->data.relation_decl.between_left_many ? "[]" : "");
                switch (node->data.relation_decl.between_right_kind) {
                    case RELATION_ENDPOINT_SUBJECT: printf("subject"); break;
                    case RELATION_ENDPOINT_OBJECT: printf("object"); break;
                    case RELATION_ENDPOINT_CLASS: printf("class"); break;
                    case RELATION_ENDPOINT_TOBJECT: printf("tobject"); break;
                    case RELATION_ENDPOINT_NAMED:
                        ast_print_inline(node->data.relation_decl.between_right_type);
                        break;
                }
                printf("%s", node->data.relation_decl.between_right_many ? "[]" : "");
            }
            printf("\n");
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
                ast_print(node->data.relation_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
                ast_print(node->data.relation_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
                ast_print(node->data.relation_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
                ast_print(node->data.relation_decl.methods[i], indent + 1);
            }
            break;

        case AST_EFFECT_DECL:
            printf("Effect: %s\n", node->data.effect_decl.name);
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
                ast_print(node->data.effect_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
                ast_print(node->data.effect_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
                ast_print(node->data.effect_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
                ast_print(node->data.effect_decl.methods[i], indent + 1);
            }
            break;

        case AST_DOMAIN_SLOT:
            printf("%sSlot: %s",
                   node->data.domain_slot.is_subject ? "Subject"
                   : (node->data.domain_slot.is_vessel ? "Vessel"
                      : (node->data.domain_slot.is_tobject ? "TObject" : "Object")),
                   node->data.domain_slot.slot_name);
            if (node->data.domain_slot.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.domain_slot.type);
            }
            if (node->data.domain_slot.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.domain_slot.initializer);
            }
            printf("\n");
            break;

        default:
            return false;
    }

    return true;
}
