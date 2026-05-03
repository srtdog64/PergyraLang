#ifndef PERGYRA_CODEGEN_SLOT_TYPE_POLICY_H
#define PERGYRA_CODEGEN_SLOT_TYPE_POLICY_H

#include <stdbool.h>

bool pgy_codegen_type_name_is_slot(const char *type_name);
bool pgy_codegen_type_name_is_secure_slot(const char *type_name);
bool pgy_codegen_type_name_is_device_slot(const char *type_name);
bool pgy_codegen_type_name_is_read_view(const char *type_name);
bool pgy_codegen_type_name_is_write_view(const char *type_name);
bool pgy_codegen_type_name_is_view(const char *type_name);
bool pgy_codegen_type_name_is_slot_or_view(const char *type_name);
bool pgy_codegen_type_name_is_slot_family(const char *type_name);

bool pgy_codegen_call_name_is_view_read(const char *name);
bool pgy_codegen_call_name_is_view_write(const char *name);
bool pgy_codegen_call_name_is_view_constructor(const char *name);
bool pgy_codegen_call_name_is_read(const char *name);
bool pgy_codegen_call_name_is_write(const char *name);
bool pgy_codegen_call_name_is_release(const char *name);
bool pgy_codegen_call_name_is_slot_operation(const char *name);
bool pgy_codegen_call_name_is_move(const char *name);
bool pgy_codegen_call_name_is_slot_source(const char *name);
bool pgy_codegen_call_name_is_claim_slot(const char *name);
bool pgy_codegen_call_name_is_claim_secure_slot(const char *name);
bool pgy_codegen_call_name_is_claim_device_slot(const char *name);
bool pgy_codegen_call_name_is_slot_claim(const char *name);
const char *pgy_codegen_claim_slot_abi_prefix(const char *name);

#endif /* PERGYRA_CODEGEN_SLOT_TYPE_POLICY_H */
