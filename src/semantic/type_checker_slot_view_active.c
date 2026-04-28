/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Active Slot view discovery and owner-escape diagnostics.
 */

#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
semantic_find_active_slot_view(Scope *scope,
                               const char **view_name_out,
                               const char **view_kind_out,
                               const char **source_slot_out)
{
    if (view_name_out != NULL)
        *view_name_out = NULL;
    if (view_kind_out != NULL)
        *view_kind_out = NULL;
    if (source_slot_out != NULL)
        *source_slot_out = NULL;

    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *sym = cur->symbols[i];
            bool is_read;
            bool is_write;

            if (sym == NULL || sym->type == NULL)
                continue;
            is_read = type_is_read_view(sym->type);
            is_write = type_is_write_view(sym->type);
            if (!is_read && !is_write)
                continue;

            if (view_name_out != NULL)
                *view_name_out = sym->name;
            if (view_kind_out != NULL)
                *view_kind_out = is_write ? "WriteView" : "ReadView";
            if (source_slot_out != NULL)
                *source_slot_out = sym->slot_info.paired_slot_name;
            return true;
        }
    }

    return false;
}

bool
semantic_find_active_slot_view_for_source(Scope *scope,
                                          const char *source_slot,
                                          const char **view_name_out,
                                          const char **view_kind_out,
                                          bool *is_write_view_out)
{
    if (view_name_out != NULL)
        *view_name_out = NULL;
    if (view_kind_out != NULL)
        *view_kind_out = NULL;
    if (is_write_view_out != NULL)
        *is_write_view_out = false;
    if (source_slot == NULL)
        return false;

    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *sym = cur->symbols[i];
            bool is_read;
            bool is_write;

            if (sym == NULL || sym->type == NULL
                || sym->slot_info.paired_slot_name == NULL
                || strcmp(sym->slot_info.paired_slot_name, source_slot) != 0) {
                continue;
            }

            is_read = type_is_read_view(sym->type);
            is_write = type_is_write_view(sym->type);
            if (!is_read && !is_write)
                continue;

            if (view_name_out != NULL)
                *view_name_out = sym->name;
            if (view_kind_out != NULL)
                *view_kind_out = is_write ? "WriteView" : "ReadView";
            if (is_write_view_out != NULL)
                *is_write_view_out = is_write;
            return true;
        }
    }

    return false;
}

bool
semantic_reject_active_slot_owner_escape(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *escape_kind,
                                         const char *escape_name)
{
    const char *slot_name;
    Symbol *sym;
    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;

    if (site == NULL || ctx == NULL || site->type != AST_IDENTIFIER
        || site->data.identifier.name == NULL) {
        return false;
    }

    slot_name = site->data.identifier.name;
    sym = scope_lookup(ctx->scope, slot_name);
    if (sym == NULL || sym->kind != SYMBOL_SLOT || sym->type == NULL
        || !type_is_owned_slot_handle(sym->type)) {
        return false;
    }

    if (!semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
            &active_view_name, &active_view_kind, NULL)) {
        return false;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
        PGY_CAUSE_PIN_PARALLEL_CONFLICT,
        PGY_FIX_SERIALIZE_PIN_ACCESS,
        site,
        "Cannot store or forward slot '%s' through %s '%s' while %s '%s' is live.\n"
        "Reason:\n"
        "- pinned views are scoped capability leases over the source slot\n"
        "- escaping the owner handle would let code outside the view scope read, write, release, or forward the source\n"
        "Fix:\n"
        "- store a copied value read through the active view when that is intended\n"
        "- or end the pin/view scope before passing '%s' through %s",
        sym->name,
        escape_kind != NULL ? escape_kind : "boundary",
        escape_name != NULL ? escape_name : "<unknown>",
        active_view_kind != NULL ? active_view_kind : "view",
        active_view_name != NULL ? active_view_name : "<view>",
        sym->name,
        escape_kind != NULL ? escape_kind : "that boundary");
    return true;
}
