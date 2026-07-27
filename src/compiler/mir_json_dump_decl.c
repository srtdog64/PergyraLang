/* Declaration-metadata JSON dump owner (schema pgy.mir.v1).
 * Split from mir_json_dump.c by responsibility: nominal/ability/enum/role
 * declaration headers, their fields, generic params, methods, and variants.
 * Instruction/routine/program serialization stays in mir_json_dump.c. */

#include "mir.h"
#include "mir_decl_headers.h"
#include "mir_json_dump_decl.h"
#include "mir_json_dump_internal.h"

#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/callable_contract_vocabulary.h"

static const char *
mir_json_nominal_kind_name(NominalDeclKind kind)
{
    switch (kind) {
    case NOMINAL_DECL_SUBJECT: return "subject";
    case NOMINAL_DECL_VESSEL:  return "vessel";
    case NOMINAL_DECL_STRUCT:  return "struct";
    case NOMINAL_DECL_OBJECT:  return "object";
    case NOMINAL_DECL_TOBJECT: return "tobject";
    case NOMINAL_DECL_CLASS:
    default:                   return "class";
    }
}

static void
mir_json_emit_ability_ref(FILE *out, const MIRAbilityRef *ref);

static void
mir_json_emit_decl_fields(FILE *out, const MIRDeclHeader *header)
{
    size_t emitted = 0;
    fputs(",\"fields\":[", out);
    for (size_t f = 0; f < mir_decl_header_field_count(header); f++) {
        const MIRDeclField *field = mir_decl_header_field(header, f);
        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN)
            == MIR_DECL_FIELD_ROLE_SLOT)
            continue;
        if (emitted > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_field_name(field));
        fputs(",\"type\":", out);
        mir_json_emit_str_or_null(out, mir_decl_field_type_name(field));
        fputc('}', out);
        emitted++;
    }
    fputc(']', out);
}

static void
mir_json_emit_decl_role_slots(FILE *out, const MIRDeclHeader *header)
{
    size_t emitted = 0;

    fputs(",\"role_slots\":[", out);
    for (size_t f = 0; f < mir_decl_header_field_count(header); f++) {
        const MIRDeclField *field = mir_decl_header_field(header, f);
        size_t ability_count;

        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN)
            != MIR_DECL_FIELD_ROLE_SLOT)
            continue;
        if (emitted > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_field_name(field));
        fprintf(out, ",\"dynamic\":%s,\"abilities\":[",
                mir_decl_field_is_dynamic(field) ? "true" : "false");
        ability_count = mir_decl_field_required_ability_count(field);
        for (size_t a = 0; a < ability_count; a++) {
            if (a > 0)
                fputc(',', out);
            mir_json_emit_ability_ref(
                out, mir_decl_field_required_ability_ref(field, a));
        }
        fputs("]}", out);
        emitted++;
    }
    fputc(']', out);
}

static void
mir_json_emit_decl_generic_params(FILE *out, const MIRDeclHeader *header)
{
    size_t count = mir_decl_header_generic_param_count(header);

    if (count == 0)
        return;
    fputs(",\"generic_params\":[", out);
    for (size_t i = 0; i < count; i++) {
        const MIRDeclGenericParam *param =
            mir_decl_header_generic_param(header, i);
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_generic_param_name(param));
        fputs(",\"constraint\":", out);
        mir_json_emit_str_or_null(
            out, mir_decl_generic_param_constraint_type_name(param));
        fputs(",\"default_type\":", out);
        mir_json_emit_str_or_null(
            out, mir_decl_generic_param_default_type_name(param));
        fputc('}', out);
    }
    fputc(']', out);
}

static void
mir_json_emit_decl_method_params(FILE *out, const MIRDeclMethod *method)
{
    fputs(",\"params\":[", out);
    for (size_t p = 0; p < mir_decl_method_param_count(method); p++) {
        FuncParam *param = mir_decl_method_param(method, p);
        if (p > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, param != NULL ? param->name : NULL);
        fputs(",\"type\":", out);
        mir_json_emit_str_or_null(out,
            mir_decl_method_param_type_name(method, p));
        fputc('}', out);
    }
    fputc(']', out);
}

static void
mir_json_emit_mask_names(FILE *out, uint32_t mask,
                         PgyCallableContractAxis axis,
                         bool local_when_zero)
{
    bool first = true;
    size_t count = pgy_callable_contract_vocabulary_axis_count(axis);
    fputc('[', out);
    if (mask == 0 && local_when_zero) {
        const PgyCallableContractWordSpec *zero =
            pgy_callable_contract_vocabulary_at_rank(axis, count - 1);
        mir_json_emit_str_or_null(out, zero->spelling);
    } else {
        for (size_t i = 0; i < count; i++) {
            const PgyCallableContractWordSpec *spec =
                pgy_callable_contract_vocabulary_at_rank(axis, i);
            if (spec == NULL || spec->mask == 0 ||
                (mask & spec->mask) == 0)
                continue;
            if (!first)
                fputc(',', out);
            mir_json_emit_str_or_null(out, spec->spelling);
            first = false;
        }
    }
    fputc(']', out);
}

