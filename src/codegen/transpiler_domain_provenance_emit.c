/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend domain propagation provenance and projection recompute helpers.
 */

#include "transpiler_domain_provenance_emit.h"

#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "domain_frontier_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"

static const MIRDeclField *
domain_slot_view_field_by_syntax_id(
    const TranspilerHostedDomainSlotView *slot_view,
    uint32_t field_syntax_id)
{
    const MIRDeclField *matched = NULL;

    if (slot_view == NULL || field_syntax_id == 0)
        return NULL;

    for (size_t i = 0; i < slot_view->count; i++) {
        const MIRDeclField *field =
            transpiler_hosted_domain_slot_view_metadata(slot_view, i);
        if (field != NULL
            && mir_decl_field_source_syntax_id(field) == field_syntax_id) {
            if (matched != NULL)
                return NULL;
            matched = field;
        }
    }
    return matched;
}

static const MIRDeclField *
domain_header_field_by_syntax_id(const MIRDeclHeader *header,
                                 uint32_t field_syntax_id)
{
    const MIRDeclField *matched = NULL;

    if (header == NULL || field_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (field != NULL
            && mir_decl_field_source_syntax_id(field) == field_syntax_id) {
            if (matched != NULL)
                return NULL;
            matched = field;
        }
    }
    return matched;
}

static bool
domain_runtime_text_equals(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

static bool
domain_runtime_storage_ready(TranspilerCtx *ctx)
{
    const MIRProgram *mir = ctx != NULL ? ctx->mir : NULL;

    if (mir == NULL || !mir->has_domain_runtime_facts
        || ((mir->domain_participant_role_fact_count == 0)
            != (mir->domain_participant_role_facts == NULL))
        || ((mir->domain_projection_member_assignment_fact_count == 0)
            != (mir->domain_projection_member_assignment_facts == NULL))) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend requires complete MIR domain runtime assignment facts");
        return false;
    }
    return true;
}

const PgyDomainParticipantRoleFact *
transpiler_require_domain_participant_role_fact(
    TranspilerCtx *ctx,
    const char *owner_name,
    PgyDomainParticipantRole role)
{
    const PgyDomainParticipantRoleFact *matched = NULL;
    TranspilerHostedDomainSlotView slot_view;
    const MIRDeclField *field;
    ASTNodeType owner_kind;

    if (ctx == NULL || owner_name == NULL
        || !domain_runtime_storage_ready(ctx)) {
        return NULL;
    }
    owner_kind = role == PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
        ? AST_EFFECT_DECL : AST_RELATION_DECL;
    if (transpiler_active_decl_header_of_type(
            ctx, owner_kind, owner_name) == NULL) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend domain participant role owner '%s' has no exact MIR declaration",
            owner_name);
        return NULL;
    }
    for (size_t i = 0;
         i < ctx->mir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &ctx->mir->domain_participant_role_facts[i];
        if (fact->role != role
            || !domain_runtime_text_equals(fact->owner_name, owner_name)) {
            continue;
        }
        if (matched != NULL) {
            transpiler_set_mir_topology_invalid(ctx,
                "C backend domain participant role for '%s' is duplicated",
                owner_name);
            return NULL;
        }
        matched = fact;
    }
    if (matched == NULL) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend domain participant role for '%s' is missing",
            owner_name);
        return NULL;
    }

    slot_view = transpiler_hosted_domain_slot_view_from_decl(
        ctx, owner_name, NULL);
    if (!slot_view.uses_mir_metadata
        || transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend domain participant role for '%s' has no exact slot inventory",
            owner_name);
        return NULL;
    }
    field = domain_slot_view_field_by_syntax_id(
        &slot_view, matched->field_syntax_id);
    if (matched->program_syntax_id == 0 || matched->owner_syntax_id == 0
        || field == NULL || !mir_decl_field_is_binding_like(field)
        || !domain_runtime_text_equals(
            mir_decl_field_name(field), matched->field_name)
        || !domain_runtime_text_equals(
            mir_decl_field_type_name(field), matched->field_type_name)) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend domain participant role for '%s' failed its exact field identity/type join",
            owner_name);
        return NULL;
    }
    return matched;
}

void
emit_hidden_provenance_fields(TranspilerCtx *ctx,
                              const char *prefix,
                              const char *name)
{
    if (ctx == NULL || ctx->out == NULL || prefix == NULL || name == NULL)
        return;

    codebuf_write(ctx->out, "    uint32_t __%s_epoch_%s;\n", prefix, name);
    codebuf_write(ctx->out, "    int __%s_cause_%s;\n", prefix, name);
}

