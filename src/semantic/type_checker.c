/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"

#define INITIAL_DIAG_CAPACITY 16

/* -----------------------------------------------------------------
 * Context lifecycle
 * ----------------------------------------------------------------- */

SemanticContext *
semantic_context_create(void)
{
    SemanticContext *ctx = calloc(1, sizeof(SemanticContext));
    if (ctx == NULL)
        return NULL;

    ctx->scope               = scope_create(NULL, SCOPE_GLOBAL);
    ctx->diagnostic_capacity = INITIAL_DIAG_CAPACITY;
    ctx->diagnostics         = calloc(INITIAL_DIAG_CAPACITY,
                                      sizeof(Diagnostic *));
    if (ctx->scope == NULL || ctx->diagnostics == NULL) {
        scope_destroy(ctx->scope);
        free(ctx->diagnostics);
        free(ctx);
        return NULL;
    }

    /* Register built-in types in global scope */
    type_system_init();

    return ctx;
}

void
semantic_context_destroy(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;

    scope_destroy(ctx->scope);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        free(ctx->diagnostics[i]->message);
        free(ctx->diagnostics[i]);
    }
    free(ctx->diagnostics);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Diagnostic emission
 * ----------------------------------------------------------------- */

static void
emit_diagnostic(SemanticContext *ctx, DiagnosticLevel level,
                 const ASTNode *node, const char *fmt, va_list ap)
{
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *existing = ctx->diagnostics[i];
        if (existing == NULL)
            continue;
        if (existing->level != level)
            continue;
        if (existing->line != (node ? node->line : 0)
            || existing->col != (node ? node->column : 0)) {
            continue;
        }
        if (existing->message != NULL && strcmp(existing->message, buf) == 0)
            return;
    }

    if (ctx->diagnostic_count >= ctx->diagnostic_capacity) {
        size_t new_cap = ctx->diagnostic_capacity * 2;
        Diagnostic **grown = realloc(ctx->diagnostics,
                                     new_cap * sizeof(Diagnostic *));
        if (grown == NULL)
            return;
        ctx->diagnostics         = grown;
        ctx->diagnostic_capacity = new_cap;
    }

    Diagnostic *d = calloc(1, sizeof(Diagnostic));
    if (d == NULL)
        return;

    d->level = level;
    d->line  = node ? node->line   : 0;
    d->col   = node ? node->column : 0;
    d->message = pergyra_strdup(buf);

    ctx->diagnostics[ctx->diagnostic_count++] = d;

    if (level == DIAG_ERROR)
        ctx->has_error = true;
}

void
semantic_error(SemanticContext *ctx, const ASTNode *node,
               const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning(SemanticContext *ctx, const ASTNode *node,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, node, fmt, ap);
    va_end(ap);
}

void
semantic_print_diagnostics(SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *d = ctx->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        fprintf(stderr, "[%s] %u:%u - %s\n",
                level, d->line, d->col, d->message);
    }
}

/* -----------------------------------------------------------------
 * Utility — resolve AST type node to Type*
 * ----------------------------------------------------------------- */

static Type *
resolve_named_type(const char *name, SemanticContext *ctx, const ASTNode *site)
{
    if (strcmp(name, "Int")    == 0) return TYPE_INT;
    if (strcmp(name, "Long")   == 0) return TYPE_LONG;
    if (strcmp(name, "Float")  == 0) return TYPE_FLOAT;
    if (strcmp(name, "Double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "Bool")   == 0) return TYPE_BOOL;
    if (strcmp(name, "String") == 0) return TYPE_STRING;
    if (strcmp(name, "QubitSlot") == 0) return TYPE_QUBIT;
    if (strcmp(name, "Void")   == 0) return TYPE_VOID;
    if (strcmp(name, "Array")  == 0) return TYPE_ARRAY;
    if (strcmp(name, "Slice")  == 0) return TYPE_SLICE;
    if (strcmp(name, "Box")    == 0) return TYPE_BOX;
    if (strcmp(name, "Rc")     == 0) return TYPE_RC;
    if (strcmp(name, "Weak")   == 0) return TYPE_WEAK;
    if (strcmp(name, "RemoteFuture") == 0) return TYPE_REMOTE_FUTURE;
    if (strcmp(name, "DeviceSlot") == 0) return TYPE_DEVICE_SLOT;
    if (strcmp(name, "Allocator") == 0) return TYPE_ALLOCATOR;
    if (strcmp(name, "Option") == 0) return TYPE_OPTION;

    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL)
        return sym->type;

    semantic_error(ctx, site, "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

static Type *
resolve_generic_type_arg(GenericParam *gp, SemanticContext *ctx,
                         const ASTNode *site)
{
    if (gp == NULL)
        return TYPE_UNKNOWN;
    if (gp->constraint != NULL)
        return resolve_type_node(gp->constraint, ctx);
    return resolve_named_type(gp->name, ctx, site);
}

static bool
name_looks_qualified(const char *name)
{
    return name != NULL && strchr(name, '.') != NULL;
}

static uint32_t
parse_effect_word(const char *word)
{
    if (word == NULL || *word == '\0')
        return EFFECT_NONE;
    if (strcmp(word, "secure") == 0)
        return EFFECT_SECURE;
    if (strcmp(word, "remote") == 0)
        return EFFECT_REMOTE;
    if (strcmp(word, "nondeterministic") == 0)
        return EFFECT_NONDETERMINISTIC;
    if (strcmp(word, "collapse") == 0)
        return EFFECT_COLLAPSE;
    if (strcmp(word, "local") == 0)
        return EFFECT_NONE;
    return UINT32_MAX;
}

static void
append_effect_name(char *buf, size_t buf_size, const char *name, bool *first)
{
    if (buf == NULL || buf_size == 0 || name == NULL || first == NULL)
        return;

    if (!*first)
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    strncat(buf, name, buf_size - strlen(buf) - 1);
    *first = false;
}

static void
effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size)
{
    bool first = true;

    if (buf == NULL || buf_size == 0)
        return;

    buf[0] = '\0';
    if (mask == EFFECT_NONE) {
        strncat(buf, "local", buf_size - 1);
        return;
    }

    if (type_effect_mask_has(mask, EFFECT_SECURE))
        append_effect_name(buf, buf_size, "secure", &first);
    if (type_effect_mask_has(mask, EFFECT_REMOTE))
        append_effect_name(buf, buf_size, "remote", &first);
    if (type_effect_mask_has(mask, EFFECT_NONDETERMINISTIC))
        append_effect_name(buf, buf_size, "nondeterministic", &first);
    if (type_effect_mask_has(mask, EFFECT_COLLAPSE))
        append_effect_name(buf, buf_size, "collapse", &first);
}

static uint32_t
effects_from_structured_comment(StructuredComment *comment,
                                SemanticContext *ctx,
                                const ASTNode *site)
{
    uint32_t mask = EFFECT_NONE;

    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag == NULL || tag->type != DOC_TAG_EFFECTS || tag->content == NULL)
                continue;

            char *copy = pergyra_strdup(tag->content);
            char *tok = copy != NULL ? strtok(copy, ", \t\r\n|") : NULL;
            while (tok != NULL) {
                for (char *p = tok; *p != '\0'; p++)
                    *p = (char)tolower((unsigned char)*p);

                uint32_t effect = parse_effect_word(tok);
                if (effect == UINT32_MAX) {
                    semantic_warning(ctx, site,
                        "Unknown effect tag '%s' in structured comment; expected secure, remote, nondeterministic, collapse, or local",
                        tok);
                } else {
                    mask |= effect;
                }
                tok = strtok(NULL, ", \t\r\n|");
            }
            free(copy);
        }
    }

    return mask;
}

static bool
structured_comment_has_effects_tag(StructuredComment *comment)
{
    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag != NULL && tag->type == DOC_TAG_EFFECTS)
                return true;
        }
    }
    return false;
}

static uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out)
{
    bool has_doc_contract;
    bool has_sig_contract;
    uint32_t doc_effects;
    uint32_t sig_effects;

    if (node == NULL)
        return EFFECT_NONE;

    has_doc_contract = structured_comment_has_effects_tag(node->data.func_decl.doc_comment);
    has_sig_contract = node->data.func_decl.has_effects_clause;
    doc_effects = effects_from_structured_comment(node->data.func_decl.doc_comment, ctx, node);
    sig_effects = node->data.func_decl.declared_effects;

    if (has_contract_out != NULL)
        *has_contract_out = has_doc_contract || has_sig_contract;

    return doc_effects | sig_effects;
}

void
semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return;
    ctx->current_function_effects |= effect_mask;
}

bool
type_is_constructed_named(const Type *type, const char *name)
{
    return type != NULL
        && type->kind == TYPE_KIND_CONSTRUCTED
        && type->data.constructed.constructor != NULL
        && strcmp(type->data.constructed.constructor->name, name) == 0;
}

bool
type_is_qubit(const Type *type)
{
    if (type == NULL)
        return false;
    if (TYPE_QUBIT != NULL && type_equals(type, TYPE_QUBIT))
        return true;
    return type->name != NULL && strcmp(type->name, "QubitSlot") == 0;
}

bool
type_is_slot_handle(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_SLOT;
}

bool
type_is_owned_slot_handle(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_OWNED;
}

bool
type_is_read_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_READ_VIEW;
}

bool
type_is_write_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_WRITE_VIEW;
}

bool
type_is_move_token(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_MOVE_TOKEN;
}

bool
type_is_resource_handle(const Type *type)
{
    return type_is_qubit(type)
        || type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_anchored_resource_handle(const Type *type)
{
    return type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_movable_resource_handle(const Type *type)
{
    return type_is_qubit(type);
}

static const char *
resource_handle_display_name(const Type *type)
{
    if (type == NULL)
        return "resource";
    if (type_is_qubit(type))
        return "QubitSlot";
    if (type_is_read_view(type))
        return "ReadView";
    if (type_is_write_view(type))
        return "WriteView";
    if (type_is_move_token(type))
        return "MoveToken";
    if (type_is_owned_slot_handle(type))
        return type->data.slot.is_secure ? "SecureSlot" : "Slot";
    if (type_is_constructed_named(type, "DeviceSlot"))
        return "DeviceSlot";
    return type->name != NULL ? type->name : "resource";
}

static ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_actor_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ACTOR_DECL)
            continue;
        if (stmt->data.actor_decl.name != NULL
            && strcmp(stmt->data.actor_decl.name, type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static TypeNominalFlavor
nominal_flavor_from_decl(const ASTNode *decl)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return TYPE_NOMINAL_NONE;

    switch (decl->data.class_decl.nominal_kind) {
    case NOMINAL_DECL_SUBJECT:
        return TYPE_NOMINAL_SUBJECT;
    case NOMINAL_DECL_STRUCT:
        return TYPE_NOMINAL_STRUCT;
    case NOMINAL_DECL_OBJECT:
        return TYPE_NOMINAL_OBJECT;
    case NOMINAL_DECL_DTO:
        return TYPE_NOMINAL_DTO;
    case NOMINAL_DECL_CLASS:
    default:
        return TYPE_NOMINAL_CLASS;
    }
}

static bool
decl_is_subject_type(const ASTNode *decl)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
}

static bool
decl_is_subject_host(const ASTNode *decl)
{
    if (decl == NULL)
        return false;
    if (decl->type == AST_ACTOR_DECL)
        return true;
    return decl_is_subject_type(decl);
}

static ASTNode *
find_subject_host_decl_by_name(ASTNode *program, const char *type_name)
{
    ASTNode *decl = find_type_decl_by_name(program, type_name);
    if (decl_is_subject_host(decl))
        return decl;
    decl = find_actor_decl_by_name(program, type_name);
    if (decl_is_subject_host(decl))
        return decl;
    return NULL;
}

static ASTNode *
find_domain_decl_by_name(ASTNode *program, ASTNodeType decl_type,
                         const char *name);

static ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name);

static size_t
subject_host_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    if (decl->type == AST_ACTOR_DECL)
        return decl->data.actor_decl.field_count;
    return 0;
}

static ClassField *
subject_host_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    if (decl->type == AST_ACTOR_DECL) {
        if (index < decl->data.actor_decl.field_count)
            return decl->data.actor_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static Type *
create_overlay_nominal_type(const char *name)
{
    Type *type = calloc(1, sizeof(Type));
    if (type == NULL)
        return TYPE_UNKNOWN;
    type->kind = TYPE_KIND_CLASS;
    type->nominal_flavor = TYPE_NOMINAL_CLASS;
    type->name = pergyra_strdup(name);
    return type;
}

static size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_SYSTEMIC_DECL:
        return decl->data.systemic_decl.party_count
            + decl->data.systemic_decl.shared_count;
    case AST_WORLD_DECL:
        return decl->data.world_decl.systemic_count
            + decl->data.world_decl.zone_count
            + decl->data.world_decl.shared_count;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.slot_count
            + decl->data.zone_decl.shared_count;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.slot_count
            + decl->data.relation_decl.shared_count;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.slot_count
            + decl->data.effect_decl.shared_count;
    default:
        return 0;
    }
}

static ASTNode *
overlay_field_decl_at(ASTNode *decl, size_t index, const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_SYSTEMIC_DECL:
        if (index < decl->data.systemic_decl.party_count)
            return NULL;
        index -= decl->data.systemic_decl.party_count;
        if (index < decl->data.systemic_decl.shared_count) {
            ASTNode *shared = decl->data.systemic_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_WORLD_DECL:
        if (index < decl->data.world_decl.systemic_count)
            return NULL;
        index -= decl->data.world_decl.systemic_count;
        if (index < decl->data.world_decl.zone_count)
            return NULL;
        index -= decl->data.world_decl.zone_count;
        if (index < decl->data.world_decl.shared_count) {
            ASTNode *shared = decl->data.world_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_ZONE_DECL:
        if (index < decl->data.zone_decl.slot_count) {
            ASTNode *slot = decl->data.zone_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.zone_decl.slot_count;
        if (index < decl->data.zone_decl.shared_count) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_RELATION_DECL:
        if (index < decl->data.relation_decl.slot_count) {
            ASTNode *slot = decl->data.relation_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.relation_decl.slot_count;
        if (index < decl->data.relation_decl.shared_count) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_EFFECT_DECL:
        if (index < decl->data.effect_decl.slot_count) {
            ASTNode *slot = decl->data.effect_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.effect_decl.slot_count;
        if (index < decl->data.effect_decl.shared_count) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    default:
        break;
    }

    return NULL;
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return find_type_decl_by_name(program, name);
    case SYMBOL_ACTOR:
        return find_actor_decl_by_name(program, name);
    case SYMBOL_SYSTEMIC:
        return find_domain_decl_by_name(program, AST_SYSTEMIC_DECL, name);
    case SYMBOL_WORLD:
        return find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

static bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx)
{
    if (!type_is_owned_slot_handle(type) || ctx == NULL)
        return false;
    return type_is_subject_type(type->data.slot.inner_type, ctx);
}

static bool
type_is_class_object_type(const Type *type, SemanticContext *ctx)
{
    return type_is_subject_type(type, ctx);
}

static bool
type_is_nominal_host_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl != NULL)
        return !decl->data.class_decl.is_struct;
    return find_actor_decl_by_name(ctx->program_root, type->name) != NULL;
}

static bool
type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    if (type->nominal_flavor == TYPE_NOMINAL_SUBJECT)
        return true;

    decl = find_subject_host_decl_by_name(ctx->program_root, type->name);
    return decl_is_subject_host(decl);
}

static bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL
        || ctx == NULL) {
        return false;
    }

    decl = find_type_decl_by_name(ctx->program_root,
        expr->data.call.callee->data.identifier.name);
    if (decl != NULL)
        return !decl->data.class_decl.is_struct;
    decl = find_actor_decl_by_name(ctx->program_root,
        expr->data.call.callee->data.identifier.name);
    return decl != NULL;
}

