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

#endif /* PERGYRA_CODEGEN_SLOT_TYPE_POLICY_H */