void
emit_hidden_provenance_stamp(TranspilerCtx *ctx,
                             const char *self_expr,
                             const char *prefix,
                             const char *name,
                             int cause)
{
    if (ctx == NULL || self_expr == NULL || prefix == NULL || name == NULL)
        return;

    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_epoch_%s++;\n", self_expr, prefix, name);
    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_cause_%s = %d;\n",
        self_expr, prefix, name, cause);
}

static bool
domain_projection_fact_matches_owner(
    const PgyDomainProjectionMemberAssignmentFact *fact,
    const char *owner_name)
{
    return fact != NULL
        && domain_runtime_text_equals(fact->owner_name, owner_name);
}

static bool
domain_projection_fact_is_first_directive(const MIRProgram *mir,
                                          size_t index,
                                          const char *owner_name)
{
    const PgyDomainProjectionMemberAssignmentFact *fact;

    if (mir == NULL
        || index >= mir->domain_projection_member_assignment_fact_count) {
        return false;
    }
    fact = &mir->domain_projection_member_assignment_facts[index];
    if (!domain_projection_fact_matches_owner(fact, owner_name))
        return false;
    for (size_t i = 0; i < index; i++) {
        const PgyDomainProjectionMemberAssignmentFact *prior =
            &mir->domain_projection_member_assignment_facts[i];
        if (domain_projection_fact_matches_owner(prior, owner_name)
            && prior->owner_syntax_id == fact->owner_syntax_id
            && prior->directive_syntax_id == fact->directive_syntax_id) {
            return false;
        }
    }
    return true;
}

static bool
domain_projection_source_path_valid(
    TranspilerCtx *ctx,
    const PgyDomainProjectionMemberAssignmentFact *fact,
    const char *source_type_name)
{
    const char *current_type = source_type_name;
    const char *path_cursor;

    if (ctx == NULL || fact == NULL || source_type_name == NULL
        || fact->source_path == NULL || fact->source_path_segments == NULL
        || fact->source_path_segment_count == 0) {
        return false;
    }
    path_cursor = fact->source_path;
    for (size_t i = 0; i < fact->source_path_segment_count; i++) {
        const PgyDomainProjectionPathSegmentFact *segment =
            &fact->source_path_segments[i];
        const MIRDeclHeader *header =
            transpiler_active_decl_header_of_type(
                ctx, AST_CLASS_DECL, current_type);
        const MIRDeclField *field = domain_header_field_by_syntax_id(
            header, segment->field_syntax_id);
        size_t name_length;

        if (field == NULL
            || !domain_runtime_text_equals(
                mir_decl_field_name(field), segment->field_name)
            || !domain_runtime_text_equals(
                mir_decl_field_type_name(field), segment->field_type_name)) {
            return false;
        }
        name_length = strlen(segment->field_name);
        if (strncmp(path_cursor, segment->field_name, name_length) != 0)
            return false;
        path_cursor += name_length;
        if (i + 1 < fact->source_path_segment_count) {
            if (*path_cursor != '.')
                return false;
            path_cursor++;
        } else if (*path_cursor != '\0') {
            return false;
        }
        current_type = segment->field_type_name;
    }
    return domain_runtime_text_equals(
        current_type, fact->source_leaf_type_name);
}

static bool
domain_projection_assignment_valid(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const char *owner_name,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    const MIRDeclField *projection_slot;
    const MIRDeclField *source_slot;
    const MIRDeclHeader *target_header;
    const MIRDeclField *target_field;
    const char *target_type_name;
    const char *source_type_name;

    if (ctx == NULL || slot_view == NULL || fact == NULL
        || !domain_projection_fact_matches_owner(fact, owner_name)
        || fact->program_syntax_id == 0 || fact->owner_syntax_id == 0
        || fact->directive_syntax_id == 0
        || fact->projection_slot_syntax_id == 0
        || fact->source_slot_syntax_id == 0
        || fact->target_decl_syntax_id == 0
        || fact->target_field_syntax_id == 0
        || fact->source_decl_syntax_id == 0
        || (unsigned)fact->operation
            > (unsigned)PGY_DOMAIN_PROJECTION_BIND) {
        return false;
    }
    projection_slot = domain_slot_view_field_by_syntax_id(
        slot_view, fact->projection_slot_syntax_id);
    source_slot = domain_slot_view_field_by_syntax_id(
        slot_view, fact->source_slot_syntax_id);
    if (projection_slot == NULL || source_slot == NULL
        || !domain_runtime_text_equals(
            mir_decl_field_name(projection_slot),
            fact->projection_slot_name)
        || !domain_runtime_text_equals(
            mir_decl_field_name(source_slot), fact->source_slot_name)) {
        return false;
    }
    target_type_name = mir_decl_field_type_name(projection_slot);
    source_type_name = mir_decl_field_type_name(source_slot);
    target_header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, target_type_name);
    target_field = domain_header_field_by_syntax_id(
        target_header, fact->target_field_syntax_id);
    if (target_field == NULL
        || !domain_runtime_text_equals(
            mir_decl_field_name(target_field), fact->target_field_name)
        || !domain_runtime_text_equals(
            mir_decl_field_type_name(target_field),
            fact->target_field_type_name)
        || !domain_projection_source_path_valid(
            ctx, fact, source_type_name)) {
        return false;
    }
    return true;
}