static bool
expr_is_qubit_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimQubit") == 0;
}

static bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimDeviceSlot") == 0;
}

static bool
expr_is_movable_resource_transfer_source(const ASTNode *expr)
{
    if (expr == NULL)
        return false;

    switch (expr->type) {
    case AST_IDENTIFIER:
    case AST_CALL:
    case AST_CHANNEL_RECV:
    case AST_AWAIT_EXPR:
        return true;
    default:
        return false;
    }
}

static bool
slot_transfer_compatible(const Type *from, const Type *to)
{
    if (from == NULL || to == NULL)
        return false;
    if (from->kind != TYPE_KIND_SLOT || to->kind != TYPE_KIND_SLOT)
        return false;
    if (from->data.slot.access_mode != SLOT_ACCESS_MOVE_TOKEN)
        return false;
    if (to->data.slot.access_mode != SLOT_ACCESS_OWNED)
        return false;
    if (from->data.slot.is_secure != to->data.slot.is_secure)
        return false;
    return type_is_assignable(from->data.slot.inner_type, to->data.slot.inner_type)
        && type_is_assignable(to->data.slot.inner_type, from->data.slot.inner_type);
}

static bool
expr_is_movable_resource_boundary(const ASTNode *expr)
{
    return expr != NULL
        && (expr->type == AST_CHANNEL_RECV
            || expr->type == AST_AWAIT_EXPR);
}

static bool
expr_is_static_member_access(const ASTNode *expr)
{
    if (expr == NULL || expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL) {
        return false;
    }

    if (expr->data.member.object->type == AST_IDENTIFIER) {
        const char *name = expr->data.member.object->data.identifier.name;
        return name != NULL && name[0] >= 'A' && name[0] <= 'Z';
    }

    return expr_is_static_member_access(expr->data.member.object);
}

static char *
flatten_static_member_access(const ASTNode *expr, char separator)
{
    if (expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER)
        return expr->data.identifier.name != NULL
            ? pergyra_strdup(expr->data.identifier.name) : NULL;

    if (expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL
        || expr->data.member.name == NULL) {
        return NULL;
    }

    char *lhs = flatten_static_member_access(expr->data.member.object, separator);
    if (lhs == NULL)
        return NULL;

    size_t lhs_len = strlen(lhs);
    size_t rhs_len = strlen(expr->data.member.name);
    char *result = malloc(lhs_len + rhs_len + 2);
    if (result == NULL) {
        free(lhs);
        return NULL;
    }

    memcpy(result, lhs, lhs_len);
    result[lhs_len] = separator;
    memcpy(result + lhs_len + 1, expr->data.member.name, rhs_len);
    result[lhs_len + rhs_len + 1] = '\0';
    free(lhs);
    return result;
}

static Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
    if (sym == NULL) {
        if (name_looks_qualified(display_name)) {
            semantic_error(ctx, expr,
                "Undefined function '%s' (check namespace spelling or export visibility)",
                display_name);
        } else {
            semantic_error(ctx, expr, "Undefined function '%s'", display_name);
        }
        return TYPE_UNKNOWN;
    }

    if (sym->kind == SYMBOL_CLASS || sym->kind == SYMBOL_PARTY
        || sym->kind == SYMBOL_ACTOR || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT || sym->kind == SYMBOL_SYSTEMIC
        || sym->kind == SYMBOL_WORLD || sym->kind == SYMBOL_ZONE) {
        if (ctx->program_root != NULL) {
            ASTNode *decl = constructor_decl_for_symbol_kind(ctx->program_root,
                sym->kind, display_name);
            size_t field_count = 0;
            bool decl_is_generic = false;
            if (decl != NULL && decl->type == AST_CLASS_DECL) {
                field_count = decl->data.class_decl.field_count;
                decl_is_generic =
                    (decl->data.class_decl.generic_params != NULL
                     && decl->data.class_decl.generic_params->count > 0);
            } else if (decl != NULL && decl->type == AST_ACTOR_DECL) {
                field_count = decl->data.actor_decl.field_count;
            } else if (decl != NULL
                       && (decl->type == AST_RELATION_DECL
                           || decl->type == AST_EFFECT_DECL
                           || decl->type == AST_SYSTEMIC_DECL
                           || decl->type == AST_WORLD_DECL
                           || decl->type == AST_ZONE_DECL)) {
                field_count = overlay_field_count(decl);
            }
            if (decl != NULL) {
                size_t provided = expr->data.call.arg_count;
                /* Skip field-type validation for generic classes — the
                 * generic params (T, U) aren't in scope at the call site.
                 * Type safety is handled by the let-annotation type. */
                if (provided > field_count) {
                    semantic_error(ctx, expr,
                        "Constructor '%s' accepts at most %zu positional field argument(s), got %zu",
                        display_name, field_count, provided);
                } else if (!decl_is_generic) {
                    for (size_t i = 0; i < provided; i++) {
                        const char *field_name = NULL;
                        ASTNode *field_type_node = NULL;
                        if (decl->type == AST_CLASS_DECL || decl->type == AST_ACTOR_DECL) {
                            ClassField *field = subject_host_field_at(decl, i);
                            if (field != NULL) {
                                field_name = field->name;
                                field_type_node = field->type;
                            }
                        } else {
                            field_type_node = overlay_field_decl_at(decl, i, &field_name);
                        }
                        if (field_type_node == NULL)
                            continue;
                        Type *field_type = resolve_type_node(field_type_node, ctx);
                        Type *arg_type = type_check_expression(expr->data.call.arguments[i], ctx);
                        if (field_type != NULL && arg_type != NULL
                            && !type_is_assignable(arg_type, field_type)) {
                            semantic_error(ctx, expr->data.call.arguments[i],
                                "Constructor '%s' argument %zu initializes field '%s' of type '%s', got '%s'",
                                display_name, i + 1,
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                    }
                } else {
                    /* For generic classes, still type-check expressions */
                    for (size_t i = 0; i < provided; i++)
                        type_check_expression(expr->data.call.arguments[i], ctx);
                }
            }
        }
        sym->is_used = true;
        return sym->type;
    }

    if (sym->type->kind != TYPE_KIND_FUNCTION) {
        semantic_error(ctx, expr, "'%s' is not a function", display_name);
        return TYPE_UNKNOWN;
    }
    sym->is_used = true;
    semantic_record_effect(ctx, type_function_effects(sym->type));

    size_t expected = sym->type->data.function.param_count;
    size_t provided = expr->data.call.arg_count;
    if (provided != expected) {
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu",
            display_name, expected, provided);
        return sym->type->data.function.return_type;
    }

    for (size_t i = 0; i < provided; i++) {
        Type *param_type = sym->type->data.function.param_types[i];
        Type *arg_type = type_is_movable_resource_handle(param_type)
            ? type_check_qubit_use(expr->data.call.arguments[i], ctx)
            : type_check_expression(expr->data.call.arguments[i], ctx);

        if (type_is_movable_resource_handle(arg_type)
            || type_is_movable_resource_handle(param_type)) {
            if (!type_is_movable_resource_handle(arg_type)
                || !type_is_movable_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Movable resource argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
                    resource_handle_display_name(param_type));
                continue;
            }
            consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            continue;
        }

        if (type_is_anchored_resource_handle(arg_type)
            || type_is_anchored_resource_handle(param_type)) {
            if (!type_is_anchored_resource_handle(arg_type)
                || !type_is_anchored_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Resource handle argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            /* Check own/ref qualifier from function declaration AST */
            {
                ParamMode pmode = PARAM_MODE_DEFAULT;
                if (ctx->program_root != NULL) {
                    ASTNode *prog = ctx->program_root;
                    for (size_t si = 0; si < prog->data.program.count; si++) {
                        ASTNode *stmt = prog->data.program.statements[si];
                        if (stmt != NULL && stmt->type == AST_FUNC_DECL
                            && stmt->data.func_decl.name != NULL
                            && strcmp(stmt->data.func_decl.name, display_name) == 0
                            && i < stmt->data.func_decl.param_count) {
                            pmode = stmt->data.func_decl.params[i]->mode;
                            break;
                        }
                    }
                }
                if (pmode == PARAM_MODE_OWN) {
                    /* own: move — consume the source slot */
                    if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                        const char *src = expr->data.call.arguments[i]->data.identifier.name;
                        Symbol *src_sym = scope_lookup(ctx->scope, src);
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
                            if (src_sym->slot_info.state == SLOT_STATE_RELEASED)
                                semantic_error(ctx, expr->data.call.arguments[i],
                                    "Cannot move from released slot '%s'", src);
                            else
                                src_sym->slot_info.state = SLOT_STATE_RELEASED;
                        }
                    }
                    continue;
                } else if (pmode == PARAM_MODE_REF) {
                    /* ref: borrow — source remains valid */
                    continue;
                }
            }
            semantic_error(ctx, expr->data.call.arguments[i],
                "Slot arguments require 'own' or 'ref' qualifier in function signature; "
                "use 'func F(own s: Slot<T>)' to take ownership or "
                "'func F(ref s: Slot<T>)' to borrow");
            continue;
        }

        require_assignable(arg_type, param_type, expr->data.call.arguments[i], ctx);
    }

    return sym->type->data.function.return_type;
}

static Symbol *
lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return NULL;
    }
    return scope_lookup(ctx->scope, expr->data.identifier.name);
}

bool
consume_qubit_value(ASTNode *expr, SemanticContext *ctx, const char *action)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return false;

    if (sym->is_consumed) {
        return false;
    }

    sym->is_consumed = true;
    sym->is_used = true;
    (void)action;
    return true;
}

const char *
qubit_state_name(QubitSemanticState state)
{
    switch (state) {
    case QUBIT_STATE_NONE:           return "NONE";
    case QUBIT_STATE_SUPERPOSITION:  return "SUPERPOSITION";
    case QUBIT_STATE_ENTANGLED:      return "ENTANGLED";
    case QUBIT_STATE_COLLAPSED:      return "COLLAPSED";
    case QUBIT_STATE_CLASSICAL:      return "CLASSICAL";
    default:                         return "UNKNOWN";
    }
}

QubitSemanticState
get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return QUBIT_STATE_NONE;
    return sym->qubit_info.semantic_state;
}

bool
set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                         QubitSemanticState new_state)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return false;
    sym->qubit_info.semantic_state = new_state;
    return true;
}

/* -----------------------------------------------------------------
 * Compile-time entanglement pool tracking
 * ----------------------------------------------------------------- */

int32_t
alloc_entangle_pool(SemanticContext *ctx)
{
    return ctx->next_entangle_pool++;
}

int32_t
get_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return -1;
    return sym->qubit_info.entangle_pool_id;
}

void
set_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx,
                        int32_t pool_id)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return;
    sym->qubit_info.entangle_pool_id = pool_id;
}

void
merge_entangle_pools(SemanticContext *ctx,
                     int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool || dst_pool < 0 || src_pool < 0)
        return;
    /* Walk the entire scope chain and re-assign src → dst */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == src_pool) {
                sym->qubit_info.entangle_pool_id = dst_pool;
            }
        }
    }
}

void
propagate_collapse_to_pool(SemanticContext *ctx, int32_t pool_id)
{
    if (pool_id < 0)
        return;
    /* Walk the entire scope chain and collapse all members of this pool */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == pool_id
                && sym->qubit_info.semantic_state != QUBIT_STATE_COLLAPSED
                && sym->qubit_info.semantic_state != QUBIT_STATE_CLASSICAL) {
                sym->qubit_info.semantic_state = QUBIT_STATE_COLLAPSED;
            }
        }
    }
}

