#include "mir_nominal_abi_layout.h"

#include "mir_abi_layout.h"
#include "mir_decl_headers.h"
#include "../common/string_compat.h"

#include <limits.h>
#include <stdlib.h>
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

static bool
mir_nominal_option_template_ready(const MIRTypeLayout *layout)
{
    return layout != NULL && layout->field_count == 2
        && layout->fields[0].field_name != NULL
        && layout->fields[1].field_name != NULL
        && layout->fields[0].offset == 0
        && layout->fields[0].field_size > 0
        && layout->fields[0].field_align > 0
        && layout->representation == MIR_ABI_REPR_EXPLICIT_TAG
        && layout->discriminant_field_name != NULL
        && strcmp(layout->fields[0].field_name,
                  layout->discriminant_field_name) == 0;
}

static bool
mir_nominal_option_type_matches_header(const MIRDeclHeader *header,
                                       const char *type_name)
{
    size_t name_length;
    size_t type_length;

    if (header == NULL || header->name == NULL || type_name == NULL)
        return false;
    name_length = strlen(header->name);
    type_length = strlen(type_name);
    return type_length == name_length + 8
        && memcmp(type_name, "Option<", 7) == 0
        && memcmp(type_name + 7, header->name, name_length) == 0
        && type_name[7 + name_length] == '>';
}

static bool
mir_nominal_option_try_capture(MIRDeclHeader *header)
{
    MIRTypeLayout layout;
    const MIRTypeLayout *template;
    const MIRTypeLayout *payload;
    char *type_name;
    size_t name_length;
    uint32_t tag_end;
    uint32_t value_offset;
    uint32_t value_end;
    uint32_t wrapper_align;

    if (header == NULL || header->name == NULL
        || !header->abi_layout_present || header->abi_layout_id == 0)
        return false;
    template = mir_abi_lookup("Option<Int>");
    if (!mir_nominal_option_template_ready(template))
        return false;
    payload = &header->abi_layout;
    name_length = strlen(header->name);
    if (name_length > SIZE_MAX - 9)
        return false;
    type_name = malloc(name_length + 9);
    if (type_name == NULL)
        return false;
    memcpy(type_name, "Option<", 7);
    memcpy(type_name + 7, header->name, name_length);
    type_name[7 + name_length] = '>';
    type_name[8 + name_length] = '\0';

    if (template->fields[0].offset >
            UINT32_MAX - template->fields[0].field_size) {
        free(type_name);
        return false;
    }
    tag_end = template->fields[0].offset
        + template->fields[0].field_size;
    wrapper_align = payload->align_bytes > template->fields[0].field_align
        ? payload->align_bytes : template->fields[0].field_align;
    if (!mir_nominal_align_up(tag_end, payload->align_bytes, &value_offset)
        || value_offset > UINT32_MAX - payload->size_bytes) {
        free(type_name);
        return false;
    }
    value_end = value_offset + payload->size_bytes;
    memset(&layout, 0, sizeof(layout));
    layout.abi_type_name = type_name;
    layout.size_bytes = value_end;
    if (!mir_nominal_align_up(
            layout.size_bytes, wrapper_align, &layout.size_bytes)) {
        free(type_name);
        return false;
    }
    layout.align_bytes = wrapper_align;
    layout.field_count = 2;
    layout.fields[0] = template->fields[0];
    layout.fields[1].field_name = template->fields[1].field_name;
    layout.fields[1].offset = value_offset;
    layout.fields[1].field_size = payload->size_bytes;
    layout.fields[1].field_align = payload->align_bytes;
    layout.representation = template->representation;
    layout.discriminant_field_name = template->discriminant_field_name;
    layout.primary_tag_value = template->primary_tag_value;
    layout.secondary_tag_value = template->secondary_tag_value;
    layout.niche_none_pattern = template->niche_none_pattern;
    header->option_abi_layout_id = mir_abi_layout_id(&layout);
    if (header->option_abi_layout_id == 0) {
        free(type_name);
        return false;
    }
    header->option_abi_type_name = type_name;
    header->option_abi_layout = layout;
    header->option_abi_layout.abi_type_name = header->option_abi_type_name;
    header->option_abi_layout_present = true;
    return true;
}