static bool
domain_projection_directive_group_valid(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const char *owner_name,
    const PgyDomainProjectionMemberAssignmentFact *head)
{
    const MIRDeclField *projection_slot;
    const MIRDeclHeader *target_header;
    const char *target_type_name;
    size_t group_count = 0;

    if (ctx == NULL || slot_view == NULL || head == NULL)
        return false;
    projection_slot = domain_slot_view_field_by_syntax_id(
        slot_view, head->projection_slot_syntax_id);
    if (projection_slot == NULL)
        return false;
    target_type_name = mir_decl_field_type_name(projection_slot);
    target_header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, target_type_name);
    if (target_header == NULL)
        return false;

    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (!domain_projection_fact_matches_owner(fact, owner_name)
            || fact->owner_syntax_id != head->owner_syntax_id
            || fact->directive_syntax_id != head->directive_syntax_id) {
            continue;
        }
        if (fact->projection_slot_syntax_id
                != head->projection_slot_syntax_id
            || fact->source_slot_syntax_id != head->source_slot_syntax_id
            || fact->target_decl_syntax_id != head->target_decl_syntax_id
            || fact->source_decl_syntax_id != head->source_decl_syntax_id
            || fact->operation != head->operation
            || !domain_projection_assignment_valid(
                ctx, slot_view, owner_name, fact)) {
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &ctx->mir->domain_projection_member_assignment_facts[j];
            if (prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                return false;
            }
        }
        group_count++;
    }
    if (group_count != mir_decl_header_field_count(target_header))
        return false;
    for (size_t i = 0; i < mir_decl_header_field_count(target_header); i++) {
        const MIRDeclField *target_field =
            mir_decl_header_field(target_header, i);
        size_t matches = 0;
        for (size_t j = 0;
             j < ctx->mir->domain_projection_member_assignment_fact_count;
             j++) {
            const PgyDomainProjectionMemberAssignmentFact *fact =
                &ctx->mir->domain_projection_member_assignment_facts[j];
            if (domain_projection_fact_matches_owner(fact, owner_name)
                && fact->owner_syntax_id == head->owner_syntax_id
                && fact->directive_syntax_id == head->directive_syntax_id
                && target_field != NULL
                && fact->target_field_syntax_id
                    == mir_decl_field_source_syntax_id(target_field)) {
                matches++;
            }
        }
        if (matches != 1)
            return false;
    }
    return true;
}

static bool
domain_projection_owner_facts_valid(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const char *owner_name,
    size_t expected_directive_count,
    size_t *directive_count_out)
{
    uint32_t owner_syntax_id = 0;
    size_t directive_count = 0;
    size_t owner_fact_count = 0;

    if (!domain_runtime_storage_ready(ctx))
        return false;
    if (slot_view == NULL || !slot_view->uses_mir_metadata
        || owner_name == NULL) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend projection runtime facts have no exact owner slot inventory");
        return false;
    }
    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (!domain_projection_fact_matches_owner(fact, owner_name))
            continue;
        owner_fact_count++;
        if (owner_syntax_id == 0)
            owner_syntax_id = fact->owner_syntax_id;
        if (owner_syntax_id != fact->owner_syntax_id
            || !domain_projection_assignment_valid(
                ctx, slot_view, owner_name, fact)) {
            goto invalid;
        }
        if (domain_projection_fact_is_first_directive(
                ctx->mir, i, owner_name)) {
            directive_count++;
            if (!domain_projection_directive_group_valid(
                    ctx, slot_view, owner_name, fact)) {
                goto invalid;
            }
        }
    }
    if (owner_fact_count == 0
        || directive_count != expected_directive_count) {
        goto invalid;
    }
    if (directive_count_out != NULL)
        *directive_count_out = directive_count;
    return true;