Type *
type_check_qubit_use(ASTNode *expr, SemanticContext *ctx)
{
    if (expr != NULL && expr->type == AST_IDENTIFIER) {
        Symbol *sym = lookup_identifier_symbol(expr, ctx);
        if (sym == NULL) {
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if (!type_is_qubit(sym->type)) {
            semantic_error(ctx, expr,
                "Expected a movable resource handle (currently QubitSlot), got '%s'",
                sym->type->name);
            return TYPE_UNKNOWN;
        }
        if (sym->is_consumed) {
            semantic_error(ctx, expr,
                "%s '%s' was moved or released and cannot be used again; create a fresh value or keep ownership in one binding",
                resource_handle_display_name(sym->type),
                expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    if (expr_is_movable_resource_boundary(expr)) {
        semantic_error(ctx, expr,
            "Movable resources from recv/await must first be bound to a named variable before use");
        return TYPE_UNKNOWN;
    }

    return type_check_expression(expr, ctx);
}

Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return TYPE_UNKNOWN;
    if (index >= type->data.constructed.arg_count)
        return TYPE_UNKNOWN;
    return type->data.constructed.args[index];
}

Type *
resolve_type_node(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return TYPE_VOID;

    if (node->type == AST_CHANNEL_TYPE) {
        Type *inner = resolve_type_node(node->data.channel_type.element_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_CHANNEL, args, 1);
    }

    if (node->type == AST_FUTURE_TYPE) {
        Type *inner = resolve_type_node(node->data.future_type.value_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    if (node->type != AST_TYPE)
        return TYPE_UNKNOWN;

    const char *name = node->data.type.name;

    if (strcmp(name, "Slot") == 0 || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "ReadView") == 0 || strcmp(name, "WriteView") == 0
        || strcmp(name, "MoveToken") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 1) {
            semantic_error(ctx, node,
                "%s requires exactly one type argument", name);
            return TYPE_UNKNOWN;
        }
        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        if (strcmp(name, "SecureSlot") == 0)
            return type_create_slot(inner, true);
        if (strcmp(name, "ReadView") == 0)
            return type_create_read_view(inner);
        if (strcmp(name, "WriteView") == 0)
            return type_create_write_view(inner);
        if (strcmp(name, "MoveToken") == 0)
            return type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
        return type_create_slot(inner, false);
    }

    if (strcmp(name, "Array") == 0 || strcmp(name, "Slice") == 0
        || strcmp(name, "Box") == 0 || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0 || strcmp(name, "Channel") == 0
        || strcmp(name, "Future") == 0 || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "DeviceSlot") == 0 || strcmp(name, "Result") == 0
        || strcmp(name, "Option") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 1) {
            semantic_error(ctx, node,
                "%s requires exactly one type argument", name);
            return TYPE_UNKNOWN;
        }

        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *constructor = TYPE_UNKNOWN;
        if (strcmp(name, "Array") == 0) constructor = TYPE_ARRAY;
        else if (strcmp(name, "Slice") == 0) constructor = TYPE_SLICE;
        else if (strcmp(name, "Box") == 0) constructor = TYPE_BOX;
        else if (strcmp(name, "Rc") == 0) constructor = TYPE_RC;
        else if (strcmp(name, "Weak") == 0) constructor = TYPE_WEAK;
        else if (strcmp(name, "Channel") == 0) constructor = TYPE_CHANNEL;
        else if (strcmp(name, "Future") == 0) constructor = TYPE_FUTURE;
        else if (strcmp(name, "RemoteFuture") == 0) constructor = TYPE_REMOTE_FUTURE;
        else if (strcmp(name, "DeviceSlot") == 0) constructor = TYPE_DEVICE_SLOT;
        else if (strcmp(name, "Result") == 0) constructor = TYPE_RESULT;
        else if (strcmp(name, "Option") == 0) constructor = TYPE_OPTION;
        Type *args[1] = { inner };
        return type_create_constructed(constructor, args, 1);
    }

    /* User-defined generic class: Node<Int>, Pair<String> etc.
     * If the name resolves to a SYMBOL_CLASS and generic_args are present,
     * build a TYPE_KIND_CONSTRUCTED type so the class specialization can
     * be tracked through the type system. */
    if (node->data.type.generic_args != NULL
        && node->data.type.generic_args->count > 0) {
        Symbol *sym = scope_lookup(ctx->scope, name);
        if (sym != NULL && sym->kind == SYMBOL_CLASS && sym->type != NULL) {
            GenericParams *ga = node->data.type.generic_args;
            size_t argc = ga->count;
            Type **args = calloc(argc, sizeof(Type *));
            if (args != NULL) {
                for (size_t i = 0; i < argc; i++)
                    args[i] = resolve_generic_type_arg(ga->params[i], ctx, node);
                Type *result = type_create_constructed(sym->type, args, argc);
                free(args);
                return result;
            }
        }
    }

    return resolve_named_type(name, ctx, node);
}

bool
require_assignable(Type *from, Type *to,
                    const ASTNode *site, SemanticContext *ctx)
{
    if (type_is_assignable(from, to))
        return true;

    /* Slot sugar: allow assigning T to Slot<T> (auto Claim+Write) */
    if (to->kind == TYPE_KIND_SLOT && to->data.slot.inner_type != NULL
        && type_is_assignable(from, to->data.slot.inner_type))
        return true;

    semantic_error(ctx, site,
        "Type mismatch: cannot assign '%s' to '%s'",
        from->name, to->name);
    return false;
}

Type *
wrap_constructed(Type *constructor, Type *inner)
{
    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}

static const char *
operator_overload_suffix(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

static bool
operator_method_name_matches(TokenType op, const char *name)
{
    static const struct {
        TokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }

    return false;
}

static ASTNode *
find_role_decl_in_program(ASTNode *program, const char *role_name)
{
    if (program == NULL || program->type != AST_PROGRAM || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_role_operator_method(ASTNode *role, ASTNode *program, TokenType op,
                          int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc->data.include_stmt.role_name);
        ASTNode *method = find_role_operator_method(included, program, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static Type *
type_check_role_operator_overload(ASTNode *expr, SemanticContext *ctx,
                                  Type *left, Type *right)
{
    if (ctx->program_root == NULL || left == NULL || left->name == NULL)
        return NULL;

    ASTNode *program = ctx->program_root;
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, left->name) != 0) {
            continue;
        }

        ASTNode *method = find_role_operator_method(
            stmt, ctx->program_root, expr->data.binary.op.type, 0);
        if (method == NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        Type *rhs_type = TYPE_INT;
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = resolve_type_node(rhs_param->type, ctx);
        if (!type_is_assignable(right, rhs_type))
            continue;

        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    return NULL;
}

static Type *
type_check_operator_overload(ASTNode *expr, SemanticContext *ctx,
                             Type *left, Type *right)
{
    const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
    if (suffix == NULL || left == NULL || right == NULL || left->name == NULL)
        return NULL;

    char fn_name[256];
    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, left->name);

    Symbol *sym = scope_lookup(ctx->scope, fn_name);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION
        || sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;

    if (sym->type->data.function.param_count != 2)
        return NULL;

    Type *lhs = sym->type->data.function.param_types[0];
    Type *rhs = sym->type->data.function.param_types[1];
    if (!type_is_assignable(left, lhs) || !type_is_assignable(right, rhs))
        return NULL;

    sym->is_used = true;
    return sym->type->data.function.return_type;
}

static Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    if (expr->data.array_literal.count == 0)
        return wrap_constructed(TYPE_ARRAY, TYPE_INT);

    Type *elem_type = type_check_expression(expr->data.array_literal.elements[0], ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;

    for (size_t i = 1; i < expr->data.array_literal.count; i++) {
        Type *next = type_check_expression(expr->data.array_literal.elements[i], ctx);
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error(ctx, expr->data.array_literal.elements[i],
                "Array literal element type mismatch: expected '%s', got '%s'",
                elem_type->name, next->name);
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_ARRAY, elem_type);
}

/* -----------------------------------------------------------------
 * Expression type checker
 * ----------------------------------------------------------------- */

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        return TYPE_INT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_IDENTIFIER: {
        Symbol *sym = scope_lookup(ctx->scope,
                                    expr->data.identifier.name);
        if (sym == NULL) {
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if ((type_is_qubit(sym->type) || type_is_move_token(sym->type))
            && sym->is_consumed) {
            semantic_error(ctx, expr,
                "%s '%s' was moved or released and cannot be used again; create a fresh value or keep ownership in one binding",
                resource_handle_display_name(sym->type),
                expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    case AST_BINARY:
        return type_check_binary(expr, ctx);

    case AST_UNARY:
        return type_check_unary(expr, ctx);

    case AST_CALL:
        return type_check_call(expr, ctx);

    case AST_MEMBER_ACCESS:
        return type_check_member_access(expr, ctx);

    case AST_ARRAY_ACCESS:
        return type_check_array_access(expr, ctx);

    case AST_ARRAY_LITERAL:
        return type_check_array_literal(expr, ctx);

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error(ctx, expr,
                "'await' used outside of async function");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
        {
            Type *future_type = type_check_expression(expr->data.await_expr.expression, ctx);
            if (future_type != NULL
                && future_type->kind == TYPE_KIND_CONSTRUCTED
                && (type_equals(future_type->data.constructed.constructor, TYPE_FUTURE)
                    || type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE))
                && future_type->data.constructed.arg_count == 1) {
                Type *inner = future_type->data.constructed.args[0];
                if (type_is_anchored_resource_handle(inner)) {
                    semantic_error(ctx, expr->data.await_expr.expression,
                        "'await' cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; await a plain value or movable transfer result instead");
                    return TYPE_UNKNOWN;
                }
                /* RemoteFuture<T> → Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled.  Local Future<T> → T as before. */
                if (type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE)) {
                    Type *result_args[1] = { inner };
                    return type_create_constructed(TYPE_RESULT, result_args, 1);
                }
                return inner;
            }
            semantic_error(ctx, expr->data.await_expr.expression,
                "'await' requires Future<T> or RemoteFuture<T>");
            return TYPE_UNKNOWN;
        }

    case AST_SPAWN_EXPR:
        return type_check_spawn_expr(expr, ctx);

    case AST_CHANNEL_SEND:
        return type_check_channel_send(expr, ctx);

    case AST_CHANNEL_RECV:
        return type_check_channel_recv(expr, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = type_check_expression(expr->data.binary.left,  ctx);
    Type *right = type_check_expression(expr->data.binary.right, ctx);

    if (type_is_slot_handle(left) && left->data.slot.inner_type != NULL)
        left = left->data.slot.inner_type;
    if (type_is_slot_handle(right) && right->data.slot.inner_type != NULL)
        right = right->data.slot.inner_type;

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

    /* If either operand is unknown, skip checks and propagate */
    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        TokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    /* Comparison operators → Bool */
    TokenType op = expr->data.binary.op.type;
    if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
        || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
        || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL) {
        if (!type_equals(left, right)) {
            semantic_error(ctx, expr,
                "Cannot compare '%s' and '%s'",
                left->name, right->name);
        }
        return TYPE_BOOL;
    }

    /* Arithmetic: both operands must match */
    if (!type_equals(left, right)) {
        semantic_error(ctx, expr,
            "Type mismatch in binary operation: '%s' and '%s'",
            left->name, right->name);
        return TYPE_UNKNOWN;
    }

    return left;
}

Type *
type_check_unary(ASTNode *expr, SemanticContext *ctx)
{
    Type *operand = type_check_expression(expr->data.unary.operand, ctx);

    TokenType op = expr->data.unary.op.type;
    if (op == TOKEN_NOT) {
        if (!type_equals(operand, TYPE_BOOL)) {
            semantic_error(ctx, expr,
                "'!' operator requires Bool, got '%s'", operand->name);
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_MINUS) {
        if (!type_equals(operand, TYPE_INT)
            && !type_equals(operand, TYPE_FLOAT)) {
            semantic_error(ctx, expr,
                "Unary '-' requires numeric type, got '%s'",
                operand->name);
        }
        return operand;
    }

    /* Postfix ?: try/propagate — unwrap Result<T> to T, propagate error */
    if (op == TOKEN_QUESTION) {
        if (!type_is_constructed_named(operand, "Result")) {
            semantic_error(ctx, expr,
                "'?' operator requires Result<T>, got '%s'",
                operand->name);
            return TYPE_UNKNOWN;
        }
        /* Return the inner T of Result<T> */
        return type_get_constructed_arg(operand, 0);
    }

    return operand;
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = expr->data.call.callee;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        BuiltinKind bk   = builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        /* Channel(capacity) is a built-in constructor that the transpiler
         * handles; let it pass without a symbol table entry. The actual
         * type is resolved from the annotation in emit_let_decl. */
        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;  /* type inferred from let annotation */

        /* Result built-in functions — Ok/Err still return TYPE_UNKNOWN
         * (type from let annotation); others go to stdlib_call */
        if (strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
            return TYPE_UNKNOWN;

        /* Standard library built-in functions */
        {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        return type_check_function_symbol_call(expr, sym, name, ctx);
    }

    /* Callee is a member access (method call) */
    if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode *object = callee->data.member.object;
        const char *method_name = callee->data.member.name;

        if (expr_is_static_member_access(callee)) {
            char *flat_name = flatten_static_member_access(callee, '_');
            char *display_name = flatten_static_member_access(callee, '.');
            Symbol *sym = flat_name != NULL
                ? scope_lookup(ctx->scope, flat_name)
                : NULL;
            if (sym == NULL && callee->data.member.name != NULL) {
                sym = scope_lookup(ctx->scope, callee->data.member.name);
            }
            Type *result = type_check_function_symbol_call(
                expr, sym, display_name != NULL ? display_name : "<member>", ctx);
            free(flat_name);
            free(display_name);
            return result;
        }

        if (!(object != NULL
              && object->type == AST_IDENTIFIER
              && object->data.identifier.name != NULL
              && object->data.identifier.name[0] >= 'A'
              && object->data.identifier.name[0] <= 'Z')) {
            /* Resolve object type for normal method calls.
             * Namespace/static-style calls like Math.Add are lowered later. */
            Type *object_type = type_check_expression(object, ctx);
            ASTNode *class_decl;

            if (type_is_nominal_host_type(object_type, ctx)
                && object_type->name != NULL
                && method_name != NULL) {
                class_decl = find_type_decl_by_name(ctx->program_root,
                    object_type->name);
                if (class_decl != NULL) {
                    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
                        ASTNode *method = class_decl->data.class_decl.methods[i];
                        if (method == NULL || method->type != AST_FUNC_DECL
                            || method->data.func_decl.name == NULL)
                            continue;
                        if (strcmp(method->data.func_decl.name, method_name) == 0) {
                            if (method->data.func_decl.return_type != NULL)
                                return resolve_type_node(
                                    method->data.func_decl.return_type, ctx);
                            return TYPE_VOID;
                        }
                    }
                }
            }
        }
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    if (expr_is_static_member_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && expr->data.member.name != NULL)
            sym = scope_lookup(ctx->scope, expr->data.member.name);
        if (sym != NULL) {
            sym->is_used = true;
            free(flat_name);
            free(display_name);
            return sym->type;
        }
        semantic_error(ctx, expr,
            "Undefined symbol '%s' (check namespace spelling or export visibility)",
            display_name != NULL ? display_name : "<member>");
        free(flat_name);
        free(display_name);
        return TYPE_UNKNOWN;
    }

    Type *object_type = type_check_expression(expr->data.member.object, ctx);

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(expr->data.member.name, "Length") == 0) {
        return TYPE_INT;
    }

    /* Resolve class field types by looking up the class declaration AST */
    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && ctx->program_root != NULL) {
        const char *field_name = expr->data.member.name;
        ASTNode *prog = ctx->program_root;
        for (size_t si = 0; si < prog->data.program.count; si++) {
            ASTNode *stmt = prog->data.program.statements[si];
            if (stmt == NULL || stmt->type != AST_CLASS_DECL)
                continue;
            if (stmt->data.class_decl.name == NULL
                || strcmp(stmt->data.class_decl.name, object_type->name) != 0)
                continue;
            /* Found the class — search its fields */
            for (size_t fi = 0; fi < stmt->data.class_decl.field_count; fi++) {
                ClassField *cf = stmt->data.class_decl.fields[fi];
                if (cf == NULL || cf->name == NULL)
                    continue;
                if (strcmp(cf->name, field_name) == 0)
                    return resolve_type_node(cf->type, ctx);
            }
            /* Field not found in this class — fall through to UNKNOWN */
            break;
        }
        /* Accept any remaining field access on class types */
        return TYPE_UNKNOWN;
    }

    /* Unknown member access — allow without error for class/enum types */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM))
        return TYPE_UNKNOWN;

    return TYPE_UNKNOWN;
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = type_check_expression(expr->data.array_access.array, ctx);
    Type *index_type  = type_check_expression(expr->data.array_access.index, ctx);

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error(ctx, expr->data.array_access.index,
            "Array index must be Int, got '%s'", index_type->name);
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return type_get_constructed_arg(object_type, 0);
    }

    semantic_error(ctx, expr->data.array_access.array,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        object_type->name);
    return TYPE_UNKNOWN;
}

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    Type *value_type  = type_check_expression(expr->data.assignment.value,  ctx);
    Type *target_type = type_check_expression(expr->data.assignment.target, ctx);

    if (type_is_slot_handle(target_type)
        && target_type->data.slot.inner_type != NULL
        && !type_is_resource_handle(value_type)
        && type_is_assignable(value_type, target_type->data.slot.inner_type)) {
        return target_type;
    }

    if (type_is_resource_handle(target_type) || type_is_resource_handle(value_type)) {
        semantic_error(ctx, expr,
            "Resource handle assignment is not allowed; anchored handles (Slot/SecureSlot/DeviceSlot) cannot be copied or rebound with '=', and movable handles must be transferred through a new binding. Use Read/Write for Slot<T>, keep DeviceSlot local, move a QubitSlot into a new binding, or Claim... to create a fresh handle");
        return target_type;
    }

    if (type_is_class_object_type(target_type, ctx)
        || type_is_class_object_type(value_type, ctx)) {
        semantic_error(ctx, expr,
            "Subject assignment is not allowed; subjects are identity-bearing active hosts. Mutate fields or methods on the existing subject instead of rebinding it with '='");
        return target_type;
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */

bool
type_check_let_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;

    /* Check for duplicate in current scope */
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error(ctx, node,
            "Redeclaration of '%s' in the same scope", name);
        return false;
    }

    /*
     * Detect ClaimSlot / ClaimSecureSlot calls so we can register
     * a SYMBOL_SLOT with proper metadata.
     */
    bool is_slot_decl = false;
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name =
            init->data.call.callee->data.identifier.name;
        BuiltinKind bk = builtin_resolve(callee_name);

        if (bk == BUILTIN_CLAIM_SLOT || bk == BUILTIN_CLAIM_SECURE_SLOT) {
            is_slot_decl = true;
            bool is_secure = (bk == BUILTIN_CLAIM_SECURE_SLOT);
            if (is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);

            /* Resolve inner type from annotation if present.
             * If the annotation is already a Slot type (e.g. Slot<String>),
             * use it directly instead of double-wrapping. */
            Type *slot_type = NULL;
            if (ann != NULL) {
                Type *ann_type = resolve_type_node(ann, ctx);
                if (ann_type->kind == TYPE_KIND_SLOT) {
                    slot_type = ann_type;
                    is_secure = ann_type->data.slot.is_secure;
                } else {
                    slot_type = type_create_slot(ann_type, is_secure);
                }
            } else {
                slot_type = type_create_slot(TYPE_INT, is_secure);
            }
            char token_name_buf[256];
            const char *paired_token = NULL;
            if (is_secure) {
                snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
                paired_token = token_name_buf;
            }
            Symbol *sym       = symbol_create_slot(name, slot_type,
                                                    is_secure, paired_token,
                                                    node->line, node->column);
            scope_declare(ctx->scope, sym);
            if (is_secure) {
                Symbol *tok = symbol_create_token(paired_token, name,
                                                  node->line, node->column);
                if (!scope_declare(ctx->scope, tok))
                    symbol_destroy(tok);
            }
            scope_register_slot(ctx->scope, sym);
            return true;
        }
    }

    /* Normal variable declaration with type inference */
    Type *init_type = (init != NULL)
                      ? type_check_expression(init, ctx)
                      : TYPE_VOID;

    Type *decl_type;
    
    /* Type inference: if no annotation, infer from initializer */
    if (ann != NULL) {
        /* Explicit type annotation */
        decl_type = resolve_type_node(ann, ctx);
        if (init != NULL) {
            if (init->type == AST_CALL
                && init->data.call.callee->type == AST_IDENTIFIER
                && strcmp(init->data.call.callee->data.identifier.name,
                          "BoxArray") == 0) {
                init_type = decl_type;
            }
            if (!slot_transfer_compatible(init_type, decl_type))
                require_assignable(init_type, decl_type, init, ctx);
        }
    } else if (init != NULL) {
        /* Infer type from initializer */
        decl_type = init_type;
        
        /* For generic types like Box<T>, Array<T>, Result<T,E>, 
           ensure the inferred type is concrete */
        if (init_type->kind == TYPE_KIND_GENERIC) {
            semantic_error(ctx, init, 
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error(ctx, node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }

    if (type_is_class_object_type(decl_type, ctx)
        && init != NULL
        && type_is_class_object_type(init_type, ctx)
        && !expr_is_class_constructor_call(init, ctx)
        && (init->type == AST_IDENTIFIER || init->type == AST_MEMBER_ACCESS)) {
        semantic_error(ctx, node,
            "Subjects cannot be copied into a new binding. Construct a fresh subject or mutate the existing one");
    }

    if (type_is_qubit(decl_type)) {
        bool valid_qubit_init = false;
        if (init_type == TYPE_UNKNOWN) {
            valid_qubit_init = true;
        } else if (expr_is_qubit_claim(init)) {
            valid_qubit_init = true;
        } else if (init != NULL && init_type != NULL && type_is_qubit(init_type)) {
            if (init->type == AST_IDENTIFIER) {
                valid_qubit_init = true;
                consume_qubit_value(init, ctx, "moved");
            } else if (expr_is_movable_resource_transfer_source(init)) {
                valid_qubit_init = true;
            }
        }
        if (!valid_qubit_init) {
            semantic_error(ctx, node,
                "QubitSlot is a movable resource handle; it must come from ClaimQubit(), a moved QubitSlot value, or a transfer boundary such as recv/await. Plain copying is not allowed");
        }
    }

    /* Slot<T> move semantics: when assigning from another Slot variable,
     * consume (invalidate) the source.  ClaimSlot() creates fresh. */
    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode == SLOT_ACCESS_OWNED
        && init != NULL && init->type == AST_IDENTIFIER
        && init_type != NULL && init_type->kind == TYPE_KIND_SLOT
        && init_type->data.slot.access_mode == SLOT_ACCESS_OWNED) {
        /* Move: source Slot becomes invalid after this point */
        const char *src_name = init->data.identifier.name;
        Symbol *src_sym = scope_lookup(ctx->scope, src_name);
        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, init,
                    "Cannot move from released slot '%s'", src_name);
            } else {
                src_sym->slot_info.state = SLOT_STATE_RELEASED;
            }
        }
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode != SLOT_ACCESS_OWNED) {
        const char *source_slot = NULL;
        if (init != NULL && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (callee_name != NULL
                && (strcmp(callee_name, "ViewRead") == 0
                    || strcmp(callee_name, "ViewWrite") == 0
                    || strcmp(callee_name, "Move") == 0)) {
                source_slot = init->data.call.arguments[0]->data.identifier.name;
            }
        }

        Symbol *sym = symbol_create_view(name, decl_type, source_slot,
            node->line, node->column);
        scope_declare(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT) {
        if (decl_type->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (slot_transfer_compatible(init_type, decl_type)) {
            if (init != NULL && init->type == AST_IDENTIFIER) {
                Symbol *move_sym = scope_lookup(ctx->scope, init->data.identifier.name);
                if (move_sym != NULL)
                    move_sym->is_consumed = true;
            }
        }
        if (init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error(ctx, node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding; use a fresh claim or initialize from an inner value instead");
        }
        char token_name_buf[256];
        const char *paired_token = NULL;
        if (decl_type->data.slot.is_secure) {
            snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
            paired_token = token_name_buf;
        }
        Symbol *sym = symbol_create_slot(name, decl_type,
            decl_type->data.slot.is_secure, paired_token, node->line, node->column);
        scope_declare(ctx->scope, sym);
        if (decl_type->data.slot.is_secure) {
            Symbol *tok = symbol_create_token(paired_token, name,
                                              node->line, node->column);
            if (!scope_declare(ctx->scope, tok))
                symbol_destroy(tok);
        }
        scope_register_slot(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (type_is_anchored_resource_handle(decl_type)) {
        bool is_fresh_claim = expr_is_device_slot_claim(init);
        if (!is_fresh_claim && init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error(ctx, node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding; keep the handle in one binding or reacquire it from its owning system");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
    }

    Symbol *sym = symbol_create_variable(name, decl_type,
                                          node->line, node->column);

    if (type_is_constructed_named(decl_type, "DeviceSlot"))
        sym->slot_info.state = SLOT_STATE_CLAIMED;

    /* Set qubit semantic state for QubitSlot variables */
    if (type_is_qubit(decl_type)) {
        if (expr_is_qubit_claim(init)) {
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL && init->type == AST_IDENTIFIER) {
            /* Move: copy source qubit's semantic state */
            Symbol *src = lookup_identifier_symbol(init, ctx);
            if (src != NULL)
                sym->qubit_info.semantic_state = src->qubit_info.semantic_state;
            else
                sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        } else if (init != NULL && init->type == AST_CALL) {
            /* Function returning QubitSlot */
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL
                   && (init->type == AST_CHANNEL_RECV
                       || init->type == AST_AWAIT_EXPR)) {
            /* Transfer from orchestration boundary; precise state is unknown. */
            sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        }
    }

    if (!is_slot_decl)
        scope_declare(ctx->scope, sym);

    return !ctx->has_error;
}

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;
    if (node->data.return_stmt.value != NULL)
        ret_type = type_check_expression(node->data.return_stmt.value, ctx);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (type_is_anchored_resource_handle(ret_type)) {
        semantic_error(ctx, node,
            "Returning anchored resource handles (Slot/SecureSlot/DeviceSlot) is not supported yet; return the inner value or a future/result token instead");
    }

    if (node->data.return_stmt.value != NULL
        && type_is_qubit(ret_type)
        && node->data.return_stmt.value->type == AST_IDENTIFIER) {
        consume_qubit_value(node->data.return_stmt.value, ctx, "returned");
    }

    return !ctx->has_error;
}

bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel = ctx->in_parallel;
    ctx->in_parallel   = true;

    for (size_t i = 0; i < node->data.parallel.task_count; i++) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_statement(node->data.parallel.tasks[i], ctx);
        scope_exit(&ctx->scope);
    }

    ctx->in_parallel = prev_parallel;
    return !ctx->has_error;
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.ability_decl.name;

    /* Register ability as a symbol so roles can reference it */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ABILITY;
    sym->type = TYPE_VOID; /* Abilities don't have a concrete type */
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of ability '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check require fields have valid types */
    for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
        ASTNode *req = node->data.ability_decl.require_fields[i];
        resolve_type_node(req->data.require_field.type, ctx);
    }

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        /* Only type-check methods that have a body */
        if (method->data.func_decl.body != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (method->data.func_decl.return_type != NULL)
                resolve_type_node(method->data.func_decl.return_type, ctx);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                if (method->data.func_decl.params[j]->type != NULL)
                    resolve_type_node(method->data.func_decl.params[j]->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

static bool
any_subject_role_has_ability(ASTNode *program, const char *ability_name);

bool
type_check_role_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.role_decl.name;

    /* Register role as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROLE;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of role '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check for_type exists */
    if (node->data.role_decl.for_type != NULL) {
        Type *bound_type = resolve_type_node(node->data.role_decl.for_type, ctx);
        if (bound_type != NULL
            && node->data.role_decl.for_type->type == AST_TYPE
            && node->data.role_decl.for_type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, node->data.role_decl.for_type->data.type.name);
            if (type_decl == NULL) {
                type_decl = find_actor_decl_by_name(
                    ctx->program_root, node->data.role_decl.for_type->data.type.name);
            }
            if (type_decl != NULL && !decl_is_subject_host(type_decl)) {
                semantic_error(ctx, node->data.role_decl.for_type,
                    "Role '%s' must be bound to a subject or primitive domain; '%s' is not a subject",
                    name,
                    node->data.role_decl.for_type->data.type.name);
            }
        }
    }

    /* Check includes reference existing roles */
    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        const char *role_name = inc->data.include_stmt.role_name;
        Symbol *role_sym = scope_lookup(ctx->scope, role_name);
        if (role_sym == NULL) {
            semantic_warning(ctx, inc,
                "Included role '%s' not found in current scope", role_name);
        }
    }

    /* Check impl ability blocks */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            /* Check that the ability exists */
            if (impl->data.impl_ability.ability_name != NULL) {
                Symbol *ab = scope_lookup(ctx->scope,
                    impl->data.impl_ability.ability_name);
                if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, impl,
                        "Ability '%s' not found in current scope",
                        impl->data.impl_ability.ability_name);
                }
            }

            /* Type-check each method implementation */
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                type_check_func_decl(impl->data.impl_ability.methods[j], ctx);
            }
        } else if (impl->type == AST_OVERRIDE_FUNC) {
            /* Type-check the overridden function */
            if (impl->data.override_func.func_decl != NULL)
                type_check_func_decl(impl->data.override_func.func_decl, ctx);
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_party_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.party_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_PARTY;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        /* Forward-declared in Pass 1 — update kind */
        existing->kind = SYMBOL_PARTY;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of party '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    /* Check role slot ability references */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];

        /* dyn slots require at least one ability for vtable dispatch */
        if (rs->data.role_slot.is_dynamic &&
            rs->data.role_slot.ability_count == 0) {
            semantic_error(ctx, rs,
                "Dynamic role slot '%s' requires at least one ability type",
                rs->data.role_slot.slot_name);
        }

        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab_type = rs->data.role_slot.required_abilities[j];
            if (ab_type != NULL && ab_type->data.type.name != NULL) {
                const char *ability_name = ab_type->data.type.name;
                Symbol *ab = scope_lookup(ctx->scope, ab_type->data.type.name);
                if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, rs,
                        "Ability '%s' not found for role slot '%s'",
                        ab_type->data.type.name,
                        rs->data.role_slot.slot_name);
                } else if (ctx->program_root != NULL
                           && !any_subject_role_has_ability(ctx->program_root,
                               ability_name)) {
                    semantic_error(ctx, rs,
                        "Party role slot '%s' requires ability '%s', but no subject-bound role implements it",
                        rs->data.role_slot.slot_name,
                        ability_name);
                }
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        type_check_func_decl(node->data.party_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_systemic_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.systemic_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_SYSTEMIC;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_SYSTEMIC;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of systemic '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    /* Check party slot references */
    for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
        ASTNode *ps = node->data.systemic_decl.party_slots[i];
        if (ps->data.systemic_slot.party_type != NULL) {
            Symbol *party = scope_lookup(ctx->scope,
                ps->data.systemic_slot.party_type);
            if (party == NULL || party->kind != SYMBOL_PARTY) {
                semantic_warning(ctx, ps,
                    "Party type '%s' not found for slot '%s'",
                    ps->data.systemic_slot.party_type,
                    ps->data.systemic_slot.slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
        ASTNode *shared = node->data.systemic_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
        type_check_func_decl(node->data.systemic_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);

static ASTNode *
find_world_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_before_local(ASTNode *world, const char *state_name, size_t limit)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    if (limit > world->data.world_decl.state_count)
        limit = world->data.world_decl.state_count;

    for (size_t i = 0; i < limit; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl_local(ASTNode *world, SemanticContext *ctx, const char *slot_name)
{
    ASTNode *slot;
    if (world == NULL || world->type != AST_WORLD_DECL
        || ctx == NULL || slot_name == NULL) {
        return NULL;
    }

    slot = find_world_zone_slot_local(world, slot_name);
    if (slot == NULL || slot->data.world_zone.zone_type == NULL)
        return NULL;

    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
        slot->data.world_zone.zone_type);
}

static const char *
resolve_world_plain_zone_input_name(ASTNode *world, const char *input_name)
{
    ASTNode *state;

    if (world == NULL || world->type != AST_WORLD_DECL || input_name == NULL)
        return NULL;

    if (find_world_zone_slot_local(world, input_name) != NULL)
        return input_name;

    state = find_world_state_local(world, input_name);
    if (state != NULL && state->type == AST_WORLD_STATE
        && state->data.world_state.source_kind == WORLD_STATE_SOURCE_ZONE) {
        return state->data.world_state.zone_slot_name;
    }

    return NULL;
}

static ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state_decl_local(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_world_zone_state(ASTNode *world, ASTNode *site, const char *state_name,
                         SemanticContext *ctx, const char *action_name,
                         const char **zone_slot_name_out)
{
    ASTNode *state = find_world_state_local(world, state_name);
    if (state == NULL) {
        ASTNode *zone = find_world_zone_slot_local(world, state_name);
        if (zone != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone->data.world_zone.slot_name;
            return true;
        }
    }
    if (state == NULL) {
        semantic_error(ctx, site,
            "World %s references unknown zone/state '%s'",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE) {
        semantic_error(ctx, site,
            "World %s cannot target derived state '%s'; use the underlying zone slot or a plain 'state name: zone slot' alias",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = state->data.world_state.zone_slot_name;
    return true;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.world_decl.name;
    ASTNode *saved_world = ctx->current_world;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_WORLD;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of world '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    ctx->current_world = node;

    /* Check systemic references */
    for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
        ASTNode *ws = node->data.world_decl.systemics[i];
        if (ws->data.world_systemic.systemic_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope,
                ws->data.world_systemic.systemic_type);
            if (sys == NULL || sys->kind != SYMBOL_SYSTEMIC) {
                semantic_warning(ctx, ws,
                    "Systemic type '%s' not found for slot '%s'",
                    ws->data.world_systemic.systemic_type,
                    ws->data.world_systemic.slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        if (wz->data.world_zone.zone_type != NULL) {
            Symbol *zone = scope_lookup(ctx->scope,
                wz->data.world_zone.zone_type);
            if (zone == NULL || zone->kind != SYMBOL_ZONE) {
                semantic_warning(ctx, wz,
                    "Zone type '%s' not found for slot '%s'",
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *zone_slot_name = state->data.world_state.zone_slot_name;
        ASTNode *zone_decl = NULL;
        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            bool saw_direct_zone_input = false;
            bool saw_state_input = false;
            if (state->data.world_state.input_count == 0) {
                semantic_error(ctx, state,
                    "Composed world state '%s' must reference at least one zone/state input",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (state->data.world_state.state_name != NULL
                    && strcmp(state->data.world_state.state_name, input_name) == 0) {
                    semantic_error(ctx, state,
                        "Composed world state '%s' cannot reference itself",
                        input_name);
                    continue;
                }
                if (find_world_zone_slot_local(node, input_name) != NULL)
                {
                    saw_direct_zone_input = true;
                    continue;
                }
                if (find_world_state_before_local(node, input_name, i) != NULL)
                {
                    saw_state_input = true;
                    continue;
                }
                semantic_error(ctx, state,
                    "Composed world state '%s' references unknown or later world zone/state '%s'",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>",
                    input_name);
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                const char *input_plain_zone;
                if (input_name == NULL)
                    continue;
                input_plain_zone = resolve_world_plain_zone_input_name(node, input_name);
                for (size_t prev_i = 0; prev_i < input_i; prev_i++) {
                    const char *prev_name = state->data.world_state.input_names[prev_i];
                    const char *prev_plain_zone;
                    if (prev_name == NULL)
                        continue;
                    if (strcmp(prev_name, input_name) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' repeats input '%s'",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            input_name);
                        break;
                    }
                    prev_plain_zone = resolve_world_plain_zone_input_name(node, prev_name);
                    if (input_plain_zone != NULL && prev_plain_zone != NULL
                        && strcmp(input_plain_zone, prev_plain_zone) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' redundantly mixes zone slot '%s' with plain alias/input '%s'",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            prev_name,
                            input_name);
                        break;
                    }
                }
                if (find_world_zone_slot_local(node, input_name) != NULL) {
                    semantic_warning(ctx, state,
                        "Composed world state '%s' directly references zone slot '%s'; prefer a plain 'state name: zone %s' alias so command and derived layers stay separate",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        input_name,
                        input_name);
                }
            }
            if (saw_direct_zone_input && saw_state_input) {
                semantic_warning(ctx, state,
                    "Composed world state '%s' mixes direct zone-slot inputs with world-state inputs; prefer composing from world-state aliases only",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
        } else if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, state,
                "World state '%s' references unknown zone slot '%s'",
                state->data.world_state.state_name != NULL
                    ? state->data.world_state.state_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        } else {
            zone_decl = resolve_world_zone_decl_local(node, ctx, zone_slot_name);
        }

        if (zone_decl != NULL && state->data.world_state.detail_name != NULL) {
            const char *detail_name = state->data.world_state.detail_name;
            ASTNode *detail_decl = NULL;

            if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                detail_decl = find_zone_domain_slot(zone_decl, detail_name);
                if (detail_decl == NULL
                    || detail_decl->data.domain_slot.is_subject) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown object/dto projection slot '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                detail_decl = find_zone_layer_slot_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown layer slot '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                detail_decl = find_zone_state_decl_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown zone state '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            }
        }
        for (size_t j = i + 1; j < node->data.world_decl.state_count; j++) {
            ASTNode *other = node->data.world_decl.states[j];
            if (other != NULL
                && other->data.world_state.state_name != NULL
                && state->data.world_state.state_name != NULL
                && strcmp(other->data.world_state.state_name,
                          state->data.world_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of world state '%s'",
                    state->data.world_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *activate = node->data.world_decl.activations[i];
        const char *zone_slot_name = activate->data.world_activate.zone_slot_name;
        if (activate->data.world_activate.state_name != NULL) {
            if (!resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, activate,
                "World activate references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.activate_count; j++) {
            ASTNode *other = node->data.world_decl.activations[j];
            const char *other_zone = other->data.world_activate.zone_slot_name;
            if (other->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' activates zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and deactivates zone '%s'; choose one lifecycle direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *maintain = node->data.world_decl.maintained_zones[j];
            const char *other_zone = maintain->data.world_maintain.zone_slot_name;
            if (maintain->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, maintain,
                    maintain->data.world_maintain.state_name, ctx, "maintain",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and maintains zone '%s'; maintain already implies the continuing direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = node->data.world_decl.deactivations[i];
        const char *zone_slot_name = deactivate->data.world_deactivate.zone_slot_name;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            if (!resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, deactivate,
                "World deactivate references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *other = node->data.world_decl.deactivations[j];
            const char *other_zone = other->data.world_deactivate.zone_slot_name;
            if (other->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' deactivates zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = node->data.world_decl.maintained_zones[i];
        const char *zone_slot_name = maintain->data.world_maintain.zone_slot_name;
        const char *state_name = maintain->data.world_maintain.state_name;
        if (state_name != NULL) {
            if (!resolve_world_zone_state(node, maintain, state_name, ctx, "maintain",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "World maintain references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *other = node->data.world_decl.maintained_zones[j];
            const char *other_zone = other->data.world_maintain.zone_slot_name;
            if (other->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_maintain.state_name, ctx, "maintain", &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' maintains zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and deactivates zone '%s'; choose one lifecycle direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.activate_count; j++) {
            ASTNode *activate = node->data.world_decl.activations[j];
            const char *other_zone = activate->data.world_activate.zone_slot_name;
            if (activate->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and activates zone '%s'; maintain already implies the continuing direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        type_check_func_decl(node->data.world_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_world = saved_world;
    return !ctx->has_error;
}

static bool
type_check_domain_slots(ASTNode **slots,
                        size_t slot_count,
                        SemanticContext *ctx,
                        const char *kind_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot->data.domain_slot.is_subject
            && !type_is_subject_type(slot_type, ctx)) {
            semantic_error(ctx, slot,
                "%s subject slot '%s' requires a subject type",
                kind_name,
                slot->data.domain_slot.slot_name);
        }
    }

    return !ctx->has_error;
}

static bool
type_check_domain_slot_initializers(ASTNode **slots,
                                    size_t slot_count,
                                    SemanticContext *ctx,
                                    const char *kind_name)
{
    if (slots == NULL || ctx == NULL)
        return true;

    scope_enter(&ctx->scope, SCOPE_BLOCK);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Symbol *sym;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type == NULL || slot_type == TYPE_UNKNOWN)
            continue;

        sym = symbol_create_variable(slot->data.domain_slot.slot_name, slot_type,
            slot->line, slot->column);
        if (sym == NULL)
            continue;

        if (!scope_declare(ctx->scope, sym)) {
            semantic_error(ctx, slot,
                "Redeclaration of %s slot '%s'",
                kind_name,
                slot->data.domain_slot.slot_name);
            symbol_destroy(sym);
        }
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Type *init_type;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.initializer == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        init_type = type_check_expression(slot->data.domain_slot.initializer, ctx);
        if (slot_type == NULL || init_type == NULL
            || slot_type == TYPE_UNKNOWN || init_type == TYPE_UNKNOWN) {
            continue;
        }

        require_assignable(init_type, slot_type,
            slot->data.domain_slot.initializer, ctx);
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

static size_t
count_subject_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t subject_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.is_subject) {
            subject_count++;
        }
    }
    return subject_count;
}

static size_t
count_object_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t object_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && !slot->data.domain_slot.is_subject) {
            object_count++;
        }
    }
    return object_count;
}

static ASTNode *
find_nth_subject_domain_slot(ASTNode **slots, size_t slot_count, size_t ordinal)
{
    size_t seen = 0;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject) {
            continue;
        }

        if (seen == ordinal)
            return slot;
        seen++;
    }

    return NULL;
}

static bool
role_decl_has_ability(ASTNode *role, ASTNode *program,
                      const char *ability_name, int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL
        || ability_name == NULL || depth > 16) {
        return false;
    }

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_name != NULL
            && strcmp(impl->data.impl_ability.ability_name, ability_name) == 0) {
            return true;
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc != NULL ? inc->data.include_stmt.role_name : NULL);
        if (role_decl_has_ability(included, program, ability_name, depth + 1))
            return true;
    }

    return false;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, type_name) != 0) {
            continue;
        }
        if (role_decl_has_ability(stmt, program, ability_name, 0))
            return true;
    }

    return false;
}

static bool
any_subject_role_has_ability(ASTNode *program, const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        if (role_decl_has_ability(stmt, program, ability_name, 0))
            return true;
    }

    return false;
}

static ASTNode *
find_domain_slot_local(ASTNode **slots, size_t slot_count, const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    return find_domain_slot_local(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, slot_name);
}

static ASTNode *
find_domain_decl_by_name(ASTNode *program,
                         ASTNodeType decl_type,
                         const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != decl_type)
            continue;

        const char *decl_name = NULL;
        if (decl_type == AST_RELATION_DECL)
            decl_name = stmt->data.relation_decl.name;
        else if (decl_type == AST_EFFECT_DECL)
            decl_name = stmt->data.effect_decl.name;
        else if (decl_type == AST_ZONE_DECL)
            decl_name = stmt->data.zone_decl.name;

        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
find_zone_effect_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_relation_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_authority(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        if (authority != NULL
            && authority->type == AST_ZONE_AUTHORITY
            && authority->data.zone_authority.subject_slot_name != NULL
            && strcmp(authority->data.zone_authority.subject_slot_name, slot_name) == 0) {
            return authority;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_zone_effect_state(ASTNode *zone,
                          ASTNode *site,
                          const char *state_name,
                          SemanticContext *ctx,
                          const char *action_name,
                          const char **effect_slot_name,
                          const char **target_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (effect_slot_name != NULL)
        *effect_slot_name = NULL;
    if (target_slot_name != NULL)
        *target_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is a relation state and cannot be used as an effect state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (effect_slot_name != NULL)
        *effect_slot_name = state->data.zone_state.layer_slot_name;
    if (target_slot_name != NULL)
        *target_slot_name = state->data.zone_state.left_or_target_slot_name;
    return true;
}

static bool
resolve_zone_relation_state(ASTNode *zone,
                            ASTNode *site,
                            const char *state_name,
                            SemanticContext *ctx,
                            const char *action_name,
                            const char **relation_slot_name,
                            const char **left_slot_name,
                            const char **right_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (relation_slot_name != NULL)
        *relation_slot_name = NULL;
    if (left_slot_name != NULL)
        *left_slot_name = NULL;
    if (right_slot_name != NULL)
        *right_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (!state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is an effect state and cannot be used as a relation state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (relation_slot_name != NULL)
        *relation_slot_name = state->data.zone_state.layer_slot_name;
    if (left_slot_name != NULL)
        *left_slot_name = state->data.zone_state.left_or_target_slot_name;
    if (right_slot_name != NULL)
        *right_slot_name = state->data.zone_state.right_slot_name;
    return true;
}

static bool
type_check_zone_actor_authority(ASTNode *zone,
                                ASTNode *site,
                                const char *actor_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *actor_slot;
    ASTNode *authority;

    if (zone == NULL || ctx == NULL || actor_slot_name == NULL)
        return false;

    actor_slot = find_zone_domain_slot(zone, actor_slot_name);
    if (actor_slot == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown authority subject slot '%s'",
            action_name, actor_slot_name);
        return true;
    }

    if (!actor_slot->data.domain_slot.is_subject) {
        semantic_error(ctx, site,
            "Zone %s authority '%s' must be a subject slot",
            action_name, actor_slot_name);
        return true;
    }

    authority = find_zone_authority(zone, actor_slot_name);
    if (zone->data.zone_decl.authority_count > 0 && authority == NULL) {
        semantic_error(ctx, site,
            "Zone %s actor '%s' is not declared in zone authority set",
            action_name, actor_slot_name);
        return true;
    }

    return true;
}

static ASTNode *
find_named_class_decl_local(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
type_check_projection_contract(ASTNode **slots,
                               size_t slot_count,
                               const char *owner_label,
                               ASTNode *site,
                               const char *object_slot_name,
                               const char *source_slot_name,
                               SemanticContext *ctx,
                               const char *action_name)
{
    ASTNode *object_slot;
    ASTNode *source_slot;
    Type *target_type;
    Type *source_type;
    ASTNode *target_decl;
    ASTNode *source_decl;

    object_slot = find_domain_slot_local(slots, slot_count, object_slot_name);
    source_slot = find_domain_slot_local(slots, slot_count, source_slot_name);
    if (object_slot == NULL || source_slot == NULL || ctx == NULL)
        return false;

    if (object_slot->data.domain_slot.is_subject) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' must be an object/dto slot",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>");
        return true;
    }
    if (site != NULL && site->type == AST_ZONE_REFRESH
        && site->data.zone_refresh.requires_dto
        && !object_slot->data.domain_slot.is_dto) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' must be a dto slot",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>");
        return true;
    }
    if (!source_slot->data.domain_slot.is_subject) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' must be a subject slot",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>");
        return true;
    }

    target_type = resolve_type_node(object_slot->data.domain_slot.type, ctx);
    source_type = resolve_type_node(source_slot->data.domain_slot.type, ctx);
    if (target_type == NULL || source_type == NULL
        || target_type == TYPE_UNKNOWN || source_type == TYPE_UNKNOWN) {
        return true;
    }

    target_decl = find_named_class_decl_local(ctx->program_root, target_type->name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' must use an object/dto projection type, got '%s'",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            target_type->name != NULL ? target_type->name : "<unknown>");
        return true;
    }
    if (site != NULL && site->type == AST_ZONE_REFRESH) {
        NominalDeclKind expected_kind =
            site->data.zone_refresh.requires_dto ? NOMINAL_DECL_DTO : NOMINAL_DECL_OBJECT;
        const char *expected_label =
            site->data.zone_refresh.requires_dto ? "dto" : "object";

        if (target_decl->data.class_decl.nominal_kind != expected_kind) {
            semantic_error(ctx, site,
                "%s %s target slot '%s' must use a %s declaration, got '%s'",
                owner_label, action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                expected_label,
                target_type->name != NULL ? target_type->name : "<unknown>");
            return true;
        }
    }

    if (!type_is_subject_type(source_type, ctx)) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' must use a subject type, got '%s'",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type->name != NULL ? source_type->name : "<unknown>");
        return true;
    }

    source_decl = find_subject_host_decl_by_name(ctx->program_root, source_type->name);
    if (!decl_is_subject_host(source_decl)) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' must reference a subject declaration",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>");
        return true;
    }

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        ClassField *source_field;
        Type *target_field_type;
        Type *source_field_type;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_field = NULL;
        for (size_t j = 0; j < subject_host_field_count(source_decl); j++) {
            ClassField *candidate = subject_host_field_at(source_decl, j);
            if (candidate != NULL && candidate->name != NULL
                && strcmp(candidate->name, target_field->name) == 0) {
                source_field = candidate;
                break;
            }
        }
        if (source_field == NULL || source_field->type == NULL) {
            semantic_error(ctx, site,
                "%s %s target field '%s' is missing from source subject slot '%s'",
                owner_label, action_name,
                target_field->name,
                source_slot_name != NULL ? source_slot_name : "<unknown>");
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        source_field_type = resolve_type_node(source_field->type, ctx);
        require_assignable(source_field_type, target_field_type, site, ctx);
    }

    return true;
}

static bool
type_check_zone_projection_contract(ASTNode *zone,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name)
{
    if (zone == NULL)
        return false;
    return type_check_projection_contract(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, "Zone", site,
        object_slot_name, source_slot_name, ctx, action_name);
}

static bool
type_check_zone_effect_contract(ASTNode *zone,
                                ASTNode *apply_like,
                                const char *effect_slot_name,
                                const char *target_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *effect_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    ASTNode *decl_target;
    Type *target_type;
    Type *decl_target_type;
    size_t subject_count;

    effect_slot = find_zone_effect_slot(zone, effect_slot_name);
    target_slot = find_zone_domain_slot(zone, target_slot_name);
    if (effect_slot == NULL || target_slot == NULL || ctx == NULL)
        return false;

    effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
        effect_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return false;

    subject_count = count_subject_domain_slots(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count);
    if (subject_count != 1) {
        semantic_error(ctx, apply_like,
            "Zone %s requires effect '%s' to declare exactly one subject target, found %zu",
            action_name,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            subject_count);
        return true;
    }

    decl_target = find_nth_subject_domain_slot(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count, 0);
    if (decl_target == NULL)
        return false;

    target_type = resolve_type_node(target_slot->data.domain_slot.type, ctx);
    decl_target_type = resolve_type_node(decl_target->data.domain_slot.type, ctx);
    if (!type_is_assignable(target_type, decl_target_type)) {
        semantic_error(ctx, apply_like,
            "Zone %s target slot '%s' has type '%s' but effect '%s' expects subject type '%s'",
            action_name,
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>");
    }

    return true;
}

static bool
type_check_zone_relation_contract(ASTNode *zone,
                                  ASTNode *link_like,
                                  const char *relation_slot_name,
                                  const char *left_slot_name,
                                  const char *right_slot_name,
                                  SemanticContext *ctx,
                                  const char *action_name)
{
    ASTNode *relation_slot;
    ASTNode *relation_decl;
    ASTNode *left_slot;
    ASTNode *right_slot;
    ASTNode *decl_left;
    ASTNode *decl_right;
    Type *left_type;
    Type *right_type;
    Type *decl_left_type;
    Type *decl_right_type;
    size_t subject_count;

    relation_slot = find_zone_relation_slot(zone, relation_slot_name);
    left_slot = find_zone_domain_slot(zone, left_slot_name);
    right_slot = find_zone_domain_slot(zone, right_slot_name);
    if (relation_slot == NULL || left_slot == NULL || right_slot == NULL || ctx == NULL)
        return false;

    relation_decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
        relation_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return false;

    subject_count = count_subject_domain_slots(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count);
    if (subject_count != 2) {
        semantic_error(ctx, link_like,
            "Zone %s requires relation '%s' to declare exactly two subject endpoints, found %zu",
            action_name,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            subject_count);
        return true;
    }

    decl_left = find_nth_subject_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count, 0);
    decl_right = find_nth_subject_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count, 1);
    if (decl_left == NULL || decl_right == NULL)
        return false;

    left_type = resolve_type_node(left_slot->data.domain_slot.type, ctx);
    right_type = resolve_type_node(right_slot->data.domain_slot.type, ctx);
    decl_left_type = resolve_type_node(decl_left->data.domain_slot.type, ctx);
    decl_right_type = resolve_type_node(decl_right->data.domain_slot.type, ctx);

    if (!type_is_assignable(left_type, decl_left_type)) {
        semantic_error(ctx, link_like,
            "Zone %s left slot '%s' has type '%s' but relation '%s' expects left subject type '%s'",
            action_name,
            left_slot_name != NULL ? left_slot_name : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>");
    }

    if (!type_is_assignable(right_type, decl_right_type)) {
        semantic_error(ctx, link_like,
            "Zone %s right slot '%s' has type '%s' but relation '%s' expects right subject type '%s'",
            action_name,
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>");
    }

    return true;
}

static bool
type_check_overlay_decl_common(ASTNode *node,
                               SemanticContext *ctx,
                               const char *name,
                               SymbolKind kind,
                               ASTNode **shared_fields,
                               size_t shared_count,
                               ASTNode **methods,
                               size_t method_count,
                               const char *kind_name)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = kind;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = kind;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of %s '%s'", kind_name, name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods[i], ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_relation_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_relation = ctx->current_relation;
    ctx->current_relation = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.relation_decl.name,
        SYMBOL_RELATION,
        node->data.relation_decl.shared_fields,
        node->data.relation_decl.shared_count,
        node->data.relation_decl.methods,
        node->data.relation_decl.method_count,
        "relation");

    ok = type_check_domain_slots(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "Relation") && ok;
    ok = type_check_domain_slot_initializers(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "relation") && ok;
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.relation_decl.slots,
            node->data.relation_decl.slot_count, "Relation", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            refresh->data.zone_refresh.requires_dto ? "publish" : "refresh") && ok;
    }
    size_t subject_count = count_subject_domain_slots(
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count);
    if (subject_count == 0) {
        semantic_warning(ctx, node,
            "Relation '%s' should declare at least one subject endpoint; use 'for name: Type' or 'subject slot ...'",
            node->data.relation_decl.name);
    } else if (subject_count > 2) {
        semantic_warning(ctx, node,
            "Relation '%s' currently models a small edge; more than two subject endpoints may be better expressed as party or zone",
            node->data.relation_decl.name);
    }
    ctx->current_relation = saved_relation;
    return ok && !ctx->has_error;
}

bool
type_check_effect_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_effect = ctx->current_effect;
    ctx->current_effect = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.effect_decl.name,
        SYMBOL_EFFECT,
        node->data.effect_decl.shared_fields,
        node->data.effect_decl.shared_count,
        node->data.effect_decl.methods,
        node->data.effect_decl.method_count,
        "effect");

    ok = type_check_domain_slots(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "Effect") && ok;
    ok = type_check_domain_slot_initializers(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "effect") && ok;
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.effect_decl.slots,
            node->data.effect_decl.slot_count, "Effect", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            refresh->data.zone_refresh.requires_dto ? "publish" : "refresh") && ok;
    }
    size_t subject_count = count_subject_domain_slots(
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count);
    if (subject_count == 0) {
        semantic_warning(ctx, node,
            "Effect '%s' should declare at least one subject target; use 'for name: Type' or 'subject slot ...'",
            node->data.effect_decl.name);
    } else if (subject_count > 1) {
        semantic_warning(ctx, node,
            "Effect '%s' currently expects a small target surface; multiple subject targets may be better expressed via relation or zone",
            node->data.effect_decl.name);
    }
    ctx->current_effect = saved_effect;
    return ok && !ctx->has_error;
}

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    size_t subject_count;
    size_t object_count;
    size_t mutation_rule_count;
    ctx->current_zone = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.zone_decl.name,
        SYMBOL_ZONE,
        node->data.zone_decl.shared_fields,
        node->data.zone_decl.shared_count,
        node->data.zone_decl.methods,
        node->data.zone_decl.method_count,
        "zone");

    ok = type_check_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "zone") && ok;
    subject_count = count_subject_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    object_count = count_object_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);

    if (subject_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' should declare at least one subject slot",
            node->data.zone_decl.name);
    } else if (subject_count > 4) {
        semantic_warning(ctx, node,
            "Zone '%s' declares %zu subject slots; prefer keeping active subjects to 4 or fewer and model supporting state as objects",
            node->data.zone_decl.name,
            subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has multiple subject slots but no object slots; consider modeling passive support state as objects",
            node->data.zone_decl.name);
    }

    mutation_rule_count = node->data.zone_decl.apply_count
        + node->data.zone_decl.link_count
        + node->data.zone_decl.detach_count
        + node->data.zone_decl.unlink_count
        + node->data.zone_decl.refresh_count
        + node->data.zone_decl.maintained_effect_count
        + node->data.zone_decl.maintained_relation_count
        + node->data.zone_decl.maintained_state_count;

    for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
        ASTNode *authority = node->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(node, authority->data.zone_authority.subject_slot_name);
        if (slot == NULL) {
            semantic_error(ctx, authority,
                "Zone authority references unknown subject slot '%s'",
                authority->data.zone_authority.subject_slot_name);
            continue;
        }
        if (!slot->data.domain_slot.is_subject) {
            semantic_error(ctx, authority,
                "Zone authority '%s' must reference a subject slot",
                authority->data.zone_authority.subject_slot_name);
        }
        slot_type = slot != NULL ? resolve_type_node(slot->data.domain_slot.type, ctx) : NULL;
        for (size_t j = 0; j < authority->data.zone_authority.ability_count; j++) {
            const char *ability_name = authority->data.zone_authority.required_abilities[j];
            Symbol *ab = ability_name != NULL ? scope_lookup(ctx->scope, ability_name) : NULL;
            if (ability_name == NULL)
                continue;
            if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                semantic_error(ctx, authority,
                    "Zone authority '%s' requires unknown ability '%s'",
                    authority->data.zone_authority.subject_slot_name,
                    ability_name);
                continue;
            }
            if (slot_type == NULL || slot_type == TYPE_UNKNOWN
                || slot_type->name == NULL) {
                continue;
            }
            if (!subject_type_has_ability(ctx->program_root, slot_type->name, ability_name)) {
                semantic_error(ctx, authority,
                    "Zone authority '%s' requires ability '%s', but subject type '%s' has no matching role impl",
                    authority->data.zone_authority.subject_slot_name,
                    ability_name,
                    slot_type->name);
            }
        }
        for (size_t j = i + 1; j < node->data.zone_decl.authority_count; j++) {
            ASTNode *other = node->data.zone_decl.authorities[j];
            if (other != NULL
                && other->data.zone_authority.subject_slot_name != NULL
                && strcmp(authority->data.zone_authority.subject_slot_name,
                          other->data.zone_authority.subject_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' declares authority '%s' more than once",
                    node->data.zone_decl.name,
                    authority->data.zone_authority.subject_slot_name);
            }
        }
    }

    if (mutation_rule_count > 0 && node->data.zone_decl.authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = node->data.zone_decl.layer_slots[i];
        const char *type_name = layer_slot->data.zone_layer_slot.layer_type;
        Symbol *sym = type_name != NULL ? scope_lookup(ctx->scope, type_name) : NULL;
        SymbolKind expected = layer_slot->data.zone_layer_slot.is_relation
            ? SYMBOL_RELATION
            : SYMBOL_EFFECT;
        const char *kind_name = layer_slot->data.zone_layer_slot.is_relation
            ? "relation"
            : "effect";
        if (sym == NULL || sym->kind != expected) {
            semantic_warning(ctx, layer_slot,
                "%s type '%s' not found for slot '%s'",
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                layer_slot->data.zone_layer_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        const char *effect_slot_name = apply->data.zone_apply.effect_slot_name;
        const char *target_slot_name = apply->data.zone_apply.target_slot_name;
        const char *state_name = apply->data.zone_apply.state_name;
        const char *actor_slot_name = apply->data.zone_apply.actor_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx, "apply",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, apply,
                "Zone apply should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, apply, actor_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        const char *relation_slot_name = link->data.zone_link.relation_slot_name;
        const char *left_slot_name = link->data.zone_link.left_slot_name;
        const char *right_slot_name = link->data.zone_link.right_slot_name;
        const char *state_name = link->data.zone_link.state_name;
        const char *actor_slot_name = link->data.zone_link.actor_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx, "link",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, link,
                "Zone link should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, link, actor_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        const char *effect_slot_name = detach->data.zone_detach.effect_slot_name;
        const char *target_slot_name = detach->data.zone_detach.target_slot_name;
        const char *state_name = detach->data.zone_detach.state_name;
        const char *actor_slot_name = detach->data.zone_detach.actor_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx, "detach",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, detach,
                "Zone detach should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, detach, actor_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        const char *relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
        const char *left_slot_name = unlink->data.zone_unlink.left_slot_name;
        const char *right_slot_name = unlink->data.zone_unlink.right_slot_name;
        const char *state_name = unlink->data.zone_unlink.state_name;
        const char *actor_slot_name = unlink->data.zone_unlink.actor_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx, "unlink",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, unlink,
                "Zone unlink should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, unlink, actor_slot_name, ctx, "unlink");
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        const char *object_slot_name = refresh->data.zone_refresh.object_slot_name;
        const char *source_slot_name = refresh->data.zone_refresh.source_slot_name;
        const char *actor_slot_name = refresh->data.zone_refresh.actor_slot_name;
        const char *action_name = refresh->data.zone_refresh.requires_dto ? "publish" : "refresh";
        if (find_zone_domain_slot(node, object_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown target slot '%s'",
                action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, source_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown source slot '%s'",
                action_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>");
        }
        type_check_zone_projection_contract(node, refresh,
            object_slot_name, source_slot_name, ctx, action_name);
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, refresh,
                "Zone %s should specify 'by <subjectSlot>' when authority is declared",
                action_name);
        }
        type_check_zone_actor_authority(node, refresh, actor_slot_name, ctx, action_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        const char *effect_slot_name = maintain->data.zone_maintain_effect.effect_slot_name;
        const char *target_slot_name = maintain->data.zone_maintain_effect.target_slot_name;
        const char *actor_slot_name = maintain->data.zone_maintain_effect.actor_slot_name;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, maintain, actor_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_effect_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_effects[j];
            if (strcmp(effect_slot_name, other->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(target_slot_name, other->data.zone_maintain_effect.target_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
            ASTNode *detach = node->data.zone_decl.detaches[j];
            const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
            const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
            if (detach->data.zone_detach.state_name != NULL) {
                resolve_zone_effect_state(node, detach,
                    detach->data.zone_detach.state_name, ctx, "detach",
                    &detach_effect_slot_name, &detach_target_slot_name);
            }
            if (detach_effect_slot_name != NULL
                && detach_target_slot_name != NULL
                && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'; choose one lifecycle direction",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        const char *relation_slot_name = maintain->data.zone_maintain_relation.relation_slot_name;
        const char *left_slot_name = maintain->data.zone_maintain_relation.left_slot_name;
        const char *right_slot_name = maintain->data.zone_maintain_relation.right_slot_name;
        const char *actor_slot_name = maintain->data.zone_maintain_relation.actor_slot_name;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, maintain, actor_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_relation_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_relations[j];
            if (strcmp(relation_slot_name, other->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(left_slot_name, other->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(right_slot_name, other->data.zone_maintain_relation.right_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
            ASTNode *unlink = node->data.zone_decl.unlinks[j];
            const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
            const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
            const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
            if (unlink->data.zone_unlink.state_name != NULL) {
                resolve_zone_relation_state(node, unlink,
                    unlink->data.zone_unlink.state_name, ctx, "unlink",
                    &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
            }
            if (unlink_relation_slot_name != NULL
                && unlink_left_slot_name != NULL
                && unlink_right_slot_name != NULL
                && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                && strcmp(left_slot_name, unlink_left_slot_name) == 0
                && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'; choose one lifecycle direction",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        ASTNode *state;
        const char *state_name = maintain->data.zone_maintain_state.state_name;
        const char *actor_slot_name = maintain->data.zone_maintain_state.actor_slot_name;
        state = find_zone_state(node, state_name);
        if (state == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown state '%s'",
                state_name != NULL ? state_name : "<unknown>");
        } else if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
                ASTNode *unlink = node->data.zone_decl.unlinks[j];
                const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
                const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
                const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
                if (unlink->data.zone_unlink.state_name != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        unlink->data.zone_unlink.state_name, ctx, "unlink",
                        &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
                }
                if (unlink_relation_slot_name != NULL
                    && unlink_left_slot_name != NULL
                    && unlink_right_slot_name != NULL
                    && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                    && strcmp(left_slot_name, unlink_left_slot_name) == 0
                    && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and unlinks state '%s'; choose one lifecycle direction",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
                ASTNode *detach = node->data.zone_decl.detaches[j];
                const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
                const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
                if (detach->data.zone_detach.state_name != NULL) {
                    resolve_zone_effect_state(node, detach,
                        detach->data.zone_detach.state_name, ctx, "detach",
                        &detach_effect_slot_name, &detach_target_slot_name);
                }
                if (detach_effect_slot_name != NULL
                    && detach_target_slot_name != NULL
                    && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                    && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and detaches state '%s'; choose one lifecycle direction",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
        }
        if (node->data.zone_decl.authority_count > 0 && actor_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_actor_authority(node, maintain, actor_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *state_name = state->data.zone_state.state_name;
        if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown relation slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, left_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown left slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, right_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown right slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown effect slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, target_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown target slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < node->data.zone_decl.state_count; j++) {
            ASTNode *other = node->data.zone_decl.states[j];
            if (state_name != NULL
                && other != NULL
                && other->data.zone_state.state_name != NULL
                && strcmp(state_name, other->data.zone_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of zone state '%s'",
                    state_name);
            }
        }
    }

    ctx->current_zone = saved_zone;
    return ok && !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Async system type checkers
 * ----------------------------------------------------------------- */

bool
type_check_actor_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.actor_decl.name;
    Type *actor_type;

    if (!node->data.actor_decl.from_subject_profile_surface) {
        semantic_warning(ctx, node,
            "Standalone actor syntax is transitional; prefer 'subject %s actor { ... }'",
            name != NULL ? name : "<Actor>");
    }

    /* Register actor as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ACTOR;
    actor_type = calloc(1, sizeof(Type));
    if (actor_type != NULL) {
        actor_type->kind = TYPE_KIND_CLASS;
        actor_type->nominal_flavor = TYPE_NOMINAL_SUBJECT;
        actor_type->name = pergyra_strdup(name);
    }
    sym->type = actor_type != NULL ? actor_type : TYPE_UNKNOWN;
    sym->decl_line = node->line;
    sym->decl_col  = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_ACTOR
        && existing->decl_line == node->line
        && existing->decl_col == node->column) {
        if (existing->type != NULL && existing->type != TYPE_UNKNOWN) {
            actor_type = existing->type;
        } else {
            existing->type = sym->type;
        }
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node,
            "Duplicate actor declaration '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    /* Check fields */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
        ClassField *f = node->data.actor_decl.fields[i];
        if (f && f->type) {
            Type *ft = resolve_type_node(f->type, ctx);
            if (ft && f->name) {
                Symbol *fsym = symbol_create_variable(f->name, ft,
                                                       node->line, node->column);
                scope_declare(ctx->scope, fsym);
            }
        }
    }

    /* Check methods (implicitly async) */
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;
    for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
        ASTNode *method = node->data.actor_decl.methods[i];
        if (method) {
            scope_enter(&ctx->scope, SCOPE_FUNCTION);
            /* Register self + params */
            Symbol *self_sym = symbol_create_variable("self",
                sym->type != NULL ? sym->type : TYPE_UNKNOWN,
                node->line, node->column);
            scope_declare(ctx->scope, self_sym);
            for (size_t k = 0; k < method->data.async_func_decl.param_count; k++) {
                FuncParam *p = method->data.async_func_decl.params[k];
                if (p == NULL || (p->type == NULL && strcmp(p->name, "self") == 0))
                    continue;
                Type *pt = (p->type != NULL) ? resolve_type_node(p->type, ctx) : TYPE_UNKNOWN;
                Symbol *ps = symbol_create_variable(p->name, pt,
                                                      node->line, node->column);
                scope_declare(ctx->scope, ps);
            }
            if (method->data.async_func_decl.body)
                type_check_block(method->data.async_func_decl.body, ctx);
            scope_exit(&ctx->scope);
        }
    }
    ctx->in_async_func = saved_async;
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_async_block(ASTNode *node, SemanticContext *ctx)
{
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        type_check_statement(node->data.async_block.statements[i], ctx);
    }

    ctx->in_async_func = saved_async;
    return !ctx->has_error;
}

bool
type_check_select_stmt(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        if (c != NULL) {
            bool valid_case = false;
            if (c->type == AST_BLOCK && c->data.block.count > 0) {
                ASTNode *first = c->data.block.statements[0];
                ASTNode *recv_expr = NULL;
                const char *bind_name = NULL;

                if (first != NULL && first->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    recv_expr = first;
                } else if (first != NULL && first->type == AST_ASSIGNMENT
                           && first->data.assignment.target != NULL
                           && first->data.assignment.target->type == AST_IDENTIFIER
                           && first->data.assignment.value != NULL
                           && first->data.assignment.value->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    bind_name = first->data.assignment.target->data.identifier.name;
                    recv_expr = first->data.assignment.value;
                }

                if (!valid_case) {
                    semantic_error(ctx, c,
                        "select case must begin with a channel receive pattern");
                    type_check_statement(c, ctx);
                    continue;
                }

                scope_enter(&ctx->scope, SCOPE_BLOCK);
                if (recv_expr != NULL) {
                    Type *recv_type = type_check_expression(recv_expr, ctx);
                    if (bind_name != NULL && recv_type != NULL) {
                        Symbol *binding = symbol_create_variable(
                            bind_name, recv_type, first->line, first->column);
                        scope_declare(ctx->scope, binding);
                    }
                }
                for (size_t j = 1; j < c->data.block.count; j++)
                    type_check_statement(c->data.block.statements[j], ctx);
                scope_exit(&ctx->scope);
            } else {
                semantic_error(ctx, c,
                    "select case must begin with a channel receive pattern");
                type_check_statement(c, ctx);
            }
        }
    }

    if (node->data.select_stmt.default_case)
        type_check_statement(node->data.select_stmt.default_case, ctx);

    return !ctx->has_error;
}

Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Type-check the spawned function/expression */
    Type *inner = type_check_expression(expr->data.spawn_expr.function, ctx);
    Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

Type *
type_check_channel_send(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Check channel and value types */
    Type *channel_type = type_check_expression(expr->data.channel_send.channel, ctx);
    Type *value_type = type_check_expression(expr->data.channel_send.value, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_send.channel,
            "Channel send requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_VOID;
    }

    Type *element_type = channel_type->data.constructed.args[0];

    if (type_is_anchored_resource_handle(element_type)
        || type_is_anchored_resource_handle(value_type)) {
        semantic_error(ctx, expr->data.channel_send.value,
            "Channels cannot transport anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; send the inner value or keep the handle local");
        return TYPE_VOID;
    }

    if (type_is_movable_resource_handle(element_type)
        || type_is_movable_resource_handle(value_type)) {
        if (!type_is_movable_resource_handle(element_type)
            || !type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send movable-resource mismatch: expected '%s', got '%s'",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type));
            return TYPE_VOID;
        }
        if (expr->data.channel_send.value->type != AST_IDENTIFIER) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Movable resource channel sends must transfer from a named variable; bind the value first, then send that variable");
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    require_assignable(value_type, element_type, expr->data.channel_send.value, ctx);
    return TYPE_VOID;
}

Type *
type_check_channel_recv(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *channel_type = type_check_expression(expr->data.channel_recv.channel, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channel recv requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    if (type_is_anchored_resource_handle(channel_type->data.constructed.args[0])) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channels cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; receive a plain value instead");
        return TYPE_UNKNOWN;
    }

    return channel_type->data.constructed.args[0];
}

bool
type_check_statement(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_LET_DECL:
        return type_check_let_decl(node, ctx);
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr; — destructure struct/array fields by position */
        ASTNode *init = node->data.let_destructure.initializer;
        Type *init_type = init != NULL ? type_check_expression(init, ctx) : TYPE_UNKNOWN;
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            Type *elem_type = TYPE_UNKNOWN;
            /* Array destructuring: element type from Array<T> */
            if (type_is_constructed_named(init_type, "Array")
                || type_is_constructed_named(init_type, "Slice"))
                elem_type = type_get_constructed_arg(init_type, 0);
            /* CLASS/struct: field type by position (future — for now use UNKNOWN) */
            Symbol *s = symbol_create_variable(
                node->data.let_destructure.names[i], elem_type,
                node->line, node->column);
            scope_declare(ctx->scope, s);
        }
        return true;
    }
    case AST_FUNC_DECL:
        return type_check_func_decl(node, ctx);
    case AST_CLASS_DECL:
        return type_check_class_decl(node, ctx);
    case AST_EXTERN_BLOCK:
        return type_check_extern_block(node, ctx);
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_WHILE_LOOP:
        return type_check_while_loop(node, ctx);
    case AST_MATCH_STMT:
        return type_check_match_stmt(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return false;
        }
        return true;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return false;
        }
        return true;
    case AST_ENUM_DECL:
        return true;
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_ABILITY_DECL:
        return type_check_ability_decl(node, ctx);
    case AST_ROLE_DECL:
        return type_check_role_decl(node, ctx);
    case AST_PARTY_DECL:
        return type_check_party_decl(node, ctx);
    case AST_SYSTEMIC_DECL:
        return type_check_systemic_decl(node, ctx);
    case AST_WORLD_DECL:
        return type_check_world_decl(node, ctx);
    case AST_RELATION_DECL:
        return type_check_relation_decl(node, ctx);
    case AST_EFFECT_DECL:
        return type_check_effect_decl(node, ctx);
    case AST_ZONE_DECL:
        return type_check_zone_decl(node, ctx);
    case AST_ACTOR_DECL:
        return type_check_actor_decl(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_block(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_IMPORT_DECL:
        /* Already resolved by driver — skip */
        return true;
    case AST_UNSAFE_BLOCK:
        /* Type-check body normally; safety constraints relaxed at codegen */
        if (node->data.unsafe_block.body != NULL)
            type_check_block(node->data.unsafe_block.body, ctx);
        return !ctx->has_error;
    case AST_DEFER_STMT:
        /* Type-check deferred body */
        if (node->data.defer_stmt.body != NULL)
            type_check_block(node->data.defer_stmt.body, ctx);
        return !ctx->has_error;
    case AST_BIND_STMT:
        /* bind party.slot = Role; — validated at codegen level */
        return true;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}

bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.func_decl.name;
    uint32_t prev_effects = ctx->current_function_effects;
    bool prev_tracking = ctx->tracking_function_effects;
    bool prev_async = ctx->in_async_func;
    bool in_class_scope = (ctx->scope != NULL && ctx->scope->kind == SCOPE_CLASS);
    bool has_effect_contract = false;
    uint32_t declared_effects =
        declared_effects_from_function_node(node, ctx, &has_effect_contract);

    /* If the function has generic parameters (<T, U, ...>),
     * register them as opaque types in a temporary scope so that
     * resolve_type_node("T") succeeds for params and return type. */
    bool has_generics = (node->data.func_decl.generic_params != NULL
                         && node->data.func_decl.generic_params->count > 0);
    if (has_generics) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    /* Build parameter types for the function type */
    size_t   param_count = node->data.func_decl.param_count;
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL) {
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
    }

    Type *return_type = TYPE_VOID;
    if (node->data.func_decl.return_type != NULL)
        return_type = resolve_type_node(node->data.func_decl.return_type, ctx);
    if (type_is_anchored_resource_handle(return_type)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Anchored resource handle return types (Slot/SecureSlot/DeviceSlot) are not supported yet");
    }
    if (type_is_class_object_type(return_type, ctx)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Returning subjects by value is not supported yet; return a struct/class value, keep the subject local, or use Box<T>/another handle layer explicitly");
    }

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = node->data.func_decl.params[i];
        /* Implicit 'self' type: if a parameter named "self" has no
         * type annotation and we're inside a class scope, infer the
         * enclosing class type. */
        if (param->type == NULL && param->name != NULL
            && strcmp(param->name, "self") == 0
            && ctx->scope != NULL && ctx->scope->kind == SCOPE_CLASS) {
            /* Walk parent scope to find the class symbol */
            Scope *parent = ctx->scope->parent;
            if (parent != NULL) {
                for (size_t s = parent->symbol_count; s > 0; s--) {
                    Symbol *cs = parent->symbols[s - 1];
                    if (cs != NULL && cs->kind == SYMBOL_CLASS) {
                        param_types[i] = cs->type;
                        break;
                    }
                }
            }
            if (param_types[i] == NULL)
                param_types[i] = TYPE_UNKNOWN;
        } else {
            param_types[i] = resolve_type_node(param->type, ctx);
        }
        if (type_is_anchored_resource_handle(param_types[i])) {
            bool allowed_subject_slot = type_is_subject_host_slot_handle(param_types[i], ctx);
            if (!allowed_subject_slot) {
                semantic_error(ctx, node,
                    "Anchored resource handle parameters are only supported for Slot<subject-host>/SecureSlot<subject-host> with explicit own/ref; other anchored handles remain local-only");
            } else if (param->mode == PARAM_MODE_DEFAULT) {
                semantic_error(ctx, node,
                    "Slot<subject-host> parameters require 'own' or 'ref'; use 'func F(own s: Slot<T>)' or 'func F(ref s: Slot<T>)'");
            }
        }
        if (type_is_class_object_type(param_types[i], ctx)) {
            bool is_implicit_self = (param->name != NULL
                && strcmp(param->name, "self") == 0
                && in_class_scope);
            if (!is_implicit_self) {
                semantic_error(ctx, node,
                    "Subject parameters are not supported as plain value parameters yet; keep subject interaction on methods/self, or pass an explicit Box<T>/handle once the storage model is fixed");
            }
        }
    }

    Type *func_type = type_create_function(param_types, param_count,
                                            return_type);
    free(param_types);

    Symbol *func_sym = symbol_create_function(name, func_type,
                                               node->line, node->column);
    /* If a forward-declaration placeholder exists from Pass 1,
       update its type instead of re-declaring. */
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION) {
        existing->type = func_type;
        symbol_destroy(func_sym);
    } else if (!scope_declare(ctx->scope, func_sym)) {
        semantic_error(ctx, node, "Redeclaration of function '%s'", name);
        symbol_destroy(func_sym);
        return false;
    }

    /* Close the temporary generic-params scope (if opened) before
     * entering the real function scope — the function scope will
     * re-register the generic params so they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);

    /* Re-register generic type params inside the function scope */
    if (has_generics) {
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *prev_return  = ctx->current_return;
    ctx->current_return = return_type;
    ctx->current_function_effects = EFFECT_NONE;
    ctx->tracking_function_effects = true;
    ctx->in_async_func = prev_async || node->is_async_decl;

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = func_type->data.function.param_types[i];
        const char *param_name = node->data.func_decl.params[i]->name;
        if (type_is_subject_host_slot_handle(pt, ctx) && param_name != NULL) {
            char token_name[256];
            const char *paired_token = NULL;
            Symbol *slot_sym;

            if (pt->data.slot.is_secure) {
                snprintf(token_name, sizeof(token_name), "%s_token", param_name);
                paired_token = token_name;
            }

            slot_sym = symbol_create_slot(param_name, pt,
                pt->data.slot.is_secure, paired_token,
                node->line, node->column);
            scope_declare(ctx->scope, slot_sym);
            scope_register_slot(ctx->scope, slot_sym);

            if (pt->data.slot.is_secure) {
                Symbol *tok = symbol_create_token(token_name, param_name,
                    node->line, node->column);
                scope_declare(ctx->scope, tok);
            }
            continue;
        }

        Symbol *p = symbol_create_variable(
            param_name, pt,
            node->line, node->column);
        scope_declare(ctx->scope, p);
    }

    if (node->data.func_decl.body != NULL)
        type_check_block(node->data.func_decl.body, ctx);

    {
        uint32_t inferred_effects = ctx->current_function_effects;
        uint32_t missing_effects = inferred_effects & ~declared_effects;
        char inferred_buf[128];
        char missing_buf[128];
        char declared_buf[128];

        if (has_effect_contract && missing_effects != EFFECT_NONE) {
            effect_mask_to_string(inferred_effects, inferred_buf, sizeof(inferred_buf));
            effect_mask_to_string(missing_effects, missing_buf, sizeof(missing_buf));
            effect_mask_to_string(declared_effects, declared_buf, sizeof(declared_buf));
            semantic_error(ctx, node,
                "Function '%s' is missing declared effects: %s (declared: %s, inferred from body: %s)",
                name != NULL ? name : "<anonymous>",
                missing_buf, declared_buf, inferred_buf);
        }

        func_type->data.function.effect_mask = declared_effects | inferred_effects;
    }

    ctx->current_return = prev_return;
    ctx->current_function_effects = prev_effects;
    ctx->tracking_function_effects = prev_tracking;
    ctx->in_async_func = prev_async;
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.class_decl.name;
    bool passive_projection = node->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT
        || node->data.class_decl.nominal_kind == NOMINAL_DECL_DTO;

    /* If the class has generic parameters (<T, U, ...>), register them
     * as opaque types in a temporary scope so that resolve_type_node("T")
     * succeeds for field types and method signatures. */
    bool has_generics = (node->data.class_decl.generic_params != NULL
                         && node->data.class_decl.generic_params->count > 0);
    if (has_generics) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *class_type = calloc(1, sizeof(Type));
    if (class_type == NULL) {
        if (has_generics) scope_exit(&ctx->scope);
        return false;
    }
    class_type->kind = TYPE_KIND_CLASS;
    class_type->nominal_flavor = nominal_flavor_from_decl(node);
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    class_sym->kind = SYMBOL_CLASS;

    /* Declare in the outer scope (step out of temporary generic scope) */
    {
        Scope *target = has_generics ? ctx->scope->parent : ctx->scope;
        Scope *saved = ctx->scope;
        ctx->scope = target;
        if (!scope_declare(ctx->scope, class_sym)) {
            semantic_error(ctx, node, "Redeclaration of class '%s'", name);
            symbol_destroy(class_sym);
            ctx->scope = saved;
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
        ctx->scope = saved;
    }

    /* Close the temporary generic-params scope before entering the real
     * class scope — the class scope will re-register generic params so
     * they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    /* Check methods — type-check each in a temporary class scope,
     * then register the mangled name (ClassName_MethodName) in the
     * parent scope so that callers can find it. */
    scope_enter(&ctx->scope, SCOPE_CLASS);

    /* Re-register generic type params inside the class scope */
    if (has_generics) {
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *field = node->data.class_decl.fields[i];
        Type *field_type;
        Symbol *field_sym;

        if (field == NULL || field->name == NULL || field->type == NULL)
            continue;

        field_type = resolve_type_node(field->type, ctx);
        field_sym = symbol_create_variable(field->name, field_type,
            node->line, node->column);
        if (field_sym != NULL)
            scope_declare(ctx->scope, field_sym);
    }
    if (passive_projection && node->data.class_decl.method_count > 0) {
        semantic_error(ctx, node,
            "%s '%s' is a passive projection form and cannot declare methods; keep behavior on subjects/roles and project the result into object/dto fields",
            node->data.class_decl.nominal_kind == NOMINAL_DECL_DTO ? "dto" : "object",
            name != NULL ? name : "<anonymous>");
    } else {
        for (size_t i = 0; i < node->data.class_decl.method_count; i++)
            type_check_func_decl(node->data.class_decl.methods[i], ctx);
    }

    /* Collect method signatures before the class scope is destroyed */
    if (passive_projection) {
        scope_exit(&ctx->scope);
        return !ctx->has_error;
    }
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        const char *mname = method->data.func_decl.name;
        if (mname == NULL)
            continue;
        Symbol *msym = scope_lookup_current(ctx->scope, mname);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;

        /* Build mangled name: ClassName_MethodName */
        size_t len = strlen(name) + 1 + strlen(mname) + 1;
        char *mangled = malloc(len);
        if (mangled == NULL)
            continue;
        snprintf(mangled, len, "%s_%s", name, mname);

        /* The method's func_type already includes 'self' as the first
         * parameter (registered by type_check_func_decl), so reuse
         * the original signature directly. */
        Type *mangled_ft = msym->type;

        /* Register in parent scope (outside class) */
        Symbol *mangled_sym = symbol_create_function(
            mangled, mangled_ft, method->line, method->column);
        /* Temporarily step out to declare in parent */
        Scope *class_scope = ctx->scope;
        ctx->scope = class_scope->parent;
        if (!scope_declare(ctx->scope, mangled_sym))
            symbol_destroy(mangled_sym);
        ctx->scope = class_scope;
        free(mangled);
    }

    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_extern_block(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Program entry point
 * ----------------------------------------------------------------- */

bool
type_check_program(ASTNode *program, SemanticContext *ctx)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->program_root = program;

    /*
     * Pass 1: collect all top-level function and class names
     * so that forward references within the same file work.
     */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL) {
            const char *fname = stmt->data.func_decl.name;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = stmt->data.func_decl.param_count;
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = stmt->data.func_decl.params[j];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = TYPE_VOID;
                if (stmt->data.func_decl.return_type != NULL) {
                    /* Try to resolve return type; use TYPE_UNKNOWN on failure
                     * (e.g., generic return type 'T' not in scope yet). */
                    size_t saved_diag = ctx->diagnostic_count;
                    bool saved_err = ctx->has_error;
                    ret = resolve_type_node(stmt->data.func_decl.return_type, ctx);
                    if (ctx->diagnostic_count > saved_diag) {
                        /* Roll back the diagnostic — Pass 2 will re-check */
                        ctx->diagnostic_count = saved_diag;
                        ctx->has_error = saved_err;
                        ret = TYPE_UNKNOWN;
                    }
                }
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                if (placeholder != NULL)
                    placeholder->data.function.effect_mask =
                        declared_effects_from_function_node(stmt, ctx, NULL);
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = stmt->data.event_decl.name;
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = stmt->data.event_decl.param_count;
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = stmt->data.event_decl.params[j];
                    if (p->data.let_decl.type != NULL)
                        eptypes[j] = resolve_type_node(p->data.let_decl.type, ctx);
                    else
                        eptypes[j] = TYPE_INT;
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = stmt->data.enum_decl.name;
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_ENUM;
                    t->name = pergyra_strdup(ename);
                }
                Symbol *s = symbol_create_function(ename,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            Type *etype = enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN;
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                const char *vname = stmt->data.enum_decl.variants[j];
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                size_t vpc = (stmt->data.enum_decl.variant_param_counts != NULL)
                    ? stmt->data.enum_decl.variant_param_counts[j] : 0;
                if (vpc > 0) {
                    /* Tagged union variant constructor: register as function
                     * Circle(Int) → func Circle(Int) -> Shape */
                    Type **ptypes = calloc(vpc, sizeof(Type *));
                    for (size_t p = 0; p < vpc && ptypes != NULL; p++) {
                        ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                        ptypes[p] = resolve_type_node(pt, ctx);
                    }
                    Type *ft = type_create_function(ptypes, vpc, etype);
                    free(ptypes);
                    Symbol *vs = symbol_create_function(vname, ft,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                } else {
                    /* Simple variant: register as variable */
                    Symbol *vs = symbol_create_variable(vname, etype,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                }
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
                ASTNode *decl = stmt->data.extern_block.declarations[j];
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = decl->data.func_decl.name;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    if (placeholder != NULL)
                        placeholder->data.function.effect_mask =
                            declared_effects_from_function_node(decl, ctx, NULL);
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    scope_declare(ctx->scope, s);
                }
            }
        } else if (stmt->type == AST_ACTOR_DECL) {
            const char *aname = stmt->data.actor_decl.name;
            if (aname != NULL && scope_lookup_current(ctx->scope, aname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = TYPE_NOMINAL_SUBJECT;
                    t->name = pergyra_strdup(aname);
                }
                Symbol *s = symbol_create_function(aname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_ACTOR;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_PARTY_DECL
                   || stmt->type == AST_SYSTEMIC_DECL
                   || stmt->type == AST_WORLD_DECL
                   || stmt->type == AST_RELATION_DECL
                   || stmt->type == AST_EFFECT_DECL
                   || stmt->type == AST_ZONE_DECL) {
            /* Register domain declarations as class-like symbols
             * so that constructor-like syntax can be introduced consistently */
            const char *dname = NULL;
            if (stmt->type == AST_PARTY_DECL)
                dname = stmt->data.party_decl.name;
            else if (stmt->type == AST_SYSTEMIC_DECL)
                dname = stmt->data.systemic_decl.name;
            else if (stmt->type == AST_WORLD_DECL)
                dname = stmt->data.world_decl.name;
            else if (stmt->type == AST_RELATION_DECL)
                dname = stmt->data.relation_decl.name;
            else if (stmt->type == AST_EFFECT_DECL)
                dname = stmt->data.effect_decl.name;
            else
                dname = stmt->data.zone_decl.name;
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = TYPE_NOMINAL_CLASS;
                    t->name = pergyra_strdup(dname);
                }
                Symbol *s = symbol_create_function(dname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        }
    }

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < program->data.program.count; i++)
        type_check_statement(program->data.program.statements[i], ctx);

    return !ctx->has_error;
}
