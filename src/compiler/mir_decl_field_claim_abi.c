#include "mir_decl_field_claim_abi.h"

#include "mir_abi_layout.h"
#include "mir_fact_validate_internal.h"

#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static void
mir_decl_field_claim_runtime_row_clear(MIRResourceRuntimeRow *row)
{
    if (row == NULL)
        return;
    free((void *)row->domain);
    free((void *)row->abi_type_name);
    free((void *)row->resource_op_name);
    free((void *)row->runtime_fn);
    free((void *)row->target_kind);
    free((void *)row->materialization);
    free((void *)row->call_shape);
    memset(row, 0, sizeof(*row));
}

void
mir_decl_field_claim_abi_clear(MIRDeclFieldClaim *claim)
{
    if (claim == NULL)
        return;
    mir_decl_field_claim_runtime_row_clear(&claim->runtime_call_abi);
    claim->type_layout = NULL;
    claim->abi_layout_id = 0;
    claim->runtime_call_abi_present = false;
}

static bool
mir_decl_field_claim_runtime_row_copy(MIRResourceRuntimeRow *dst,
                                      const MIRResourceRuntimeRow *src)
{
    if (dst == NULL || src == NULL)
        return false;
    *dst = *src;
    dst->domain = pergyra_strdup(src->domain);
    dst->abi_type_name = pergyra_strdup(src->abi_type_name);
    dst->resource_op_name = pergyra_strdup(src->resource_op_name);
    dst->runtime_fn = pergyra_strdup(src->runtime_fn);
    dst->target_kind = pergyra_strdup(src->target_kind);
    dst->materialization = pergyra_strdup(src->materialization);
    dst->call_shape = pergyra_strdup(src->call_shape);
    dst->runtime_call_abi_id = mir_abi_resource_runtime_row_id(dst);
    if (dst->domain == NULL || dst->abi_type_name == NULL
        || dst->resource_op_name == NULL || dst->runtime_fn == NULL
        || dst->target_kind == NULL || dst->materialization == NULL
        || dst->call_shape == NULL || dst->runtime_call_abi_id == 0) {
        mir_decl_field_claim_runtime_row_clear(dst);
        return false;
    }
    return true;
}

bool
mir_decl_field_claim_abi_capture(MIRDeclFieldClaim *claim)
{
    const MIRResourceRuntimeRow *row;

    if (claim == NULL || claim->inner_type_name == NULL)
        return false;
    row = mir_abi_resource_runtime_row_by_kind(
        claim->is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                         : MIR_RESOURCE_ABI_SLOT,
        claim->inner_type_name,
        "Claim");
    if (row == NULL
        || !mir_decl_field_claim_runtime_row_copy(
            &claim->runtime_call_abi, row)) {
        return false;
    }
    claim->type_layout = mir_abi_lookup(
        claim->runtime_call_abi.abi_type_name);
    claim->abi_layout_id = mir_abi_layout_id(claim->type_layout);
    if ((claim->type_layout == NULL || claim->abi_layout_id == 0)
        && !mir_abi_resource_runtime_row_is_constructed_nominal(
            &claim->runtime_call_abi)) {
        mir_decl_field_claim_abi_clear(claim);
        return false;
    }
    claim->runtime_call_abi_present = true;
    return true;
}

bool
mir_decl_field_claim_abi_validate(const MIRDeclFieldClaim *claim)
{
    const MIRResourceRuntimeRow *row;
    const char *expected_shape;
    bool constructed;

    if (claim == NULL || !claim->runtime_call_abi_present)
        return false;
    row = &claim->runtime_call_abi;
    expected_shape = claim->is_secure
        ? "token_ptr_to_container"
        : "returns_container";
    if (row->resource_op_name == NULL
        || strcmp(row->resource_op_name, "Claim") != 0
        || row->call_shape == NULL
        || strcmp(row->call_shape, expected_shape) != 0
        || row->runtime_call_abi_id == 0
        || row->runtime_call_abi_id
            != mir_abi_resource_runtime_row_id(row)
        || !mir_abi_resource_runtime_row_matches_owner(row)) {
        return false;
    }
    constructed = mir_abi_resource_runtime_row_is_constructed_nominal(row);
    if (constructed)
        return claim->type_layout == NULL && claim->abi_layout_id == 0;
    return claim->type_layout != NULL
        && claim->abi_layout_id != 0
        && claim->abi_layout_id == mir_abi_layout_id(claim->type_layout)
        && claim->type_layout->abi_type_name != NULL
        && row->abi_type_name != NULL
        && strcmp(claim->type_layout->abi_type_name, row->abi_type_name) == 0;
}

bool
mir_decl_header_field_claim_abi_validate(const MIRDeclHeader *header,
                                         size_t header_index,
                                         char **error_message)
{
    if (header == NULL)
        return false;
    if (header->field_claim_metadata_count != header->field_claim_count
        || (header->field_claim_metadata_count > 0
            && header->field_claim_metadata == NULL)) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] field-claim inventory is incomplete",
                header_index);
        }
        return false;
    }
    for (size_t i = 0; i < header->field_claim_metadata_count; i++) {
        const MIRDeclFieldClaim *claim = &header->field_claim_metadata[i];
        if (claim->owner_name == NULL || header->name == NULL
            || strcmp(claim->owner_name, header->name) != 0
            || claim->slot_name == NULL || claim->inner_type_name == NULL
            || (claim->is_secure && claim->token_name == NULL)
            || !mir_decl_field_claim_abi_validate(claim)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] field-claim[%zu] has invalid runtime-call ABI ownership",
                    header_index, i);
            }
            return false;
        }
    }
    return true;
}