invalid:
    transpiler_set_mir_topology_invalid(ctx,
        "C backend projection owner '%s' has incomplete or drifting exact runtime assignment facts",
        owner_name);
    return false;
}

static CodeBuf *
domain_projection_literal_from_facts(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const char *owner_name,
    const PgyDomainProjectionMemberAssignmentFact *head)
{
    const MIRDeclField *projection_slot;
    const char *target_type_name;
    CodeBuf *literal;
    bool first = true;

    if (ctx == NULL || slot_view == NULL || head == NULL)
        return NULL;
    projection_slot = domain_slot_view_field_by_syntax_id(
        slot_view, head->projection_slot_syntax_id);
    target_type_name = projection_slot != NULL
        ? mir_decl_field_type_name(projection_slot) : NULL;
    literal = target_type_name != NULL ? codebuf_create() : NULL;
    if (literal == NULL) {
        transpiler_set_mir_topology_invalid(ctx,
            "C backend could not materialize exact projection literal for '%s'",
            owner_name);
        return NULL;
    }
    codebuf_write(literal, "(%s){ ", target_type_name);
    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (!domain_projection_fact_matches_owner(fact, owner_name)
            || fact->owner_syntax_id != head->owner_syntax_id
            || fact->directive_syntax_id != head->directive_syntax_id) {
            continue;
        }
        if (!first)
            codebuf_write(literal, ", ");
        first = false;
        codebuf_write(literal, ".%s = self->%s",
            fact->target_field_name, fact->source_slot_name);
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            codebuf_write(literal, ".%s",
                fact->source_path_segments[s].field_name);
        }
    }
    codebuf_write(literal, " }");
    return literal;
}

void
emit_domain_projection_sync_loop_from_mir_runtime_facts(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const char *owner_name,
    size_t expected_directive_count,
    const char *loop_prefix,
    bool early_return_if_clean)
{
    size_t directive_count = 0;
    bool emitted_condition = false;

    if (ctx == NULL || ctx->out == NULL || slot_view == NULL
        || owner_name == NULL || loop_prefix == NULL
        || expected_directive_count == 0) {
        return;
    }
    if (!domain_projection_owner_facts_valid(ctx, slot_view, owner_name,
            expected_directive_count, &directive_count)) {
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "if (");
    if (early_return_if_clean)
        codebuf_write(ctx->out, "!(");
    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (!domain_projection_fact_is_first_directive(
                ctx->mir, i, owner_name)) {
            continue;
        }
        if (emitted_condition)
            codebuf_write(ctx->out, " || ");
        codebuf_write(ctx->out, "self->__projection_dirty_%s",
            fact->projection_slot_name);
        emitted_condition = true;
    }
    if (early_return_if_clean)
        codebuf_write(ctx->out, ")) return;\n");
    else
        codebuf_write(ctx->out, ") {\n");
    if (!early_return_if_clean)
        ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass = 0;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass_limit = %zu;\n",
        loop_prefix,
        pgy_domain_projection_frontier_pass_limit(directive_count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_%s_continue = true;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit) {\n",
        loop_prefix, loop_prefix, loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_continue = false;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_pass++;\n", loop_prefix);

    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        CodeBuf *literal;

        if (!domain_projection_fact_is_first_directive(
                ctx->mir, i, owner_name)) {
            continue;
        }
        literal = domain_projection_literal_from_facts(
            ctx, slot_view, owner_name, fact);
        if (literal == NULL)
            return;

        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__projection_dirty_%s) {\n",
            fact->projection_slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
            fact->projection_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            fact->projection_slot_name, literal->data);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            fact->projection_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_dirty_%s = false;\n",
            fact->projection_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "projection",
            fact->projection_slot_name, PGY_PROP_CAUSE_REFRESH);
        codebuf_destroy(literal);

        for (size_t dep_i = 0;
             dep_i < ctx->mir->domain_projection_member_assignment_fact_count;
             dep_i++) {
            const PgyDomainProjectionMemberAssignmentFact *dependent =
                &ctx->mir->domain_projection_member_assignment_facts[dep_i];
            if (!domain_projection_fact_is_first_directive(
                    ctx->mir, dep_i, owner_name)
                || dependent->source_slot_syntax_id
                    != fact->projection_slot_syntax_id) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out,
                "self->__projection_dirty_%s = true;\n",
                dependent->projection_slot_name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "self->__projection_ready_%s = false;\n",
                dependent->projection_slot_name);
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_%s_continue = true;\n",
                loop_prefix);
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_%s_continue) {\n", loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PGY_PANIC(\"%s\");\n", PGY_FRONTIER_REASON_PROJECTION_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (!early_return_if_clean) {
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