bool
mir_nominal_abi_layouts_capture(MIRProgram *program, char **error_message)
{
    size_t unresolved = 0;

    if (error_message != NULL)
        *error_message = NULL;
    if (program == NULL)
        return false;
    if (!MIR_DECL_HEADER_STORAGE_LAYOUT_MATCHES_LOCAL()) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR declaration header storage layout mismatch");
        return false;
    }
    for (size_t i = 0; i < program->decl_header_count; i++) {
        MIRDeclHeader *header = &program->decl_headers[i];
        free(header->option_abi_type_name);
        header->option_abi_type_name = NULL;
        header->option_abi_layout_present = false;
        header->option_abi_layout_id = 0;
        memset(&header->option_abi_layout, 0,
               sizeof(header->option_abi_layout));
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
    for (size_t i = 0; i < program->decl_header_count; i++) {
        MIRDeclHeader *header = &program->decl_headers[i];
        if (header->abi_layout_present
            && !mir_nominal_option_try_capture(header)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "failed to capture nominal Option ABI layout receipt");
            return false;
        }
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

const MIRTypeLayout *
mir_decl_header_option_abi_layout(const MIRDeclHeader *header)
{
    const MIRTypeLayout *template = mir_abi_lookup("Option<Int>");
    const MIRTypeLayout *payload;
    const MIRTypeLayout *layout;
    uint32_t expected_align;
    uint32_t expected_offset;
    uint32_t expected_size;
    uint32_t tag_end;

    if (header == NULL || !header->option_abi_layout_present
        || header->option_abi_type_name == NULL
        || !mir_nominal_option_template_ready(template)
        || !mir_nominal_option_type_matches_header(
            header, header->option_abi_type_name)) {
        return NULL;
    }
    payload = mir_decl_header_abi_layout(header);
    layout = &header->option_abi_layout;
    if (payload == NULL || layout->abi_type_name == NULL
        || strcmp(layout->abi_type_name,
                  header->option_abi_type_name) != 0
        || template->fields[0].offset >
            UINT32_MAX - template->fields[0].field_size) {
        return NULL;
    }
    tag_end = template->fields[0].offset
        + template->fields[0].field_size;
    expected_align = payload->align_bytes > template->fields[0].field_align
        ? payload->align_bytes : template->fields[0].field_align;
    if (!mir_nominal_align_up(
            tag_end, payload->align_bytes, &expected_offset)
        || expected_offset > UINT32_MAX - payload->size_bytes
        || !mir_nominal_align_up(expected_offset + payload->size_bytes,
                                 expected_align, &expected_size)) {
        return NULL;
    }
    if (layout->size_bytes != expected_size
        || layout->align_bytes != expected_align
        || layout->field_count != 2
        || layout->fields[0].field_name == NULL
        || strcmp(layout->fields[0].field_name,
                  template->fields[0].field_name) != 0
        || layout->fields[0].offset != template->fields[0].offset
        || layout->fields[0].field_size != template->fields[0].field_size
        || layout->fields[0].field_align != template->fields[0].field_align
        || layout->fields[1].field_name == NULL
        || strcmp(layout->fields[1].field_name,
                  template->fields[1].field_name) != 0
        || layout->fields[1].offset != expected_offset
        || layout->fields[1].field_size != payload->size_bytes
        || layout->fields[1].field_align != payload->align_bytes
        || layout->runtime_fn != NULL || layout->inner_c_type != NULL
        || layout->representation != template->representation
        || layout->discriminant_field_name == NULL
        || strcmp(layout->discriminant_field_name,
                  template->discriminant_field_name) != 0
        || layout->primary_tag_value != template->primary_tag_value
        || layout->secondary_tag_value != template->secondary_tag_value
        || layout->niche_none_pattern != template->niche_none_pattern
        || header->option_abi_layout_id == 0
        || header->option_abi_layout_id != mir_abi_layout_id(layout)) {
        return NULL;
    }
    return layout;
}

const MIRTypeLayout *
mir_program_abi_layout_for_type_name(const MIRProgram *program,
                                     const char *type_name)
{
    const MIRTypeLayout *layout;
    const MIRTypeLayout *found = NULL;

    if (type_name == NULL || type_name[0] == '\0')
        return NULL;
    layout = mir_abi_lookup(type_name);
    if (layout != NULL)
        return layout;
    layout = mir_decl_header_abi_layout(
        mir_nominal_unique_struct(program, type_name));
    if (layout != NULL)
        return layout;
    if (program == NULL)
        return NULL;
    for (size_t i = 0; i < program->decl_header_count; i++) {
        const MIRDeclHeader *header = &program->decl_headers[i];
        const MIRTypeLayout *option_layout;

        if (header->option_abi_type_name == NULL
            || strcmp(header->option_abi_type_name, type_name) != 0)
            continue;
        option_layout = mir_decl_header_option_abi_layout(header);
        if (option_layout == NULL || found != NULL)
            return NULL;
        found = option_layout;
    }
    return found;
}
