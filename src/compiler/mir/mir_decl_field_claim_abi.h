#ifndef PERGYRA_MIR_DECL_FIELD_CLAIM_ABI_H
#define PERGYRA_MIR_DECL_FIELD_CLAIM_ABI_H

#include "mir_decl.h"

bool mir_decl_field_claim_abi_capture(MIRDeclFieldClaim *claim);
bool mir_decl_field_claim_abi_validate(const MIRDeclFieldClaim *claim);
void mir_decl_field_claim_abi_clear(MIRDeclFieldClaim *claim);
bool mir_decl_header_field_claim_abi_validate(
    const MIRDeclHeader *header,
    size_t header_index,
    char **error_message);

#endif /* PERGYRA_MIR_DECL_FIELD_CLAIM_ABI_H */
