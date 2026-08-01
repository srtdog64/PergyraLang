#include "mir_nominal_abi_layout.h"

#include "mir_abi_layout.h"

#include <limits.h>
#include <string.h>

static bool
mir_nominal_align_up(uint32_t value, uint32_t align, uint32_t *out)
{
    uint32_t padding;

    if (out == NULL || align == 0)
        return false;
    padding = (align - (value % align)) % align;
    if (value > UINT32_MAX - padding)
        return false;
    *out = value + padding;
    return true;
}

static const MIRDeclHeader *
mir_nominal_unique_struct(const MIRProgram *program, const char *name)
{
    const MIRDeclHeader *found = NULL;

    if (program == NULL || name == NULL || name[0] == '\0')
        return NULL;
    for (size_t i = 0; i < program->decl_header_count; i++) {
        const MIRDeclHeader *candidate = &program->decl_headers[i];
        if (candidate->ast_type != AST_CLASS_DECL
            || candidate->nominal_kind != NOMINAL_DECL_STRUCT
            || candidate->name == NULL
            || strcmp(candidate->name, name) != 0) {
            continue;
        }
        if (found != NULL)
            return NULL;
        found = candidate;
    }
    return found;
}

static bool
mir_nominal_field_size_align(const MIRProgram *program,
                             const char *type_name,
                             uint32_t *size_out,
                             uint32_t *align_out)
{
    const MIRDeclHeader *nested;

    if (type_name == NULL || size_out == NULL || align_out == NULL)
        return false;
    if (strcmp(type_name, "Int") == 0) {
        *size_out = 4;
        *align_out = 4;
        return true;
    }
    nested = mir_nominal_unique_struct(program, type_name);
    if (nested == NULL || !nested->abi_layout_present
        || nested->abi_layout_id == 0) {
        return false;
    }
    *size_out = nested->abi_layout.size_bytes;
    *align_out = nested->abi_layout.align_bytes;
    return *size_out > 0 && *align_out > 0;
}

static bool
mir_nominal_try_capture(MIRProgram *program, MIRDeclHeader *header)
{
    MIRTypeLayout layout;
    uint32_t cursor = 0;
    uint32_t max_align = 1;

    if (program == NULL || header == NULL
        || header->ast_type != AST_CLASS_DECL
        || header->nominal_kind != NOMINAL_DECL_STRUCT
        || header->name == NULL || header->name[0] == '\0'
        || header->field_count == 0
        || header->field_count > MIR_MAX_TYPE_FIELDS
        || header->field_metadata == NULL
        || header->field_metadata_count != header->field_count) {
        return false;
    }
    memset(&layout, 0, sizeof(layout));
    layout.abi_type_name = header->name;
    layout.field_count = (uint16_t)header->field_count;
    layout.representation = MIR_ABI_REPR_UNTAGGED;
    for (size_t i = 0; i < header->field_count; i++) {
        const MIRDeclField *field = &header->field_metadata[i];
        uint32_t field_size;
        uint32_t field_align;

        if (field->kind != MIR_DECL_FIELD_CLASS || field->name == NULL
            || field->name[0] == '\0' || field->type_name == NULL
            || !mir_nominal_field_size_align(
                program, field->type_name, &field_size, &field_align)
            || !mir_nominal_align_up(cursor, field_align, &cursor)
            || cursor > UINT32_MAX - field_size) {
            return false;
        }
        layout.fields[i].field_name = field->name;
        layout.fields[i].offset = cursor;
        layout.fields[i].field_size = field_size;
        layout.fields[i].field_align = field_align;
        cursor += field_size;
        if (field_align > max_align)
            max_align = field_align;
    }
    if (!mir_nominal_align_up(cursor, max_align, &layout.size_bytes))
        return false;
    layout.align_bytes = max_align;
    header->abi_layout = layout;
    header->abi_layout_id = mir_abi_layout_id(&header->abi_layout);
    header->abi_layout_present = header->abi_layout_id != 0;
    return header->abi_layout_present;
}

bool
mir_nominal_abi_layouts_capture(MIRProgram *program, char **error_message)
{
    size_t unresolved = 0;

    if (error_message != NULL)
        *error_message = NULL;
    if (program == NULL)
        return false;
    for (size_t i = 0; i < program->decl_header_count; i++) {
        MIRDeclHeader *header = &program->decl_headers[i];
        header->abi_layout_present = false;
        header->abi_layout_id = 0;
        memset(&header->abi_layout, 0, sizeof(header->abi_layout));
        if (header->ast_type == AST_CLASS_DECL
            && header->nominal_kind == NOMINAL_DECL_STRUCT) {
            unresolved++;
        }
    }
    for (size_t pass = 0; pass < program->decl_header_count && unresolved > 0;
         pass++) {
        size_t progress = 0;
        for (size_t i = 0; i < program->decl_header_count; i++) {
            MIRDeclHeader *header = &program->decl_headers[i];
            if (header->abi_layout_present
                || header->ast_type != AST_CLASS_DECL
                || header->nominal_kind != NOMINAL_DECL_STRUCT) {
                continue;
            }
            if (mir_nominal_try_capture(program, header)) {
                progress++;
                unresolved--;
            }
        }
        if (progress == 0)
            break;
    }
    return true;
}

const MIRTypeLayout *
mir_decl_header_abi_layout(const MIRDeclHeader *header)
{
    if (header == NULL || !header->abi_layout_present
        || header->abi_layout_id == 0
        || header->abi_layout_id != mir_abi_layout_id(&header->abi_layout)) {
        return NULL;
    }
    return &header->abi_layout;
}
