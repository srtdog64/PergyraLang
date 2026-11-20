/* String loans across loop iterations: the loan a for-in binding of String
 * elements carries, and the loans a loop body retires, which the loop owners
 * read to check the body once more. The loans themselves, their storage
 * sources and their retirement belong to indexed_string_borrow_owner.c. */
#include "indexed_string_borrow_owner.h"
#include "type_checker/type_checker_internal.h"

void
semantic_indexed_string_borrow_bind_iteration(
    Symbol *binding, ASTNode *iterable, SemanticContext *ctx)
{
    if (binding == NULL || !type_equals(binding->type, TYPE_STRING))
        return;
    bool tracked = false;
    uint32_t source = 0;
    bool known = semantic_indexed_string_borrow_array_receiver_source(
        iterable, ctx, &tracked, &source);
    binding->indexed_string_borrow_source_syntax_id = known && tracked && source != 0
        ? source : INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    binding->indexed_string_borrow_invalidated = false;
}

size_t
semantic_indexed_string_borrow_retired_loans(const Scope *scope, size_t *visible)
{
    size_t retired = 0, strings = 0;
    for (const Scope *s = scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent)
        for (size_t i = 0; i < s->symbol_count; i++) {
            const Symbol *candidate = s->symbols[i];
            if (candidate == NULL || !type_equals(candidate->type, TYPE_STRING))
                continue;
            strings++;
            if (candidate->indexed_string_borrow_invalidated)
                retired++;
        }
    if (visible != NULL)
        *visible = strings;
    return retired;
}