static void
mir_json_emit_decl_method_contract(FILE *out, const MIRDeclMethod *method)
{
    fputs(",\"callable_kind\":", out);
    mir_json_emit_str_or_null(
        out, mir_decl_method_is_action_like(method) ? "action" : "function");
    fputs(",\"contract\":{\"requires\":[", out);
    for (size_t i = 0; i <
            mir_decl_method_required_ability_count(method); i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_ability_ref(
            out, mir_decl_method_required_ability_ref(method, i));
    }
    fputs("],\"within\":", out);
    mir_json_emit_str_or_null(out, mir_decl_method_within_zone(method));
    fputs(",\"causes\":", out);
    mir_json_emit_str_or_null(out, mir_decl_method_causes_effect(method));
    fputs(",\"authorized_by\":[", out);
    for (size_t i = 0; i < mir_decl_method_authorized_by_count(method); i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(out, mir_decl_method_authorized_by(method, i));
    }
    fputs("],\"caps_present\":", out);
    fputs(mir_decl_method_has_caps_clause(method) ? "true" : "false", out);
    fputs(",\"caps\":", out);
    mir_json_emit_mask_names(
        out, mir_decl_method_declared_capabilities(method),
        PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, false);
    fputs(",\"effects_present\":", out);
    fputs(mir_decl_method_has_effects_clause(method) ? "true" : "false", out);
    fputs(",\"effects\":", out);
    mir_json_emit_mask_names(
        out, mir_decl_method_declared_effects(method),
        PGY_CALLABLE_CONTRACT_AXIS_EFFECT,
        mir_decl_method_has_effects_clause(method));
    fputc('}', out);
}

static void
mir_json_emit_decl_methods(FILE *out,
                           const MIRDeclHeader *header,
                           bool include_params)
{
    fputs(",\"methods\":[", out);
    for (size_t m = 0; m < mir_decl_header_method_count(header); m++) {
        const MIRDeclMethod *method = mir_decl_header_method(header, m);
        if (m > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_method_name(method));
        fputs(",\"return\":", out);
        mir_json_emit_str_or_null(out,
            mir_decl_method_return_type_name(method));
        mir_json_emit_decl_method_contract(out, method);
        if (include_params)
            mir_json_emit_decl_method_params(out, method);
        fputc('}', out);
    }
    fputc(']', out);
}

static void
mir_json_emit_ability_ref(FILE *out, const MIRAbilityRef *ref)
{
    fputs("{\"base\":", out);
    mir_json_emit_str_or_null(out, mir_ability_ref_base_name(ref));
    fputs(",\"actuals\":[", out);
    for (size_t i = 0; i < mir_ability_ref_actual_arg_count(ref); i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(
            out, mir_ability_ref_actual_arg_type_name(ref, i));
    }
    fputs("]}", out);
}

