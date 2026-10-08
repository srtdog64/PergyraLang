#include "region_retention_summary.h"

#include "builtin_kind.h"
#include "../parser/ast.h"
#include "../parser/ast_api.h"
#include <string.h>

typedef struct PgyBuiltinArgumentRetentionRow {
    uint32_t builtin_kind;
    size_t argument_index;
    PgyRegionRetentionKind retention;
} PgyBuiltinArgumentRetentionRow;

#define PGY_RETENTION_ARGUMENT_ANY ((size_t)-1)
#define PGY_BUILTIN_ARGUMENT_RETENTION(identity, source_name, argument, kind) \
    { (uint32_t)BUILTIN_##identity, (argument), (kind) },
#define PGY_STDLIB_ARGUMENT_RETENTION(identity, source_name, argument, kind, arity)
static const PgyBuiltinArgumentRetentionRow builtin_argument_retention_rows[] = {
#include "builtin_argument_retention_registry.def"
};
#undef PGY_BUILTIN_ARGUMENT_RETENTION
#undef PGY_STDLIB_ARGUMENT_RETENTION
#undef PGY_RETENTION_ARGUMENT_ANY

typedef struct PgyStdlibArgumentRetentionRow {
    const char *name;
    size_t argument_index;
    PgyRegionRetentionKind retention;
    size_t arity;
} PgyStdlibArgumentRetentionRow;

#define PGY_RETENTION_ARGUMENT_ANY ((size_t)-1)
#define PGY_BUILTIN_ARGUMENT_RETENTION(identity, source_name, argument, kind)
#define PGY_STDLIB_ARGUMENT_RETENTION(identity, source_name, argument, kind, arity) \
    { (source_name), (argument), (kind), (arity) },
static const PgyStdlibArgumentRetentionRow stdlib_argument_retention_rows[] = {
#include "builtin_argument_retention_registry.def"
};
#undef PGY_STDLIB_ARGUMENT_RETENTION
#undef PGY_BUILTIN_ARGUMENT_RETENTION
#undef PGY_RETENTION_ARGUMENT_ANY

bool
semantic_region_retention_summary_for_stdlib(
    const ASTNode *call, size_t argument_index, PgyRegionRetentionKind *kind_out)
{
    uint32_t builtin_kind;
    const ASTNode *callee;
    const char *name;

    if (kind_out != NULL) *kind_out = PGY_REGION_RETENTION_UNKNOWN;
    if (call == NULL || call->type != AST_CALL ||
        !ast_call_semantic_callee_is_stdlib(call) ||
        ast_call_semantic_callee_decl_id(call) != 0 ||
        ast_call_semantic_callee_value_binding_id(call) != 0 ||
        !ast_call_semantic_callee_builtin_kind(call, &builtin_kind) ||
        builtin_kind != (uint32_t)BUILTIN_NOT_BUILTIN) return false;
    callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_IDENTIFIER) return false;
    name = ast_identifier_name(callee);
    if (name == NULL) return false;
    for (size_t row = 0; row < sizeof(stdlib_argument_retention_rows) /
             sizeof(stdlib_argument_retention_rows[0]); ++row) {
        const PgyStdlibArgumentRetentionRow *entry = &stdlib_argument_retention_rows[row];
        if (strcmp(entry->name, name) != 0 || ast_call_arg_count(call) != entry->arity ||
            argument_index >= entry->arity ||
            (entry->argument_index != (size_t)-1 && entry->argument_index != argument_index)) continue;
        if (kind_out != NULL) *kind_out = entry->retention;
        return true;
    }
    return false;
}

bool
semantic_region_retention_summary_for_builtin(
    uint32_t builtin_kind,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out)
{
    size_t row;

    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_UNKNOWN;
    for (row = 0;
         row < sizeof(builtin_argument_retention_rows)
                 / sizeof(builtin_argument_retention_rows[0]);
         row++) {
        const PgyBuiltinArgumentRetentionRow *candidate =
            &builtin_argument_retention_rows[row];
        if (candidate->builtin_kind != builtin_kind ||
            (candidate->argument_index != (size_t)-1 &&
             candidate->argument_index != argument_index))
            continue;
        if (kind_out != NULL)
            *kind_out = candidate->retention;
        return true;
    }
    return false;
}