static void
mir_json_emit_role_includes(FILE *out, const MIRDeclHeader *header)
{
    fputs(",\"includes\":[", out);
    for (size_t i = 0; i < mir_decl_header_role_include_count(header); i++) {
        const MIRDeclRoleInclude *include =
            mir_decl_header_role_include(header, i);
        if (i > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(out, mir_decl_role_include_name(include));
    }
    fputc(']', out);
}

static void
mir_json_emit_role_impls(FILE *out, const MIRDeclHeader *header)
{
    fputs(",\"impls\":[", out);
    for (size_t i = 0; i < mir_decl_header_role_impl_count(header); i++) {
        const MIRDeclRoleImpl *impl = mir_decl_header_role_impl(header, i);
        if (i > 0)
            fputc(',', out);
        fputs("{\"ability\":", out);
        mir_json_emit_ability_ref(out, mir_decl_role_impl_ability_ref(impl));
        fprintf(out, ",\"method_start\":%zu,\"method_count\":%zu}",
                mir_decl_role_impl_method_start_index(impl),
                mir_decl_role_impl_method_count(impl));
    }
    fputc(']', out);
}

static void
mir_json_emit_enum_variants(FILE *out, const MIRDeclHeader *header)
{
    fputs(",\"variants\":[", out);
    for (size_t v = 0; v < mir_decl_header_enum_variant_count(header); v++) {
        const MIRDeclEnumVariant *variant =
            mir_decl_header_enum_variant(header, v);
        if (v > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_enum_variant_name(variant));
        const size_t param_count =
            mir_decl_enum_variant_param_count(variant);
        fprintf(out, ",\"param_count\":%zu,\"param_types\":[",
                param_count);
        for (size_t p = 0; p < param_count; p++) {
            if (p > 0)
                fputc(',', out);
            mir_json_emit_str_or_null(
                out, mir_decl_enum_variant_param_type_name(variant, p)
            );
        }
        fputc(']', out);
        fputc('}', out);
    }
    fputc(']', out);
}

static bool
mir_json_decl_is_supported_nominal(ASTNodeType ast_type,
                                   NominalDeclKind nominal_kind)
{
    return ast_type == AST_ZONE_DECL
        || ast_type == AST_PARTY_DECL
        || (ast_type == AST_CLASS_DECL
        && (nominal_kind == NOMINAL_DECL_STRUCT
            || nominal_kind == NOMINAL_DECL_CLASS
            || nominal_kind == NOMINAL_DECL_SUBJECT
            || nominal_kind == NOMINAL_DECL_VESSEL
            || nominal_kind == NOMINAL_DECL_OBJECT
            || nominal_kind == NOMINAL_DECL_TOBJECT));
}

static bool
mir_json_decl_is_unsupported_fact(ASTNodeType ast_type,
                                  NominalDeclKind nominal_kind)
{
    if (ast_type == AST_EVENT_DECL)
        return true;
    return ast_type == AST_CLASS_DECL
        && !mir_json_decl_is_supported_nominal(ast_type, nominal_kind);
}

static void
mir_json_emit_decl(FILE *out, const MIRDeclHeader *header)
{
    ASTNodeType ast_type = mir_decl_header_ast_type_or(header, AST_PROGRAM);
    NominalDeclKind nominal_kind =
        mir_decl_header_nominal_kind_or(header, NOMINAL_DECL_CLASS);

    if (mir_json_decl_is_supported_nominal(ast_type, nominal_kind)) {
        bool is_struct_decl = ast_type == AST_CLASS_DECL
            && nominal_kind == NOMINAL_DECL_STRUCT;
        const char *json_nominal_kind = ast_type == AST_ZONE_DECL
            ? "zone" : mir_json_nominal_kind_name(nominal_kind);
        if (ast_type == AST_PARTY_DECL)
            json_nominal_kind = "party";
        fputs("{\"kind\":", out);
        mir_json_emit_str(out, is_struct_decl ? "struct" : "class");
        fputs(",\"nominal_kind\":", out);
        mir_json_emit_str(out, json_nominal_kind);
        fputs(",\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_header_name(header));
        mir_json_emit_decl_generic_params(out, header);
        mir_json_emit_decl_fields(out, header);
        if (ast_type == AST_PARTY_DECL)
            mir_json_emit_decl_role_slots(out, header);
        if (!is_struct_decl)
            mir_json_emit_decl_methods(out, header, false);
        fputc('}', out);
        return;
    }

    if (ast_type == AST_ABILITY_DECL) {
        fputs("{\"kind\":\"ability\",\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_header_name(header));
        mir_json_emit_decl_generic_params(out, header);
        mir_json_emit_decl_methods(out, header, true);
        fputc('}', out);
        return;
    }

    if (ast_type == AST_ENUM_DECL) {
        fputs("{\"kind\":\"enum\",\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_header_name(header));
        mir_json_emit_enum_variants(out, header);
        fputc('}', out);
        return;
    }

    if (ast_type == AST_ROLE_DECL) {
        fputs("{\"kind\":\"role\",\"name\":", out);
        mir_json_emit_str_or_null(out, mir_decl_header_name(header));
        fputs(",\"for_type\":", out);
        mir_json_emit_str_or_null(
            out, mir_decl_header_role_subject_type_name(header));
        mir_json_emit_decl_generic_params(out, header);
        mir_json_emit_role_includes(out, header);
        mir_json_emit_role_impls(out, header);
        mir_json_emit_decl_methods(out, header, true);
        fputc('}', out);
        return;
    }

    fputs("{\"kind\":\"unsupported\",\"ast_type\":", out);
    mir_json_emit_str(out, mir_source_node_type_name(ast_type));
    if (ast_type == AST_CLASS_DECL) {
        fputs(",\"nominal_kind\":", out);
        mir_json_emit_str(out, mir_json_nominal_kind_name(nominal_kind));
    }
    fputs(",\"name\":", out);
    mir_json_emit_str_or_null(out, mir_decl_header_name(header));
    fputc('}', out);
}

void
mir_json_emit_decls(FILE *out, const MIRProgram *mir)
{
    bool first_decl = true;

    if (mir == NULL || mir->decl_headers == NULL)
        return;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *header = &mir->decl_headers[i];
        ASTNodeType ast_type = mir_decl_header_ast_type_or(header, AST_PROGRAM);
        NominalDeclKind nominal_kind =
            mir_decl_header_nominal_kind_or(header, NOMINAL_DECL_CLASS);

        if (ast_type == AST_FUNC_DECL || ast_type == AST_TYPE_ALIAS)
            continue;
        if (ast_type != AST_ENUM_DECL && ast_type != AST_ABILITY_DECL
            && ast_type != AST_ROLE_DECL
            && !mir_json_decl_is_supported_nominal(ast_type, nominal_kind)
            && !mir_json_decl_is_unsupported_fact(ast_type, nominal_kind))
            continue;
        if (!first_decl)
            fputc(',', out);
        first_decl = false;
        mir_json_emit_decl(out, header);
    }
}
